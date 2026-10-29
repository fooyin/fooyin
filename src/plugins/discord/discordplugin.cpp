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

#include "discordartworkuploader.h"
#include "discordcoverartresolver.h"
#include "discordipcclient.h"
#include "discordpresencedata.h"
#include "settings/discordpage.h"
#include "settings/discordsettings.h"

#include <core/network/networkaccessmanager.h>
#include <core/player/playercontroller.h>
#include <gui/coverprovider.h>
#include <utils/enum.h>
#include <utils/settings/settingsmanager.h>
#include <utils/signalthrottler.h>

#include <QBuffer>
#include <QJsonArray>
#include <QPixmap>
#include <QTimerEvent>

using namespace Qt::StringLiterals;

constexpr auto LogoFooyin         = "fooyin"_L1;
constexpr auto LogoPlay           = "play"_L1;
constexpr auto LogoPause          = "pause"_L1;
constexpr auto LitterboxEndpoint  = "https://litterbox.catbox.moe/resources/internals/api.php"_L1;
constexpr auto ArtworkLoadDelayMs = 1000;

namespace Fooyin::Discord {
namespace {
enum class ArtworkProvider : uint8_t
{
    Upload = 0,
    MusicBrainz,
};

std::optional<ArtworkProvider> artworkProvider(Settings::Discord::ArtworkMode mode, int attempt)
{
    using enum Settings::Discord::ArtworkMode;

    switch(mode) {
        case UploadOnly:
            if(attempt == 0) {
                return ArtworkProvider::Upload;
            }
            return {};
        case MusicBrainzOnly:
            if(attempt == 0) {
                return ArtworkProvider::MusicBrainz;
            }
            return {};
        case UploadOrMusicBrainz:
            if(attempt < 2) {
                return attempt == 0 ? ArtworkProvider::Upload : ArtworkProvider::MusicBrainz;
            }
            return {};
        case MusicBrainzOrUpload:
            if(attempt < 2) {
                return attempt == 0 ? ArtworkProvider::MusicBrainz : ArtworkProvider::Upload;
            }
            return {};
    }
    return {};
}
} // namespace

DiscordPlugin::DiscordPlugin()
    : m_discordClient{new DiscordIPCClient(this)}
    , m_throttler{new SignalThrottler(this)}
    , m_artworkGeneration{0}
    , m_positionSyncedTrackId{-1}
    , m_artworkAttempted{false}
    , m_clearOnPause{false}
{ }

void DiscordPlugin::initialise(const CorePluginContext& context)
{
    m_player           = context.playerController;
    m_settings         = context.settingsManager;
    m_networkAccess    = context.networkAccess;
    m_artworkUploader  = new DiscordArtworkUploader(m_networkAccess.get(), this);
    m_coverArtResolver = new DiscordCoverArtResolver(m_networkAccess.get(), this);

    m_discordSettings = std::make_unique<DiscordSettings>(m_settings);

    m_clearOnPause = m_settings->value<Settings::Discord::ClearOnPause>();

    QObject::connect(m_player, &PlayerController::currentTrackChanged, this, [this] {
        m_positionSyncedTrackId = -1;
        resetArtwork();
        m_throttler->throttle();
        scheduleArtworkLoad();
    });
    QObject::connect(m_player, &PlayerController::positionChanged, this, &DiscordPlugin::positionChanged);
    QObject::connect(m_player, &PlayerController::positionMoved, m_throttler, &SignalThrottler::throttle);
    QObject::connect(m_player, &PlayerController::playStateChanged, this, [this](Player::PlayState state) {
        if(state == Player::PlayState::Stopped) {
            m_positionSyncedTrackId = -1;
            resetArtwork();
        }
        else {
            scheduleArtworkLoad();
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
    m_settings->subscribe<Settings::Discord::ArtworkEnabled>(this, [this]() {
        restartArtwork();
        m_throttler->throttle();
    });
    m_settings->subscribe<Settings::Discord::ArtworkSource>(this, [this]() {
        restartArtwork();
        m_throttler->throttle();
    });
    m_settings->subscribe<Settings::Discord::ArtworkRetention>(this, [this]() {
        restartArtwork(true);
        m_throttler->throttle();
    });
}

void DiscordPlugin::initialise(const GuiPluginContext& context)
{
    m_discordPage   = new DiscordPage(m_settings, this);
    m_coverProvider = new CoverProvider(context.coverRepository, this);
    m_coverProvider->setUsePlaceholder(false);

    QObject::connect(m_coverProvider, &CoverProvider::coverAdded, this, [this](const Track& track) {
        if(track == m_player->currentTrack()) {
            restartArtwork();
        }
    });

    restartArtwork();
}

void DiscordPlugin::shutdown()
{
    m_activityTask.reset();
    m_clientIdTask.reset();
    m_connectTask.reset();
    resetArtwork();
    startDisconnectTask();
}

void DiscordPlugin::timerEvent(QTimerEvent* event)
{
    if(event->timerId() == m_artworkLoadTimer.timerId()) {
        m_artworkLoadTimer.stop();
        startArtworkLoad();
        return;
    }
    if(event->timerId() == m_artworkRefreshTimer.timerId()) {
        m_artworkRefreshTimer.stop();
        m_artworkAttempted = false;
        startArtworkLoad();
        return;
    }
    QObject::timerEvent(event);
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

    if(m_artworkTrackKey == track.identityKey()) {
        data.largeImage = m_artworkUrl;
    }
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
                data.smallImage = data.largeImage.isEmpty() ? LogoPlay : LogoFooyin;
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
                data.smallImage = LogoPause;
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

void DiscordPlugin::toggleEnabled(bool enable)
{
    if(enable && !m_discordClient->isConnected()) {
        startConnectTask();
        scheduleArtworkLoad();
    }
    else {
        m_activityTask.reset();
        m_clientIdTask.reset();
        m_connectTask.reset();
        resetArtwork();
        startDisconnectTask();
    }
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

void DiscordPlugin::resetArtwork(bool clearCache)
{
    ++m_artworkGeneration;
    m_artworkAttempted = false;
    m_artworkTrackKey.clear();
    m_artworkUrl.clear();
    m_artworkLoadTimer.stop();
    m_artworkRefreshTimer.stop();

    m_artworkUploader->cancel();
    m_coverArtResolver->cancel();

    if(clearCache) {
        m_artworkUploader->clearCache();
        m_coverArtResolver->clearCache();
    }
}

void DiscordPlugin::scheduleArtworkLoad()
{
    if(m_artworkAttempted || !m_settings->value<Settings::Discord::DiscordEnabled>()
       || !m_settings->value<Settings::Discord::ArtworkEnabled>()
       || m_player->playState() == Player::PlayState::Stopped) {
        return;
    }

    m_artworkLoadTimer.start(ArtworkLoadDelayMs, this);
}

void DiscordPlugin::startArtworkLoad()
{
    m_artworkLoadTimer.stop();

    if(m_artworkAttempted || !m_artworkUploader || !m_coverArtResolver
       || !m_settings->value<Settings::Discord::DiscordEnabled>()
       || !m_settings->value<Settings::Discord::ArtworkEnabled>()) {
        return;
    }

    const Track track = m_player->currentTrack();
    if(!track.isValid() || m_player->playState() == Player::PlayState::Stopped) {
        return;
    }

    m_artworkAttempted       = true;
    m_artworkTrackKey        = track.identityKey();
    const quint64 generation = ++m_artworkGeneration;
    m_artworkUploader->cancel();
    m_coverArtResolver->cancel();

    tryArtworkSource(track, generation, 0);
}

void DiscordPlugin::tryArtworkSource(const Track& track, uint64_t generation, int attempt)
{
    if(generation != m_artworkGeneration) {
        return;
    }

    const auto mode
        = static_cast<Settings::Discord::ArtworkMode>(m_settings->value<Settings::Discord::ArtworkSource>());
    const auto provider = artworkProvider(mode, attempt);
    if(!provider) {
        setArtworkUrl({});
        return;
    }

    if(provider.value() == ArtworkProvider::Upload) {
        tryArtworkUpload(track, generation, attempt);
    }
    else {
        tryMusicBrainzArtwork(track, generation, attempt);
    }
}

void DiscordPlugin::tryArtworkUpload(const Track& track, uint64_t generation, int attempt)
{
    auto getCover = m_coverProvider->trackCoverFull(track, Track::Cover::Front);

    getCover.then(this, [this, track, generation, attempt](const QPixmap& cover) {
        if(generation != m_artworkGeneration) {
            return;
        }

        QByteArray artwork;
        QBuffer buffer{&artwork};
        if(cover.isNull() || !buffer.open(QIODevice::WriteOnly) || !cover.save(&buffer, "JPG", 85)) {
            tryArtworkSource(track, generation, attempt + 1);
            return;
        }

        const DiscordArtworkUploader::Config config{
            .endpoint       = QUrl{LitterboxEndpoint},
            .retentionHours = m_settings->value<Settings::Discord::ArtworkRetention>(),
        };

        m_artworkUploader->upload(artwork, config, this, [this, track, generation, attempt](const auto& result) {
            if(generation != m_artworkGeneration) {
                return;
            }
            if(!result) {
                tryArtworkSource(track, generation, attempt + 1);
                return;
            }
            setArtworkUrl(result->url.toString(), result->expiresAt);
        });
    });
}

void DiscordPlugin::tryMusicBrainzArtwork(const Track& track, uint64_t generation, int attempt)
{
    m_coverArtResolver->resolve(track, this, [this, track, generation, attempt](const auto& url) {
        if(generation != m_artworkGeneration) {
            return;
        }
        if(!url) {
            tryArtworkSource(track, generation, attempt + 1);
            return;
        }
        setArtworkUrl(url->toString());
    });
}

void DiscordPlugin::setArtworkUrl(const QString& url, const std::optional<QDateTime>& expiresAt)
{
    m_artworkUrl = url;
    m_artworkRefreshTimer.stop();

    if(expiresAt) {
        static constexpr auto RefreshMarginMs = ((5 * 60) - 10) * 1000;

        const auto refreshMs = static_cast<int>(QDateTime::currentDateTimeUtc().msecsTo(*expiresAt) - RefreshMarginMs);
        m_artworkRefreshTimer.start(std::max(refreshMs, 1000), this);
    }

    m_throttler->throttle();
}

void DiscordPlugin::restartArtwork(bool clearCache)
{
    resetArtwork(clearCache);
    scheduleArtworkLoad();
}
} // namespace Fooyin::Discord
