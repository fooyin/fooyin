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
 */

#pragma once

#include <core/track.h>

#include <QDateTime>
#include <QObject>
#include <QPointer>
#include <QUrl>

#include <functional>
#include <optional>
#include <unordered_map>
#include <vector>

class QNetworkAccessManager;
class QNetworkReply;

namespace Fooyin::Discord {
class DiscordCoverArtResolver : public QObject
{
    Q_OBJECT

public:
    explicit DiscordCoverArtResolver(QNetworkAccessManager* network, QObject* parent = nullptr);

    using Callback = std::function<void(const std::optional<QUrl>& url)>;
    void resolve(const Track& track, QObject* context, Callback callback);
    void cancel();
    void clearCache();

private:
    struct CacheEntry
    {
        std::optional<QUrl> url;
        QDateTime expiresAt;
    };

    static std::vector<QUrl> candidateUrls(const Track& track);
    static QByteArray cacheKey(const std::vector<QUrl>& candidates);

    void requestNext(uint64_t generation);
    void finish(const std::optional<QUrl>& url, uint64_t generation);

    QNetworkAccessManager* m_network;

    QPointer<QNetworkReply> m_reply;
    QPointer<QObject> m_context;
    std::unordered_map<QByteArray, CacheEntry> m_cache;
    std::vector<QUrl> m_candidates;
    QByteArray m_cacheKey;
    Callback m_callback;
    size_t m_candidateIndex;
    uint64_t m_generation;
};
} // namespace Fooyin::Discord
