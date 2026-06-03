/*
 * Fooyin
 * Copyright © 2025, Luke Taylor <luket@pm.me>
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

#include <QJsonArray>
#include <QTimer>

using namespace Qt::StringLiterals;

constexpr auto LogoPlay  = "play";
constexpr auto LogoPause = "pause";

namespace Fooyin::Discord {
DiscordPlugin::DiscordPlugin()
    : m_discordClient{new DiscordIPCClient(this)}
    , m_throttler{new SignalThrottler(this)}
    , m_positionSyncedTrackId{-1}
    , m_clearOnPause{false}
{ }

void DiscordPlugin::initialise(const CorePluginContext& context)
{
    m_player   = context.playerController;
    m_settings = context.settingsManager;

    m_discordSettings = std::make_unique<DiscordSettings>(m_settings);

    m_clearOnPause = m_settings->value<Settings::Discord::ClearOnPause>();

    QObject::connect(m_player, &PlayerController::currentTrackChanged, this, [this] {
        m_positionSyncedTrackId = -1;
        m_throttler->throttle();
    });
    QObject::connect(m_player, &PlayerController::positionChanged, this, &DiscordPlugin::positionChanged);
    QObject::connect(m_player, &PlayerController::positionMoved, m_throttler, &SignalThrottler::throttle);
    QObject::connect(m_player, &PlayerController::playStateChanged, this, [this](Player::PlayState state) {
        if(state == Player::PlayState::Stopped) {
            m_positionSyncedTrackId = -1;
        }
        m_throttler->throttle();
    });

    QObject::connect(m_throttler, &SignalThrottler::triggered, this, &DiscordPlugin::updateActivity);

    startClientIdChangeTask(m_settings->value<Settings::Discord::ClientId>());

    if(m_settings->value<Settings::Discord::DiscordEnabled>() && !m_discordClient->isConnected()) {
        startConnectTask();
    }

    m_settings->subscribe<Settings::Discord::DiscordEnabled>(this, &DiscordPlugin::toggleEnabled);
    m_settings->subscribe<Settings::Discord::ShowStateIcon>(m_throttler, &SignalThrottler::throttle);
    m_settings->subscribe<Settings::Discord::ClearOnPause>(this, [this](bool enabled) {
        m_clearOnPause = enabled;
        updateActivity();
    });
    m_settings->subscribe<Settings::Discord::ClientId>(this, &DiscordPlugin::startClientIdChangeTask);
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
    m_activityTask.reset();
    m_clientIdTask.reset();
    m_connectTask.reset();
    startDisconnectTask();
}

void DiscordPlugin::toggleEnabled(bool enable)
{
    if(enable && !m_discordClient->isConnected()) {
        startConnectTask();
    }
    else {
        m_activityTask.reset();
        m_clientIdTask.reset();
        m_connectTask.reset();
        startDisconnectTask();
    }
}

void DiscordPlugin::startConnectTask()
{
    m_disconnectTask.reset();
    m_connectTask = m_discordClient->connectToDiscord().then([this](bool connected) {
        if(connected) {
            updateActivity();
        }
    });
}

void DiscordPlugin::startDisconnectTask()
{
    m_disconnectTask = m_discordClient->disconnectFromDiscord();
}

void DiscordPlugin::startClientIdChangeTask(const QString& clientId)
{
    m_clientIdTask = m_discordClient->changeClientId(clientId).then([this] {
        if(m_settings->value<Settings::Discord::DiscordEnabled>() && m_discordClient->isConnected()) {
            updateActivity();
        }
    });
}

void DiscordPlugin::startUpdateActivityTask()
{
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

    const bool showPlayState = m_settings->value<Settings::Discord::ShowStateIcon>();
    if(showPlayState) {
        data.smallText = Utils::Enum::toString(m_player->playState());
    }

    switch(m_player->playState()) {
        case Player::PlayState::Playing: {
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
        case Player::PlayState::Paused: {
            if(showPlayState) {
                data.smallImage = QString::fromLatin1(LogoPause);
            }
            data.start = 0;
            data.end   = 0;
            break;
        }
        case Player::PlayState::Stopped:
            break;
    }

    m_activityTask = m_discordClient->updateActivity(data).then([](bool /*updated*/) { });
}

void DiscordPlugin::startClearActivityTask()
{
    m_activityTask = m_discordClient->clearActivity().then([](bool /*cleared*/) { });
}

void DiscordPlugin::updateActivity()
{
    if(!m_settings->value<Settings::Discord::DiscordEnabled>()) {
        return;
    }

    const auto state = m_player->playState();
    if(state == Player::PlayState::Stopped || (state == Player::PlayState::Paused && m_clearOnPause)) {
        startClearActivityTask();
        return;
    }

    startUpdateActivityTask();
}

void DiscordPlugin::positionChanged(uint64_t ms)
{
    if(ms == 0) {
        return;
    }

    const auto track = m_player->currentTrack();
    if(!track.isValid()) {
        return;
    }

    const int trackId = track.id();
    if(trackId < 0 || m_positionSyncedTrackId == trackId) {
        return;
    }

    m_positionSyncedTrackId = trackId;
    m_throttler->throttle();
}
} // namespace Fooyin::Discord
