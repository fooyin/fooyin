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

#include <QByteArray>
#include <QDateTime>
#include <QObject>
#include <QPointer>
#include <QUrl>

#include <functional>
#include <optional>
#include <unordered_map>

class QNetworkAccessManager;
class QNetworkReply;

namespace Fooyin::Discord {
class DiscordArtworkUploader : public QObject
{
    Q_OBJECT

public:
    struct Config
    {
        QUrl endpoint;
        int retentionHours{24};
    };

    struct Result
    {
        QUrl url;
        QDateTime expiresAt;
    };

    explicit DiscordArtworkUploader(QNetworkAccessManager* network, QObject* parent = nullptr);

    using Callback = std::function<void(const std::optional<Result>& result)>;
    void upload(const QByteArray& artwork, const Config& config, QObject* context, Callback callback);
    void cancel();
    void clearCache();

    [[nodiscard]] static bool isValidEndpoint(const QUrl& endpoint);

private:
    [[nodiscard]] static QByteArray cacheKey(const QByteArray& artwork, const Config& config);

    QNetworkAccessManager* m_network;

    QPointer<QNetworkReply> m_reply;
    std::unordered_map<QByteArray, Result> m_cache;
    uint64_t m_generation;
};
} // namespace Fooyin::Discord
