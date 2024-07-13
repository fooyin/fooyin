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

#include "discordplugin.h"

#include "discordipcclient.h"
#include "discordpresencedata.h"
#include "settings/discordpage.h"
#include "settings/discordsettings.h"

#include <core/player/playercontroller.h>
#include <utils/enum.h>
#include <utils/settings/settingsmanager.h>
#include <utils/signalthrottler.h>

#include <QApplication>
#include <QDataStream>
#include <QJsonArray>
#include <QLoggingCategory>
#include <QTimer>
#include <QUuid>

using namespace Qt::StringLiterals;

namespace {
// constexpr auto LogoFooyin = "fooyin";
constexpr auto LogoPlay  = "play";
constexpr auto LogoPause = "pause";
} // namespace

namespace Fooyin::Discord {
DiscordPlugin::DiscordPlugin()
    : m_discordClient{new DiscordIPCClient(this)}
    , m_throttler{new SignalThrottler(this)}
{ }

void DiscordPlugin::initialise(const CorePluginContext& context)
{
    m_player   = context.playerController;
    m_settings = context.settingsManager;

    m_discordSettings = std::make_unique<DiscordSettings>(m_settings);

    QObject::connect(m_player, &PlayerController::currentTrackChanged, m_throttler, &SignalThrottler::throttle);
    QObject::connect(m_player, &PlayerController::positionMoved, m_throttler, &SignalThrottler::throttle);
    QObject::connect(m_player, &PlayerController::playStateChanged, m_throttler, &SignalThrottler::throttle);

    QObject::connect(m_throttler, &SignalThrottler::triggered, this, &DiscordPlugin::updateActivity);

    m_discordClient->changeClientId(m_settings->value<Settings::Discord::ClientId>());

    if(m_settings->value<Settings::Discord::DiscordEnabled>() && !m_discordClient->isConnected()) {
        m_discordClient->connectToDiscord();
    }

    m_settings->subscribe<Settings::Discord::DiscordEnabled>(this, &DiscordPlugin::toggleEnabled);
    m_settings->subscribe<Settings::Discord::ShowStateIcon>(m_throttler, &SignalThrottler::throttle);
    m_settings->subscribe<Settings::Discord::ClientId>(m_discordClient, &DiscordIPCClient::changeClientId);
    m_settings->subscribe<Settings::Discord::TitleField>(m_throttler, &SignalThrottler::throttle);
    m_settings->subscribe<Settings::Discord::ArtistField>(m_throttler, &SignalThrottler::throttle);
    m_settings->subscribe<Settings::Discord::AlbumField>(m_throttler, &SignalThrottler::throttle);
}

void DiscordPlugin::initialise(const GuiPluginContext& /*context*/)
{
    m_discordPage = new DiscordPage(m_settings, this);
}

void DiscordPlugin::shutdown()
{
    m_discordClient->disconnectFromDiscord();
}

void DiscordPlugin::toggleEnabled(bool enable)
{
    if(enable && !m_discordClient->isConnected()) {
        m_discordClient->connectToDiscord();
    }
    else {
        m_discordClient->disconnectFromDiscord();
    }
}

void DiscordPlugin::updateActivity()
{
    if(!m_settings->value<Settings::Discord::DiscordEnabled>()) {
        return;
    }

    if(m_player->playState() == Player::PlayState::Stopped) {
        m_discordClient->clearActivity();
        return;
    }

    const auto track = m_player->currentTrack();
    if(!track.isValid()) {
        return;
    }

    PresenceData data;

    data.largeText = m_scriptParser.evaluate(m_settings->value<Settings::Discord::AlbumField>(), track);
    data.details   = m_scriptParser.evaluate(m_settings->value<Settings::Discord::TitleField>(), track);
    data.state     = m_scriptParser.evaluate(m_settings->value<Settings::Discord::ArtistField>(), track);
    if(data.state.length() < 2) {
        data.state.append("  "_L1);
    }

    // data.largeImage = QString::fromLatin1(LogoFooyin);

    const bool showPlayState = m_settings->value<Settings::Discord::ShowStateIcon>();
    if(showPlayState) {
        data.smallText = Utils::Enum::toString(m_player->playState());
    }

    switch(m_player->playState()) {
        case(Player::PlayState::Playing): {
            if(showPlayState) {
                data.smallImage = QString::fromLatin1(LogoPlay);
            }

            const auto currentTimeMs = static_cast<quint64>(QDateTime::currentMSecsSinceEpoch());
            const auto startMs       = static_cast<quint64>(m_player->currentPosition());
            const auto endMs         = static_cast<quint64>(track.duration());

            data.start = (currentTimeMs - startMs) / 1000;
            data.end   = (currentTimeMs + (endMs - startMs)) / 1000;
            break;
        }
        case(Player::PlayState::Paused): {
            if(showPlayState) {
                data.smallImage = QString::fromLatin1(LogoPause);
            }
            data.start = 0;
            data.end   = 0;
            break;
        }
        case(Player::PlayState::Stopped):
            break;
    }

    m_discordClient->updateActivity(data);
}
} // namespace Fooyin::Discord
