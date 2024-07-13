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

#include <QJsonObject>
#include <QObject>

namespace Fooyin::Discord {
class DiscordMessage
{
    Q_GADGET

public:
    enum class EventType : uint8_t
    {
        Null = 0,
        READY,
        ERROR,
    };
    Q_ENUM(EventType);

    DiscordMessage();
    explicit DiscordMessage(const QJsonObject& json, int opcode = 0);

    [[nodiscard]] EventType eventType() const;
    [[nodiscard]] QJsonObject json() const;
    [[nodiscard]] QJsonObject data() const;
    [[nodiscard]] QString nonce() const;

private:
    EventType m_type;
    QJsonObject m_json;
    QJsonObject m_data;
    QString m_nonce;
    int m_opcode;
};
} // namespace Fooyin::Discord
