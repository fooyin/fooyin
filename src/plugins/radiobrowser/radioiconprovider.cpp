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

#include "radioiconprovider.h"

#include <core/network/networkaccessmanager.h>
#include <core/network/networkutils.h>

#include <QBuffer>
#include <QImageReader>
#include <QMimeDatabase>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPixmap>
#include <QUrl>

using namespace Qt::StringLiterals;

constexpr auto MaxConcurrentRequests = 6;
constexpr auto MaxPendingRequests    = 256;
constexpr auto IconSize              = 128;
constexpr auto MaxCachedIcons        = 512;
constexpr auto MaxDecodedPixels      = 2048LL * 2048;
constexpr qsizetype MaxIconBytes     = 2UL * 1024 * 1024;
constexpr auto IconTransferTimeoutMs = 10'000;

namespace {
QByteArray readReplyData(QNetworkReply* reply, const qsizetype maxBytes)
{
    if(!reply || maxBytes <= 0 || reply->error() != QNetworkReply::NoError || !reply->isOpen() || !reply->isReadable()
       || reply->bytesAvailable() <= 0) {
        return {};
    }
    return reply->read(std::min(maxBytes, reply->bytesAvailable()));
}

QSize calculateScaledSize(const QSize& originalSize, int maxSize)
{
    if(!originalSize.isValid() || originalSize.isEmpty()) {
        return {};
    }

    QSize scaledSize = originalSize;
    scaledSize.scale(maxSize, maxSize, Qt::KeepAspectRatio);
    return scaledSize;
}

bool isReasonableImageSize(const QSize& size)
{
    if(!size.isValid() || size.isEmpty()) {
        return false;
    }

    return static_cast<qint64>(size.width()) * static_cast<qint64>(size.height()) <= MaxDecodedPixels;
}

QString faviconUrl(const Fooyin::RadioBrowser::RadioStation& station)
{
    return station.favicon.trimmed();
}
} // namespace

namespace Fooyin::RadioBrowser {
RadioIconProvider::RadioIconProvider(std::shared_ptr<NetworkAccessManager> network, QObject* parent)
    : QObject{parent}
    , m_network{std::move(network)}
    , m_icons{MaxCachedIcons}
{ }

RadioIconProvider::~RadioIconProvider()
{
    while(!m_replies.empty()) {
        auto* reply = m_replies.begin()->first;
        QObject::disconnect(reply, nullptr, this, nullptr);
        reply->abort();
        reply->deleteLater();
        m_replies.erase(reply);
    }
}

QIcon RadioIconProvider::icon(const RadioStation& station) const
{
    if(auto* icon = m_icons.object(faviconUrl(station))) {
        return *icon;
    }
    return {};
}

void RadioIconProvider::requestIcon(const RadioStation& station)
{
    const QString favicon = faviconUrl(station);
    if(favicon.isEmpty() || m_icons.object(favicon) || m_failed.contains(favicon) || m_pending.contains(favicon)) {
        return;
    }
    if(m_pending.size() >= MaxPendingRequests) {
        return;
    }

    m_pending.insert(favicon);
    m_queue.emplace(favicon);
    startNextRequests();
}

void RadioIconProvider::startNextRequests()
{
    while(m_replies.size() < MaxConcurrentRequests && !m_queue.empty()) {
        const QString favicon = m_queue.front();
        m_queue.pop();

        const QUrl url{favicon};
        if(!url.isValid() || (url.scheme() != "http"_L1 && url.scheme() != "https"_L1)) {
            m_failed.emplace(favicon);
            m_pending.erase(favicon);
            continue;
        }

        QNetworkRequest request = makeNetworkRequest(url);
        request.setMaximumRedirectsAllowed(3);
        request.setTransferTimeout(IconTransferTimeoutMs);
        request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
        request.setAttribute(QNetworkRequest::CacheSaveControlAttribute, false);

        auto* reply = m_network->get(request);
        reply->setReadBufferSize(MaxIconBytes + 1);
        m_replies.emplace(reply, favicon);

        QObject::connect(reply, &QNetworkReply::downloadProgress, this, [this, reply](const qint64 received, qint64) {
            if(!m_replies.contains(reply)) {
                return;
            }

            if(received > MaxIconBytes) {
                const QString failedFavicon = m_replies.at(reply);
                finishFailedReply(reply, failedFavicon);
                startNextRequests();
            }
        });
        QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]() { handleReply(reply); });
    }
}

void RadioIconProvider::handleReply(QNetworkReply* reply)
{
    const QString favicon = m_replies.at(reply);
    m_replies.erase(reply);
    m_pending.erase(favicon);

    bool handled{true};

    if(reply->error() == QNetworkReply::NoError) {
        QByteArray data = readReplyData(reply, MaxIconBytes);

        QBuffer buffer{&data};
        buffer.open(QIODevice::ReadOnly);

        const QMimeDatabase mimeDb;
        const auto mimeType   = mimeDb.mimeTypeForData(&buffer);
        const auto formatHint = mimeType.preferredSuffix().toLocal8Bit().toLower();
        buffer.seek(0);

        QImageReader reader{&buffer, formatHint};
        reader.setAutoTransform(true);

        if(!reader.canRead()) {
            reader.setFormat({});
            reader.setDevice(&buffer);
            buffer.seek(0);
            if(!reader.canRead()) {
                m_failed.insert(favicon);
                handled = false;
            }
        }

        if(handled) {
            const auto size = reader.size();
            if(size.isValid() && !isReasonableImageSize(size)) {
                m_failed.insert(favicon);
                handled = false;
            }
            else if(size.isValid() && (size.width() > IconSize || size.height() > IconSize)) {
                const auto scaledSize = calculateScaledSize(size, IconSize);
                reader.setScaledSize(scaledSize);
            }

            if(handled) {
                const QImage image = reader.read();
                if(!image.isNull()) {
                    QPixmap pixmap = QPixmap::fromImage(image);
                    if(pixmap.width() > IconSize || pixmap.height() > IconSize) {
                        pixmap = pixmap.scaled(IconSize, IconSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                    }
                    m_icons.insert(favicon, new QIcon{pixmap});
                    Q_EMIT iconLoaded(favicon);
                }
                else {
                    m_failed.insert(favicon);
                }
            }
        }
    }
    else {
        m_failed.insert(favicon);
    }

    reply->deleteLater();
    startNextRequests();
}

void RadioIconProvider::finishFailedReply(QNetworkReply* reply, const QString& favicon)
{
    m_replies.erase(reply);
    m_pending.erase(favicon);
    m_failed.insert(favicon);

    QObject::disconnect(reply, nullptr, nullptr, nullptr);
    reply->abort();
    reply->deleteLater();
}
} // namespace Fooyin::RadioBrowser

#include "moc_radioiconprovider.cpp"
