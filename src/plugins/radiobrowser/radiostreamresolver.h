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

#pragma once

#include <core/network/networkaccessmanager.h>

#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QUrl>

#include <unordered_map>

class QNetworkReply;

namespace Fooyin {
class PlaylistLoader;

namespace RadioBrowser {
class RadioStreamResolver : public QObject
{
    Q_OBJECT

public:
    explicit RadioStreamResolver(std::shared_ptr<NetworkAccessManager> network,
                                 std::shared_ptr<PlaylistLoader> playlistLoader, QObject* parent = nullptr);

    void resolve(int requestId, const QString& url);

Q_SIGNALS:
    void resolved(int requestId, const QString& originalUrl, const QStringList& streamUrls);
    void failed(int requestId, const QString& originalUrl, const QString& error);

private:
    struct PendingRequest
    {
        QUrl url;
        QByteArray data;
        bool forcePlaylist{false};
        bool directStream{false};
    };

    void fetchStreamInfo(int requestId, const QUrl& url, bool forcePlaylist);
    void handleMetadataChanged(int requestId, const QPointer<QNetworkReply>& reply);
    void handleReadyRead(int requestId, const QPointer<QNetworkReply>& reply);
    void handleReply(int requestId, const QPointer<QNetworkReply>& reply, const QUrl& url);

    void finishReply(const QPointer<QNetworkReply>& reply) const;
    void queueCompleteDirectStream(int requestId, const QPointer<QNetworkReply>& reply, QUrl url);
    void completeDirectStream(int requestId, const QPointer<QNetworkReply>& reply, const QUrl& url);

    std::shared_ptr<NetworkAccessManager> m_network;
    std::shared_ptr<PlaylistLoader> m_playlistLoader;

    std::unordered_map<int, QPointer<QNetworkReply>> m_replies;
    std::unordered_map<int, PendingRequest> m_pendingRequests;
};
} // namespace RadioBrowser
} // namespace Fooyin