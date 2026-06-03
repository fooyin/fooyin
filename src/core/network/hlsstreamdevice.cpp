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

#include "hlsstreamdevice.h"

#include "streamdeviceutils.h"

#include <core/network/networkutils.h>

#include <QMetaObject>
#include <QNetworkReply>
#include <QPointer>
#include <QThread>
#include <QTimer>

#include <algorithm>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <mutex>
#include <unordered_set>

using namespace Qt::StringLiterals;

namespace Fooyin {
struct HlsVariant
{
    QUrl url;
    int bandwidth{0};
};

struct HlsPlaylistState
{
    QUrl url;
    QUrl currentMapUrl;
    std::deque<QUrl> segmentQueue;
    std::unordered_set<QString> seenSegments;
    std::deque<QString> seenSegmentOrder;
    bool ended{false};
    bool refreshScheduled{false};
    int targetDurationMs{3000};

    void reset(const QUrl& playlistUrl)
    {
        url           = playlistUrl;
        currentMapUrl = QUrl{};
        segmentQueue.clear();
        seenSegments.clear();
        seenSegmentOrder.clear();
        ended            = false;
        refreshScheduled = false;
        targetDurationMs = 3000;
    }
};

class HlsStreamDeviceState
{
public:
    enum class ReplyKind : uint8_t
    {
        None = 0,
        Playlist,
        Segment,
    };

    QNetworkReply* reply{nullptr};
    ReplyKind replyKind{ReplyKind::None};
    std::mutex mutex;
    std::condition_variable ready;
    QString error;
    bool finished{false};
    bool aborted{false};
    StreamUtils::BufferState streamBuffer;
    HlsPlaylistState playlist;
    StreamUtils::ReadModeState readMode;

    void resetForOpen(const QUrl& playlistUrl)
    {
        error.clear();
        finished  = false;
        aborted   = false;
        reply     = nullptr;
        replyKind = ReplyKind::None;
        streamBuffer.reset();
        playlist.reset(playlistUrl);
        readMode.reset();
    }
};

namespace {
constexpr size_t MaxRememberedHlsSegments = 256;

void appendSegmentData(HlsStreamDeviceState& state, QNetworkReply* reply)
{
    if(reply != state.reply || state.replyKind != HlsStreamDeviceState::ReplyKind::Segment || state.aborted) {
        return;
    }

    StreamUtils::appendReplyData(state.streamBuffer, reply);
}

QUrl absoluteUrl(const QUrl& base, const QString& value)
{
    return base.resolved(QUrl{value.trimmed()});
}

// This chooses the highest-bandwidth variant for now
// TODO: Add selection options, and maybe adaptive selection based on measured bandwidth
void parsePlaylist(HlsStreamDeviceState& state, const QByteArray& data)
{
    const QString text      = QString::fromUtf8(data);
    const QStringList lines = text.split(u'\n', Qt::SkipEmptyParts);

    std::vector<QUrl> discoveredSegments;
    QUrl nextMapUrl;
    bool endList{false};
    bool nextLineIsVariant{false};
    std::vector<HlsVariant> variants;
    std::optional<int> nextVariantBandwidth;
    int targetDurationMs{state.playlist.targetDurationMs};

    for(QString line : lines) {
        line = line.trimmed();
        if(line.isEmpty()) {
            continue;
        }

        if(line.startsWith("#EXT-X-TARGETDURATION:"_L1)) {
            bool ok{false};
            const int seconds = line.sliced(22).trimmed().toInt(&ok);
            if(ok && seconds > 0) {
                targetDurationMs = seconds * 1000;
            }
            continue;
        }

        if(line.startsWith("#EXT-X-STREAM-INF:"_L1)) {
            const qsizetype bandwidthStart = line.indexOf("BANDWIDTH="_L1);
            if(bandwidthStart >= 0) {
                const qsizetype valueStart = bandwidthStart + 10;
                qsizetype valueEnd         = line.indexOf(u',', valueStart);
                if(valueEnd < 0) {
                    valueEnd = line.size();
                }

                bool ok{false};
                nextVariantBandwidth = line.mid(valueStart, valueEnd - valueStart).toInt(&ok);
                if(!ok) {
                    nextVariantBandwidth = 0;
                }
            }

            nextLineIsVariant = true;
            continue;
        }

        if(line.startsWith("#EXT-X-MAP:"_L1)) {
            const qsizetype uriStart = line.indexOf("URI=\""_L1);
            if(uriStart >= 0) {
                const qsizetype valueStart = uriStart + 5;
                const qsizetype valueEnd   = line.indexOf(u'"', valueStart);
                if(valueEnd > valueStart) {
                    nextMapUrl = absoluteUrl(state.playlist.url, line.mid(valueStart, valueEnd - valueStart));
                }
            }
            continue;
        }

        if(line == "#EXT-X-ENDLIST"_L1) {
            endList = true;
            continue;
        }

        if(line.startsWith(u'#')) {
            continue;
        }

        const QUrl segmentUrl = absoluteUrl(state.playlist.url, line);
        if(!segmentUrl.isValid()) {
            nextLineIsVariant = false;
            continue;
        }
        if(nextLineIsVariant) {
            variants.emplace_back(segmentUrl, nextVariantBandwidth.value_or(0));
            nextLineIsVariant = false;
            nextVariantBandwidth.reset();
        }
        else {
            discoveredSegments.push_back(segmentUrl);
        }
    }

    state.playlist.targetDurationMs = targetDurationMs;
    state.playlist.ended            = endList;

    if(discoveredSegments.empty()) {
        if(!variants.empty()) {
            const auto best = std::ranges::max_element(variants, {}, &HlsVariant::bandwidth);

            state.playlist.url              = best->url;
            state.playlist.targetDurationMs = 1000;
            state.playlist.currentMapUrl    = QUrl{};
            state.playlist.ended            = false;
            state.playlist.seenSegments.clear();
            state.playlist.seenSegmentOrder.clear();
        }

        return;
    }

    const size_t startIndex = endList ? 0 : std::max<size_t>(0, discoveredSegments.size() - 3);

    for(size_t i{startIndex}; i < discoveredSegments.size(); ++i) {
        const QUrl& segmentUrl = discoveredSegments.at(i);
        const QString key      = segmentUrl.toString(QUrl::FullyEncoded);
        if(state.playlist.seenSegments.contains(key)) {
            continue;
        }

        if(!nextMapUrl.isEmpty() && nextMapUrl != state.playlist.currentMapUrl) {
            state.playlist.segmentQueue.push_back(nextMapUrl);
            state.playlist.currentMapUrl = nextMapUrl;
        }

        state.playlist.seenSegments.insert(key);
        state.playlist.seenSegmentOrder.push_back(key);
        state.playlist.segmentQueue.push_back(segmentUrl);
    }

    while(state.playlist.seenSegmentOrder.size() > MaxRememberedHlsSegments) {
        const QString oldest = std::move(state.playlist.seenSegmentOrder.front());
        state.playlist.seenSegmentOrder.pop_front();
        state.playlist.seenSegments.erase(oldest);
    }
}

class HlsRequestScheduler
{
public:
    HlsRequestScheduler(QNetworkAccessManager* network, std::shared_ptr<HlsStreamDeviceState> state);

    void startPlaylistRequest() const;
    void startSegmentRequest() const;

private:
    void schedulePlaylistRefresh() const;
    void startRequest(const QUrl& url, HlsStreamDeviceState::ReplyKind kind) const;

    QPointer<QNetworkAccessManager> m_network;
    std::shared_ptr<HlsStreamDeviceState> m_state;
};

HlsRequestScheduler::HlsRequestScheduler(QNetworkAccessManager* network, std::shared_ptr<HlsStreamDeviceState> state)
    : m_network{network}
    , m_state{std::move(state)}
{ }

void HlsRequestScheduler::startPlaylistRequest() const
{
    QUrl url;
    {
        const std::scoped_lock lock{m_state->mutex};
        if(m_state->aborted || m_state->finished || m_state->reply) {
            return;
        }
        url = m_state->playlist.url;
    }

    startRequest(url, HlsStreamDeviceState::ReplyKind::Playlist);
}

void HlsRequestScheduler::startSegmentRequest() const
{
    QUrl url;
    {
        const std::scoped_lock lock{m_state->mutex};
        if(m_state->aborted || m_state->finished || m_state->reply || m_state->playlist.segmentQueue.empty()) {
            return;
        }
        url = m_state->playlist.segmentQueue.front();
        m_state->playlist.segmentQueue.pop_front();
    }

    startRequest(url, HlsStreamDeviceState::ReplyKind::Segment);
}

void HlsRequestScheduler::schedulePlaylistRefresh() const
{
    if(!m_network) {
        return;
    }

    int delayMs{1000};
    {
        const std::scoped_lock lock{m_state->mutex};
        if(m_state->aborted || m_state->finished || m_state->playlist.ended || m_state->playlist.refreshScheduled) {
            return;
        }
        m_state->playlist.refreshScheduled = true;
        delayMs                            = std::max(500, m_state->playlist.targetDurationMs / 2);
    }

    QTimer::singleShot(delayMs, m_network, [scheduler = *this]() {
        {
            const std::scoped_lock lock{scheduler.m_state->mutex};
            scheduler.m_state->playlist.refreshScheduled = false;
            if(scheduler.m_state->aborted || scheduler.m_state->finished || !scheduler.m_network) {
                return;
            }
        }
        scheduler.startPlaylistRequest();
    });
}

void HlsRequestScheduler::startRequest(const QUrl& url, HlsStreamDeviceState::ReplyKind kind) const
{
    if(!m_network) {
        return;
    }

    auto run = [scheduler = *this, url, kind]() {
        if(!scheduler.m_network) {
            return;
        }

        const QNetworkRequest request
            = makeNetworkRequest(url, NetworkRequestOption::AlwaysNetwork | NetworkRequestOption::AcceptAny);
        auto* reply = scheduler.m_network->get(request);
        reply->setReadBufferSize(static_cast<qint64>(scheduler.m_state->streamBuffer.maxBytes));

        {
            const std::scoped_lock lock{scheduler.m_state->mutex};
            if(scheduler.m_state->aborted) {
                reply->abort();
                reply->deleteLater();
                return;
            }
            scheduler.m_state->reply     = reply;
            scheduler.m_state->replyKind = kind;
        }

        QObject::connect(reply, &QIODevice::readyRead, reply, [state = scheduler.m_state, reply]() {
            {
                const std::scoped_lock lock{state->mutex};
                appendSegmentData(*state, reply);
            }
            state->ready.notify_all();
        });
        QObject::connect(reply, &QNetworkReply::finished, reply, [scheduler, reply]() {
            QByteArray playlistData;
            HlsStreamDeviceState::ReplyKind replyKind{HlsStreamDeviceState::ReplyKind::None};
            bool startNextSegment{false};
            bool refreshPlaylist{false};

            {
                const std::scoped_lock lock{scheduler.m_state->mutex};
                if(reply != scheduler.m_state->reply) {
                    reply->deleteLater();
                    return;
                }

                replyKind = scheduler.m_state->replyKind;
                if(reply->error() != QNetworkReply::NoError && !scheduler.m_state->aborted
                   && scheduler.m_state->error.isEmpty()) {
                    scheduler.m_state->error = reply->errorString();
                }
                else if(replyKind == HlsStreamDeviceState::ReplyKind::Playlist) {
                    playlistData = reply->readAll();
                }

                scheduler.m_state->reply     = nullptr;
                scheduler.m_state->replyKind = HlsStreamDeviceState::ReplyKind::None;

                if(scheduler.m_state->error.isEmpty() && !scheduler.m_state->aborted) {
                    if(replyKind == HlsStreamDeviceState::ReplyKind::Segment) {
                        startNextSegment = !scheduler.m_state->playlist.segmentQueue.empty();
                        refreshPlaylist
                            = scheduler.m_state->playlist.segmentQueue.empty() && !scheduler.m_state->playlist.ended;
                        if(scheduler.m_state->playlist.segmentQueue.empty() && scheduler.m_state->playlist.ended) {
                            scheduler.m_state->finished = true;
                        }
                    }
                }
            }

            if(replyKind == HlsStreamDeviceState::ReplyKind::Playlist && !playlistData.isEmpty()) {
                {
                    const std::scoped_lock lock{scheduler.m_state->mutex};
                    if(!scheduler.m_state->aborted && scheduler.m_state->error.isEmpty()) {
                        scheduler.m_state->playlist.url = reply->url();
                        parsePlaylist(*scheduler.m_state, playlistData);
                        startNextSegment = !scheduler.m_state->playlist.segmentQueue.empty();
                        refreshPlaylist
                            = scheduler.m_state->playlist.segmentQueue.empty() && !scheduler.m_state->playlist.ended;
                        if(scheduler.m_state->playlist.segmentQueue.empty() && scheduler.m_state->playlist.ended) {
                            scheduler.m_state->finished = true;
                        }
                    }
                }
            }

            reply->deleteLater();
            scheduler.m_state->ready.notify_all();

            if(startNextSegment) {
                scheduler.startSegmentRequest();
            }
            else if(refreshPlaylist) {
                scheduler.schedulePlaylistRefresh();
            }
        });
    };

    QMetaObject::invokeMethod(m_network, run);
}
} // namespace

HlsStreamDevice::HlsStreamDevice(std::shared_ptr<QNetworkAccessManager> network, QUrl url, qsizetype maxBufferedBytes,
                                 QObject* parent)
    : QIODevice{parent}
    , m_network{std::move(network)}
    , m_url{std::move(url)}
    , m_state{std::make_shared<HlsStreamDeviceState>()}
{
    m_state->streamBuffer.maxBytes = StreamUtils::clampedBufferBytes(maxBufferedBytes);
}

HlsStreamDevice::~HlsStreamDevice()
{
    HlsStreamDevice::close();
}

bool HlsStreamDevice::isSequential() const
{
    return true;
}

bool HlsStreamDevice::readWouldBlock() const
{
    const std::scoped_lock lock{m_state->mutex};
    return m_state->readMode.nonBlockingReadsEnabled && m_state->readMode.readWouldBlock;
}

qsizetype HlsStreamDevice::bufferedByteCount() const
{
    const std::scoped_lock lock{m_state->mutex};
    return m_state->streamBuffer.data.size();
}

bool HlsStreamDevice::shouldExposePathToDecoder() const
{
    return false;
}

void HlsStreamDevice::setNonBlockingReadsEnabled(bool enabled)
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

bool HlsStreamDevice::open(OpenMode mode)
{
    if(isOpen()) {
        return true;
    }

    if(!m_url.isValid() || !(mode & ReadOnly)) {
        setErrorString(tr("Invalid HLS stream."));
        return false;
    }

    {
        const std::scoped_lock lock{m_state->mutex};
        m_state->streamBuffer.maxBytes = StreamUtils::clampedBufferBytes(m_state->streamBuffer.maxBytes);
        m_state->resetForOpen(m_url);
    }

    QIODevice::open(mode);
    HlsRequestScheduler{m_network.get(), m_state}.startPlaylistRequest();
    return true;
}

void HlsStreamDevice::close()
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
        auto stopRequest = [reply]() {
            reply->abort();
            reply->deleteLater();
        };

        QMetaObject::invokeMethod(reply, stopRequest);
    }

    QIODevice::close();
}

qint64 HlsStreamDevice::readData(char* data, qint64 maxSize)
{
    if(maxSize <= 0) {
        return 0;
    }

    if(QThread::currentThread() == m_network->thread()) {
        setErrorString(tr("HLS stream cannot be read from the network thread."));
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
    const bool shouldPullActiveReply
        = reply && std::cmp_less(m_state->streamBuffer.data.size(), m_state->streamBuffer.maxBytes);
    const bool shouldStartNextSegment
        = std::cmp_less(m_state->streamBuffer.data.size(), m_state->streamBuffer.maxBytes / 2) && !reply
       && !m_state->playlist.segmentQueue.empty();
    lock.unlock();

    if(shouldPullActiveReply) {
        QMetaObject::invokeMethod(reply, [state = m_state, reply]() {
            {
                const std::scoped_lock appendLock{state->mutex};
                appendSegmentData(*state, reply);
            }
            state->ready.notify_all();
        });
    }

    if(shouldStartNextSegment) {
        HlsRequestScheduler{m_network.get(), m_state}.startSegmentRequest();
    }

    return bytesRead;
}

qint64 HlsStreamDevice::writeData(const char* /*data*/, qint64 /*maxSize*/)
{
    return -1;
}
} // namespace Fooyin
