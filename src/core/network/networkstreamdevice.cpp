/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
 *
 * Fooyin is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Fooyin is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Fooyin.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "networkstreamdevice.h"

#include "streamdeviceutils.h"

#include <core/network/networkutils.h>

#include <QLoggingCategory>
#include <QMetaObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QThread>

#include <algorithm>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <utility>

using namespace Qt::StringLiterals;

Q_LOGGING_CATEGORY(NETWORK_STREAM, "fy.networkstream")

constexpr auto MaxCleanFinishReconnectAttempts = 5;

namespace Fooyin {
struct NetworkStreamReconnectState
{
    bool reconnectOnFinished{false};
    int cleanFinishReconnectAttempts{0};

    void reset()
    {
        reconnectOnFinished          = false;
        cleanFinishReconnectAttempts = 0;
    }
};

struct IcyMetadataState
{
    NetworkStreamMetadata metadata;
    int metaInterval{0};
    int bytesUntilMetadata{0};
    int metadataBytesRemaining{-1};
    QByteArray metadataBlock;

    void reset()
    {
        metadata               = {};
        metaInterval           = 0;
        bytesUntilMetadata     = 0;
        metadataBytesRemaining = -1;
        metadataBlock.clear();
    }
};

struct NetworkStreamDeviceState
{
    QNetworkReply* reply{nullptr};
    std::mutex mutex;
    std::condition_variable ready;
    QString error;
    bool finished{false};
    bool aborted{false};
    StreamUtils::BufferState streamBuffer;
    StreamUtils::ReadModeState readMode;
    NetworkStreamReconnectState reconnect;
    IcyMetadataState icy;

    void resetForOpen()
    {
        error.clear();
        finished = false;
        aborted  = false;
        streamBuffer.reset();
        readMode.reset();
        reconnect.reset();
        icy.reset();
        reply = nullptr;
    }
};

namespace {
void startNetworkStreamRequest(const std::shared_ptr<QNetworkAccessManager>& network, const QUrl& url,
                               const std::shared_ptr<NetworkStreamDeviceState>& state);

QString decodeIcyText(QByteArray value)
{
    value = value.trimmed();
    return value.isEmpty() ? QString{} : QString::fromUtf8(value).trimmed();
}

QString icyFieldValue(const QString& metadata, const QString& key)
{
    const QString prefix  = key + u"='"_s;
    const qsizetype start = metadata.indexOf(prefix, 0, Qt::CaseInsensitive);
    if(start < 0) {
        return {};
    }

    const qsizetype valueStart = start + prefix.size();
    qsizetype valueEnd         = valueStart;
    while(valueEnd < metadata.size()) {
        if(metadata.at(valueEnd) == '\''_L1 && (valueEnd == valueStart || metadata.at(valueEnd - 1) != '\\'_L1)) {
            break;
        }
        ++valueEnd;
    }

    QString value = metadata.mid(valueStart, valueEnd - valueStart);
    value.replace("\\'"_L1, "'"_L1);
    return value.trimmed();
}

bool updateMetadataRevision(NetworkStreamDeviceState& state, NetworkStreamMetadata metadata)
{
    const bool changed
        = state.icy.metadata.streamName != metadata.streamName || state.icy.metadata.streamGenre != metadata.streamGenre
       || state.icy.metadata.streamTitle != metadata.streamTitle || state.icy.metadata.streamUrl != metadata.streamUrl
       || state.icy.metadata.bitrateKbps != metadata.bitrateKbps;
    if(!changed) {
        return false;
    }

    metadata.revision  = state.icy.metadata.revision + 1;
    state.icy.metadata = std::move(metadata);
    return true;
}

void updateReplyMetadata(NetworkStreamDeviceState& state, QNetworkReply* reply)
{
    if(reply != state.reply || state.aborted) {
        return;
    }

    NetworkStreamMetadata metadata{state.icy.metadata};
    metadata.streamName  = decodeIcyText(reply->rawHeader("icy-name"));
    metadata.streamGenre = decodeIcyText(reply->rawHeader("icy-genre"));

    bool bitrateOk{false};
    const int bitrateKbps = decodeIcyText(reply->rawHeader("icy-br")).toInt(&bitrateOk);
    if(bitrateOk && bitrateKbps > 0) {
        metadata.bitrateKbps = bitrateKbps;
    }

    bool metaIntervalOk{false};
    const int metaInterval = decodeIcyText(reply->rawHeader("icy-metaint")).toInt(&metaIntervalOk);
    if(metaIntervalOk && metaInterval > 0 && state.icy.metaInterval != metaInterval) {
        state.icy.metaInterval           = metaInterval;
        state.icy.bytesUntilMetadata     = metaInterval;
        state.icy.metadataBytesRemaining = -1;
        state.icy.metadataBlock.clear();
    }

    updateMetadataRevision(state, metadata);
}

void parseIcyMetadataBlock(NetworkStreamDeviceState& state)
{
    QString metadataText = QString::fromUtf8(state.icy.metadataBlock).trimmed();
    metadataText.remove(QChar::Null);
    if(metadataText.isEmpty()) {
        return;
    }

    NetworkStreamMetadata metadata{state.icy.metadata};
    const QString previousTitle = metadata.streamTitle;
    const QString title         = icyFieldValue(metadataText, u"StreamTitle"_s);
    const QString url           = icyFieldValue(metadataText, u"StreamUrl"_s);

    if(!title.isEmpty()) {
        metadata.streamTitle = title;
    }
    if(!url.isEmpty()) {
        metadata.streamUrl = url;
    }

    updateMetadataRevision(state, metadata);
}

void appendAudioBytes(NetworkStreamDeviceState& state, QByteArrayView data)
{
    state.streamBuffer.bytesReceived += static_cast<quint64>(data.size());
    state.streamBuffer.data.append(data.data(), data.size());
}

void appendReplyData(NetworkStreamDeviceState& state, QNetworkReply* reply)
{
    if(reply != state.reply || state.aborted) {
        return;
    }

    const size_t availableBytes = state.streamBuffer.maxBytes - state.streamBuffer.data.size();
    if(availableBytes <= 0) {
        return;
    }

    qsizetype offset{0};
    const QByteArray data = reply->read(static_cast<qint64>(availableBytes));

    while(std::cmp_less(offset, data.size())) {
        if(state.icy.metaInterval <= 0) {
            appendAudioBytes(state, QByteArrayView{data}.sliced(offset));
            return;
        }

        if(state.icy.bytesUntilMetadata > 0) {
            const qsizetype audioBytes = std::min<qsizetype>(state.icy.bytesUntilMetadata, data.size() - offset);
            appendAudioBytes(state, QByteArrayView{data}.sliced(offset, audioBytes));
            state.icy.bytesUntilMetadata -= static_cast<int>(audioBytes);
            offset += audioBytes;
            continue;
        }

        static constexpr int IcyMetadataBlockSize = 16;

        if(state.icy.metadataBytesRemaining < 0) {
            const int metadataLength         = static_cast<unsigned char>(data.at(offset)) * IcyMetadataBlockSize;
            state.icy.metadataBytesRemaining = metadataLength;
            state.icy.metadataBlock.clear();
            ++offset;

            if(metadataLength == 0) {
                state.icy.metadataBytesRemaining = -1;
                state.icy.bytesUntilMetadata     = state.icy.metaInterval;
            }
            continue;
        }

        const qsizetype metadataBytes = std::min<qsizetype>(state.icy.metadataBytesRemaining, data.size() - offset);
        state.icy.metadataBlock.append(data.constData() + offset, metadataBytes);
        state.icy.metadataBytesRemaining -= static_cast<int>(metadataBytes);
        offset += metadataBytes;

        if(state.icy.metadataBytesRemaining == 0) {
            parseIcyMetadataBlock(state);
            state.icy.metadataBytesRemaining = -1;
            state.icy.bytesUntilMetadata     = state.icy.metaInterval;
            state.icy.metadataBlock.clear();
        }
    }
}

void startNetworkStreamRequestOnNetworkThread(const std::shared_ptr<QNetworkAccessManager>& network, const QUrl& url,
                                              const std::shared_ptr<NetworkStreamDeviceState>& state)
{
    const QNetworkRequest req = makeNetworkRequest(
        url, NetworkRequestOption::AlwaysNetwork | NetworkRequestOption::AcceptAny | NetworkRequestOption::IcyMetadata);

    auto* reply = network->get(req);
    reply->setReadBufferSize(static_cast<qint64>(state->streamBuffer.maxBytes));

    {
        const std::scoped_lock lock{state->mutex};
        if(state->aborted || state->finished || state->reply) {
            reply->abort();
            reply->deleteLater();
            return;
        }

        state->reply                                  = reply;
        state->readMode.readWouldBlock                = false;
        state->readMode.readWouldBlockCount           = 0;
        state->streamBuffer.requestStartBytesReceived = state->streamBuffer.bytesReceived;
    }

    // Reply signals may arrive after the device is closed from the decode thread
    // Capture only shared state here; never capture the device object
    QObject::connect(reply, &QNetworkReply::metaDataChanged, reply, [state, reply]() {
        {
            const std::scoped_lock lock{state->mutex};
            updateReplyMetadata(*state, reply);
        }
        state->ready.notify_all();
    });
    QObject::connect(reply, &QIODevice::readyRead, reply, [state, reply]() {
        {
            const std::scoped_lock lock{state->mutex};
            updateReplyMetadata(*state, reply);
            appendReplyData(*state, reply);
        }
        state->ready.notify_all();
    });
    QObject::connect(reply, &QNetworkReply::finished, reply, [network, url, state, reply]() {
        bool reconnect{false};
        {
            const std::scoped_lock lock{state->mutex};
            if(reply == state->reply) {
                appendReplyData(*state, reply);

                const bool receivedBytes
                    = state->streamBuffer.bytesReceived > state->streamBuffer.requestStartBytesReceived;
                if(reply->error() != QNetworkReply::NoError && state->error.isEmpty() && !state->aborted) {
                    state->error = reply->errorString();
                }

                if(receivedBytes) {
                    state->reconnect.cleanFinishReconnectAttempts = 0;
                }
                else if(reply->error() == QNetworkReply::NoError && state->reconnect.reconnectOnFinished
                        && !state->aborted && state->error.isEmpty()) {
                    ++state->reconnect.cleanFinishReconnectAttempts;
                }

                reconnect = reply->error() == QNetworkReply::NoError && state->reconnect.reconnectOnFinished
                         && !state->aborted && state->error.isEmpty()
                         && (receivedBytes
                             || state->reconnect.cleanFinishReconnectAttempts <= MaxCleanFinishReconnectAttempts);
                if(reconnect) {
                    state->reply                   = nullptr;
                    state->readMode.readWouldBlock = state->readMode.nonBlockingReadsEnabled;
                }
                else {
                    state->finished = true;
                }
            }
        }
        state->ready.notify_all();
        reply->deleteLater();

        if(reconnect) {
            QMetaObject::invokeMethod(network.get(),
                                      [network, url, state]() { startNetworkStreamRequest(network, url, state); });
        }
    });
    QObject::connect(reply, &QNetworkReply::errorOccurred, reply, [state, reply](QNetworkReply::NetworkError) {
        {
            const std::scoped_lock lock{state->mutex};
            if(reply == state->reply) {
                if(!state->aborted && state->error.isEmpty()) {
                    state->error = reply->errorString();
                }
                state->finished = true;
            }
        }
        state->ready.notify_all();
    });
}

void startNetworkStreamRequest(const std::shared_ptr<QNetworkAccessManager>& network, const QUrl& url,
                               const std::shared_ptr<NetworkStreamDeviceState>& state)
{
    const bool invoked = QMetaObject::invokeMethod(
        network.get(), [network, url, state]() { startNetworkStreamRequestOnNetworkThread(network, url, state); });
    if(invoked) {
        return;
    }

    {
        const std::scoped_lock lock{state->mutex};
        if(!state->aborted && state->error.isEmpty()) {
            state->error = QObject::tr("Failed to queue network request.");
        }
        state->finished = true;
    }
    state->ready.notify_all();
}
} // namespace

NetworkStreamDevice::NetworkStreamDevice(std::shared_ptr<QNetworkAccessManager> network, QUrl url,
                                         qsizetype maxBufferedBytes, QObject* parent)
    : QIODevice{parent}
    , m_network{std::move(network)}
    , m_url{std::move(url)}
    , m_state{std::make_shared<NetworkStreamDeviceState>()}
{
    m_state->streamBuffer.maxBytes = StreamUtils::clampedBufferBytes(maxBufferedBytes);
}

NetworkStreamDevice::~NetworkStreamDevice()
{
    NetworkStreamDevice::close();
}

bool NetworkStreamDevice::isSequential() const
{
    return true;
}

bool NetworkStreamDevice::readWouldBlock() const
{
    const std::scoped_lock lock{m_state->mutex};
    return m_state->readMode.nonBlockingReadsEnabled && m_state->readMode.readWouldBlock;
}

qsizetype NetworkStreamDevice::bufferedByteCount() const
{
    const std::scoped_lock lock{m_state->mutex};
    return m_state->streamBuffer.data.size();
}

NetworkStreamMetadata NetworkStreamDevice::metadata() const
{
    const std::scoped_lock lock{m_state->mutex};
    return m_state->icy.metadata;
}

std::optional<NetworkStreamMetadata> NetworkStreamDevice::remoteStreamMetadata() const
{
    return metadata();
}

void NetworkStreamDevice::setNonBlockingReadsEnabled(bool enabled)
{
    {
        const std::scoped_lock lock{m_state->mutex};
        m_state->readMode.nonBlockingReadsEnabled = enabled;
        if(!enabled) {
            m_state->readMode.readWouldBlock = false;
        }
    }
    m_state->ready.notify_all();
}

void NetworkStreamDevice::setReconnectOnFinishedEnabled(bool enabled)
{
    {
        const std::scoped_lock lock{m_state->mutex};
        m_state->reconnect.reconnectOnFinished = enabled;
    }
    m_state->ready.notify_all();
}

bool NetworkStreamDevice::open(OpenMode mode)
{
    if(isOpen()) {
        return true;
    }

    if(!m_url.isValid() || !(mode & ReadOnly)) {
        setErrorString(tr("Invalid network stream."));
        return false;
    }

    {
        const std::scoped_lock lock{m_state->mutex};
        m_state->streamBuffer.maxBytes = StreamUtils::clampedBufferBytes(m_state->streamBuffer.maxBytes);
        m_state->resetForOpen();
    }

    QIODevice::open(mode);
    startNetworkStreamRequest(m_network, m_url, m_state);
    return true;
}

void NetworkStreamDevice::close()
{
    QNetworkReply* reply{nullptr};
    {
        const std::scoped_lock lock{m_state->mutex};
        if(m_state->aborted && !m_state->reply) {
            QIODevice::close();
            return;
        }

        m_state->aborted  = true;
        m_state->finished = true;
        reply             = m_state->reply;
        m_state->reply    = nullptr;
    }
    m_state->ready.notify_all();

    if(reply) {
        const auto stopRequest = [reply]() {
            reply->abort();
            reply->deleteLater();
        };

        QMetaObject::invokeMethod(reply, stopRequest);
    }

    QIODevice::close();
}

qint64 NetworkStreamDevice::readData(char* data, qint64 maxSize)
{
    if(maxSize <= 0) {
        return 0;
    }

    if(QThread::currentThread() == m_network->thread()) {
        setErrorString(tr("Network stream cannot be read from the network thread."));
        return -1;
    }

    std::unique_lock lock{m_state->mutex};

    const auto hasReadableState = [state = m_state]() {
        return !state->streamBuffer.data.isEmpty() || state->finished || state->aborted || !state->error.isEmpty();
    };

    bool hasData{true};
    if(m_state->readMode.nonBlockingReadsEnabled) {
        hasData = m_state->ready.wait_for(lock, ReadWaitTimeout, hasReadableState);
    }
    else {
        m_state->ready.wait(lock, hasReadableState);
    }

    if(!hasData && m_state->streamBuffer.data.isEmpty() && !m_state->finished && !m_state->aborted
       && m_state->error.isEmpty()) {
        m_state->readMode.readWouldBlock = true;
        ++m_state->readMode.readWouldBlockCount;

        return -1;
    }

    m_state->readMode.readWouldBlock = false;

    if(!m_state->error.isEmpty()) {
        const QString error = m_state->error;
        lock.unlock();
        setErrorString(error);
        return -1;
    }

    if(m_state->streamBuffer.data.isEmpty()) {
        return 0;
    }

    const qsizetype bytesRead = std::min<qsizetype>(m_state->streamBuffer.data.size(), maxSize);
    std::memcpy(data, m_state->streamBuffer.data.constData(), bytesRead);
    m_state->streamBuffer.data.remove(0, bytesRead);
    m_state->streamBuffer.bytesRead += static_cast<quint64>(bytesRead);

    QNetworkReply* reply = m_state->reply;
    if(reply && std::cmp_less(m_state->streamBuffer.data.size(), m_state->streamBuffer.maxBytes)) {
        QMetaObject::invokeMethod(
            reply,
            [state = m_state, reply]() {
                {
                    const std::scoped_lock appendLock{state->mutex};
                    appendReplyData(*state, reply);
                }
                state->ready.notify_all();
            },
            Qt::QueuedConnection);
    }

    return bytesRead;
}

qint64 NetworkStreamDevice::writeData(const char* /*data*/, qint64 /*maxSize*/)
{
    return -1;
}
} // namespace Fooyin
