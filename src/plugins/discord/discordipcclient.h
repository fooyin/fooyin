/*
 * Fooyin
 * Copyright © 2025, Luke Taylor <LukeT1@proton.me>
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

#include "discordmessage.h"

#include "discordpresencedata.h"

#include <core/player/playerdefs.h>
#include <core/track.h>

#include <QLocalSocket>

#include <QCoro/QCoroTask>

namespace Fooyin::Discord {
class DiscordIPCClient : public QObject
{
    Q_OBJECT

public:
    explicit DiscordIPCClient(QObject* parent = nullptr);

    [[nodiscard]] bool isConnected() const;
    [[nodiscard]] QString errorMessage() const;

    QCoro::Task<bool> connectToDiscord();
    QCoro::Task<> disconnectFromDiscord();

    QCoro::Task<bool> updateActivity(const PresenceData data);
    QCoro::Task<bool> clearActivity();

    QCoro::Task<> changeClientId(const QString clientId);

private:
    void setError(const QString& error);

    std::optional<DiscordMessage> readMessage();
    void sendMessage(const QJsonObject& packet, int opCode);
    bool processMessage(const DiscordMessage message);

    QCoro::Task<bool> waitForReadyRead();
    QCoro::Task<bool> startHandshake();

    QLocalSocket m_socket;
    QDataStream m_stream;

    QString m_clientId;

    bool m_handshakeCompleted;
    QString m_error;
};
} // namespace Fooyin::Discord
