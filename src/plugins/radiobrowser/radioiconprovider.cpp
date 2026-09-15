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

#include <array>
#include <ranges>

using namespace Qt::StringLiterals;

constexpr auto MaxConcurrentRequests = 6;
constexpr auto MaxPendingRequests    = 256;
constexpr auto MinIconSize           = 32;
constexpr auto MaxIconSize           = 384;
constexpr auto MaxCachedIconBytes    = 64 * 1024 * 1024;
constexpr auto MaxFailedIcons        = 2048;
constexpr auto MaxPinnedIcons        = 512;
constexpr auto MaxDecodedPixels      = 2048LL * 2048;
constexpr qsizetype MaxIconBytes     = 2UL * 1024 * 1024;
constexpr auto IconTransferTimeoutMs = 10'000;

constexpr std::array IconBuckets{64, 96, 128, 192, 256, 384};

namespace {
QByteArray readReplyData(QNetworkReply* reply, const qsizetype maxBytes)
{
    if(!reply || maxBytes <= 0 || reply->error() != QNetworkReply::NoError || !reply->isOpen() || !reply->isReadable()
       || reply->bytesAvailable() <= 0) {
        return {};
    }
    return reply->read(std::min(static_cast<qint64>(maxBytes), reply->bytesAvailable()));
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

int normalisedIconSize(const int size)
{
    return std::clamp(size, MinIconSize, MaxIconSize);
}

QString iconCacheKey(const QString& favicon, const int size)
{
    return u"%1#%2"_s.arg(favicon, QString::number(normalisedIconSize(size)));
}

int pixmapCost(const QPixmap& pixmap)
{
    return pixmap.width() * pixmap.height() * pixmap.depth() / 8;
}
} // namespace

namespace Fooyin::RadioBrowser {
RadioIconProvider::RadioIconProvider(std::shared_ptr<NetworkAccessManager> network, QObject* parent)
    : QObject{parent}
    , m_network{std::move(network)}
    , m_icons{MaxCachedIconBytes}
    , m_failed{MaxFailedIcons}
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

QIcon RadioIconProvider::icon(const RadioStation& station, int size) const
{
    const QString favicon = faviconUrl(station);
    const int iconSize    = normalisedIconSize(size);
    const QString key     = iconCacheKey(favicon, iconSize);

    if(const auto pinned = m_pinnedIcons.find(key); pinned != m_pinnedIcons.cend()) {
        return pinned->second;
    }

    if(auto* icon = m_icons.object(key)) {
        return *icon;
    }

    for(const int bucket : IconBuckets) {
        if(bucket <= iconSize) {
            continue;
        }
        if(auto* icon = m_icons.object(iconCacheKey(favicon, bucket))) {
            return *icon;
        }
    }
    return {};
}

void RadioIconProvider::requestIcon(const RadioStation& station, int size)
{
    queueIcon(station, size, true);
    startNextRequests();
}

void RadioIconProvider::setVisibleIcons(QObject* owner, const RadioStationList& stations, int size)
{
    if(!owner) {
        return;
    }

    std::set<QString> keys;
    for(const RadioStation& station : stations) {
        const QString favicon = faviconUrl(station);
        if(!favicon.isEmpty()) {
            keys.emplace(iconCacheKey(favicon, size));
        }
    }

    if(keys.empty()) {
        m_visibleIconKeys.erase(owner);
    }
    else {
        const bool knownOwner = m_visibleIconKeys.contains(owner);
        m_visibleIconKeys.insert_or_assign(owner, std::move(keys));
        if(!knownOwner) {
            QObject::connect(owner, &QObject::destroyed, this, [this, owner]() { clearVisibleIcons(owner); });
        }
    }

    rebuildPinnedIcons();
    pruneQueuedRequests();

    for(const RadioStation& station : stations) {
        const QString key = iconCacheKey(faviconUrl(station), size);
        if(const QIcon cachedIcon = icon(station, size); !cachedIcon.isNull()) {
            pinLoadedIcon(key, cachedIcon);
        }
        else {
            queueIcon(station, size, false);
        }
    }

    prioritiseQueuedRequests();
    startNextRequests();
}

void RadioIconProvider::clearVisibleIcons(QObject* owner)
{
    if(!owner) {
        return;
    }

    m_visibleIconKeys.erase(owner);
    rebuildPinnedIcons();
    pruneQueuedRequests();
    prioritiseQueuedRequests();
    startNextRequests();
}

void RadioIconProvider::startNextRequests()
{
    while(m_replies.size() < MaxConcurrentRequests && !m_queue.empty()) {
        IconRequest iconRequest = std::move(m_queue.front());
        m_queue.pop_front();

        const QUrl url{iconRequest.favicon};
        if(!url.isValid() || (url.scheme() != "http"_L1 && url.scheme() != "https"_L1)) {
            const QString key = iconCacheKey(iconRequest.favicon, iconRequest.size);
            markFailed(iconRequest.favicon);
            m_pending.erase(key);
            m_unscopedPending.erase(key);
            continue;
        }

        QNetworkRequest request = makeNetworkRequest(url);
        request.setMaximumRedirectsAllowed(3);
        request.setTransferTimeout(IconTransferTimeoutMs);
        request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
        request.setAttribute(QNetworkRequest::CacheSaveControlAttribute, false);

        auto* reply = m_network->get(request);
        reply->setReadBufferSize(MaxIconBytes + 1);
        m_replies.emplace(reply, iconRequest);

        QObject::connect(reply, &QNetworkReply::downloadProgress, this, [this, reply](const qint64 received, qint64) {
            if(!m_replies.contains(reply)) {
                return;
            }

            if(received > MaxIconBytes) {
                const IconRequest failedRequest = m_replies.at(reply);
                finishFailedReply(reply, failedRequest.favicon);
                startNextRequests();
            }
        });
        QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]() { handleReply(reply); });
    }
}

void RadioIconProvider::handleReply(QNetworkReply* reply)
{
    const IconRequest iconRequest = m_replies.at(reply);
    const QString favicon         = iconRequest.favicon;
    const int iconSize            = normalisedIconSize(iconRequest.size);
    const QString cacheKey        = iconCacheKey(favicon, iconSize);
    m_replies.erase(reply);
    m_pending.erase(cacheKey);
    m_unscopedPending.erase(cacheKey);

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
                markFailed(favicon);
                handled = false;
            }
        }

        if(handled) {
            const auto size = reader.size();
            if(size.isValid() && !isReasonableImageSize(size)) {
                markFailed(favicon);
                handled = false;
            }
            else if(size.isValid() && (size.width() > iconSize || size.height() > iconSize)) {
                const auto scaledSize = calculateScaledSize(size, iconSize);
                reader.setScaledSize(scaledSize);
            }

            if(handled) {
                const QImage image = reader.read();
                if(!image.isNull()) {
                    QPixmap pixmap = QPixmap::fromImage(image);
                    if(pixmap.width() > iconSize || pixmap.height() > iconSize) {
                        pixmap = pixmap.scaled(iconSize, iconSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                    }
                    const QIcon icon{pixmap};
                    m_icons.insert(cacheKey, new QIcon{icon}, pixmapCost(pixmap));
                    pinLoadedIcon(cacheKey, icon);
                    Q_EMIT iconLoaded(favicon);
                }
                else {
                    markFailed(favicon);
                }
            }
        }
    }
    else {
        markFailed(favicon);
    }

    reply->deleteLater();
    startNextRequests();
}

void RadioIconProvider::finishFailedReply(QNetworkReply* reply, const QString& favicon)
{
    if(const auto request = m_replies.find(reply); request != m_replies.end()) {
        const QString key = iconCacheKey(request->second.favicon, request->second.size);
        m_pending.erase(key);
        m_unscopedPending.erase(key);
        m_replies.erase(request);
    }
    markFailed(favicon);

    QObject::disconnect(reply, nullptr, nullptr, nullptr);
    reply->abort();
    reply->deleteLater();
}

void RadioIconProvider::queueIcon(const RadioStation& station, int size, const bool unscoped)
{
    const QString favicon = faviconUrl(station);

    if(favicon.isEmpty() || !icon(station, size).isNull() || m_failed.contains(favicon)) {
        return;
    }

    const QString cacheKey = iconCacheKey(favicon, size);

    if(m_pending.contains(cacheKey)) {
        if(unscoped) {
            m_unscopedPending.insert(cacheKey);
        }
        return;
    }

    if(m_pending.size() >= MaxPendingRequests) {
        return;
    }

    m_pending.insert(cacheKey);
    if(unscoped) {
        m_unscopedPending.insert(cacheKey);
    }
    m_queue.emplace_back(IconRequest{.favicon = favicon, .size = normalisedIconSize(size)});
}

void RadioIconProvider::pruneQueuedRequests()
{
    std::erase_if(m_queue, [this](const IconRequest& request) {
        const QString key = iconCacheKey(request.favicon, request.size);
        if(isVisible(key) || m_unscopedPending.contains(key)) {
            return false;
        }
        m_pending.erase(key);
        return true;
    });
}

void RadioIconProvider::prioritiseQueuedRequests()
{
    std::ranges::stable_partition(
        m_queue, [this](const IconRequest& request) { return isVisible(iconCacheKey(request.favicon, request.size)); });
}

void RadioIconProvider::rebuildPinnedIcons()
{
    std::set<QString> visibleKeys;
    for(const auto& keys : m_visibleIconKeys | std::views::values) {
        visibleKeys.insert(keys.cbegin(), keys.cend());
    }

    std::erase_if(m_pinnedIcons, [&visibleKeys](const auto& item) { return !visibleKeys.contains(item.first); });

    for(const QString& key : visibleKeys) {
        if(m_pinnedIcons.size() >= MaxPinnedIcons) {
            break;
        }
        if(!m_pinnedIcons.contains(key)) {
            if(auto* icon = m_icons.object(key)) {
                m_pinnedIcons.emplace(key, *icon);
            }
        }
    }
}

void RadioIconProvider::pinLoadedIcon(const QString& key, const QIcon& icon)
{
    if(isVisible(key) && (m_pinnedIcons.contains(key) || m_pinnedIcons.size() < MaxPinnedIcons)) {
        m_pinnedIcons.insert_or_assign(key, icon);
    }
}

bool RadioIconProvider::isVisible(const QString& key) const
{
    return std::ranges::any_of(m_visibleIconKeys | std::views::values,
                               [&key](const std::set<QString>& keys) { return keys.contains(key); });
}

void RadioIconProvider::markFailed(const QString& favicon)
{
    m_failed.insert(favicon, new bool{true});
}
} // namespace Fooyin::RadioBrowser

#include "moc_radioiconprovider.cpp"
