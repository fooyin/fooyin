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

#include "discordmessage.h"

#include <utils/enum.h>

using namespace Qt::StringLiterals;

namespace Fooyin::Discord {
DiscordMessage::DiscordMessage()
    : DiscordMessage{{}, 0}
{ }

DiscordMessage::DiscordMessage(const QJsonObject& json, int opcode)
    : m_type{EventType::Null}
    , m_json{json}
    , m_opcode{opcode}
{
    if(json.contains("evt"_L1)) {
        const auto event = json.value("evt"_L1).toString();
        if(!event.isEmpty()) {
            if(auto type = Utils::Enum::fromString<EventType>(event)) {
                m_type = type.value();
            }
        }
    }
    if(json.contains("data"_L1)) {
        m_data = json.value("data"_L1).toObject();
    }
    if(json.contains("nonce"_L1)) {
        m_nonce = json.value("nonce"_L1).toString();
    }
}

DiscordMessage::EventType DiscordMessage::eventType() const
{
    return m_type;
}

QJsonObject DiscordMessage::json() const
{
    return m_json;
}

QJsonObject DiscordMessage::data() const
{
    return m_data;
}

QString DiscordMessage::nonce() const
{
    return m_nonce;
}
} // namespace Fooyin::Discord
