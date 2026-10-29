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

#include "radiostation.h"

#include <QCache>
#include <QIcon>
#include <QObject>

#include <deque>
#include <map>
#include <memory>
#include <set>
#include <unordered_map>

class QNetworkReply;

namespace Fooyin {
class NetworkAccessManager;

namespace RadioBrowser {
class RadioIconProvider : public QObject
{
    Q_OBJECT

public:
    explicit RadioIconProvider(std::shared_ptr<NetworkAccessManager> network, QObject* parent = nullptr);
    ~RadioIconProvider() override;

    [[nodiscard]] QIcon icon(const RadioStation& station, int size) const;
    void requestIcon(const RadioStation& station, int size);

    void setVisibleIcons(QObject* owner, const RadioStationList& stations, int size);
    void clearVisibleIcons(QObject* owner);

Q_SIGNALS:
    void iconLoaded(const QString& favicon);

private:
    void startNextRequests();
    void handleReply(QNetworkReply* reply);
    void finishFailedReply(QNetworkReply* reply, const QString& favicon);

    void queueIcon(const RadioStation& station, int size, bool unscoped);
    void pruneQueuedRequests();
    void prioritiseQueuedRequests();
    void rebuildPinnedIcons();
    void pinLoadedIcon(const QString& key, const QIcon& icon);
    [[nodiscard]] bool isVisible(const QString& key) const;
    void markFailed(const QString& favicon);

    struct IconRequest
    {
        QString favicon;
        int size{0};
    };

    std::shared_ptr<NetworkAccessManager> m_network;

    QCache<QString, QIcon> m_icons;
    QCache<QString, bool> m_failed;
    std::set<QString> m_pending;
    std::set<QString> m_unscopedPending;
    std::deque<IconRequest> m_queue;
    std::unordered_map<QNetworkReply*, IconRequest> m_replies;
    std::unordered_map<QObject*, std::set<QString>> m_visibleIconKeys;
    std::map<QString, QIcon> m_pinnedIcons;
};
} // namespace RadioBrowser
} // namespace Fooyin
