/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <LukeT1@proton.me>
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

#include <QObject>

class QTcpServer;
class QTcpSocket;

namespace Fooyin::Scrobbler {
class ScrobblerAuthSession : public QObject
{
    Q_OBJECT

public:
    explicit ScrobblerAuthSession(QObject* parent = nullptr);
    ~ScrobblerAuthSession() override;

    [[nodiscard]] QString callbackUrl() const;

signals:
    void tokenReceived(const QString& token);

private:
    void processCallback();
    void sendHttpResponse(const QByteArray& code, const QByteArray& msg);
    void onError(const QByteArray& code, const QString& errorMsg);

    QString m_callbackUrl;
    QTcpServer* m_server;
    QTcpSocket* m_socket{nullptr};
    QByteArray requestData;
};
} // namespace Fooyin::Scrobbler
