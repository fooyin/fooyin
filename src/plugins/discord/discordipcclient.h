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

#include <core/player/playerdefs.h>
#include <core/track.h>

#include <QLocalSocket>

namespace Fooyin::Discord {
struct PresenceData;

class DiscordIPCClient : public QObject
{
    Q_OBJECT

public:
    explicit DiscordIPCClient(QObject* parent = nullptr);

    [[nodiscard]] bool isConnected() const;
    [[nodiscard]] QString status() const;

    void connectToDiscord();
    void disconnectFromDiscord();

    void updateActivity(const PresenceData& data);
    void clearActivity();

    void changeClientId(const QString& clientId);

signals:
    void ready();

private:
    void connectToPipe();

    std::optional<DiscordMessage> readMessage();
    void sendMessage(const QJsonObject& packet, int opCode);
    void processMessage(const DiscordMessage& message);
    void readAndProcessMessages();

    void startTimeout(const QString& nonce);
    void startHandshake();

    QLocalSocket m_socket;
    QDataStream m_stream;

    QString m_clientId;

    int m_pipeIndex;
    bool m_handshakeCompleted;

    bool m_connected;
    QString m_status;
    QString m_connectionError;

    std::vector<QString> m_replies;
};
} // namespace Fooyin::Discord
