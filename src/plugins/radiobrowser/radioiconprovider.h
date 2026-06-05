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

#include <memory>
#include <queue>
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

    [[nodiscard]] QIcon icon(const RadioStation& station) const;
    void requestIcon(const RadioStation& station);

Q_SIGNALS:
    void iconLoaded(const QString& favicon);

private:
    void startNextRequests();
    void handleReply(QNetworkReply* reply);
    void finishFailedReply(QNetworkReply* reply, const QString& favicon);

    std::shared_ptr<NetworkAccessManager> m_network;

    QCache<QString, QIcon> m_icons;
    std::set<QString> m_failed;
    std::set<QString> m_pending;
    std::queue<QString> m_queue;
    std::unordered_map<QNetworkReply*, QString> m_replies;
};
} // namespace RadioBrowser
} // namespace Fooyin
