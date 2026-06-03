/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <luket@pm.me>
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

#include "scrobbler.h"

#include "services/lastfmservice.h"
#include "services/librefmservice.h"
#include "services/listenbrainzservice.h"
#include "settings/scrobblersettings.h"

#include <core/player/playercontroller.h>
#include <utils/settings/settingsmanager.h>

#include <QIODevice>

#include <algorithm>

using namespace Qt::StringLiterals;

constexpr auto NowPlayingRefreshIntervalMs  = 180000;
constexpr auto NowPlayingFinalRefreshLeadMs = 180000;
constexpr auto MinNowPlayingRefreshDelayMs  = 1000;
constexpr auto MinScrobbleTrackDurationMs   = 30000;
constexpr auto MaxScrobbleThresholdMs       = 240000;

namespace {
uint64_t scrobbleThresholdMs(uint64_t durationMs)
{
    return std::min<uint64_t>(durationMs / 2, MaxScrobbleThresholdMs);
}
} // namespace

namespace Fooyin::Scrobbler {
Scrobbler::Scrobbler(PlayerController* playerController, std::shared_ptr<NetworkAccessManager> network,
                     SettingsManager* settings)
    : m_playerController{playerController}
    , m_network{std::move(network)}
    , m_settings{settings}
    , m_scrobbledCurrentTrack{false}
{
    addDefaultServices();
    restoreServices();

    for(auto& service : m_services) {
        service->initialise();
        service->loadSession();
        service->resumePendingSubmissions();
    }

    QObject::connect(m_playerController, &PlayerController::currentTrackChanged, this, &Scrobbler::currentTrackChanged);
    QObject::connect(m_playerController, &PlayerController::positionChanged, this, &Scrobbler::updateScrobbleThreshold);
    QObject::connect(m_playerController, &PlayerController::playStateChanged, this, &Scrobbler::handlePlayStateChanged);
}

Scrobbler::~Scrobbler()
{
    saveServices();
}

std::vector<ScrobblerService*> Scrobbler::services() const
{
    std::vector<ScrobblerService*> services;
    std::ranges::transform(m_services, std::back_inserter(services), [](const auto& service) { return service.get(); });
    return services;
}

ScrobblerService* Scrobbler::service(const QString& name) const
{
    const auto serviceIt
        = std::ranges::find_if(m_services, [name](const auto& service) { return service->name() == name; });
    if(serviceIt != m_services.cend()) {
        return serviceIt->get();
    }
    return nullptr;
}

void Scrobbler::currentTrackChanged(const Track& track)
{
    m_scrobbledCurrentTrack = false;
    updateNowPlaying(track);
}

void Scrobbler::handlePlayStateChanged(Player::PlayState state, Player::PlayState previous)
{
    if(previous == Player::PlayState::Stopped && state == Player::PlayState::Playing) {
        m_scrobbledCurrentTrack = false;

        const Track track = m_playerController->currentTrack();
        for(auto& service : m_services) {
            service->restartScrobbleSession(track);
        }
    }

    updateNowPlayingTimer();
}

void Scrobbler::scrobble(const Track& track)
{
    for(auto& service : m_services) {
        if(service->isEnabled()) {
            service->scrobble(track);
        }
    }
}

bool Scrobbler::currentTrackReachedScrobbleThreshold() const
{
    const Track track = m_playerController->currentTrack();
    if(!track.isValid() || track.isRemote() || !track.hasArtists() || track.title().isEmpty()
       || track.duration() < MinScrobbleTrackDurationMs) {
        return false;
    }

    return m_playerController->currentTimeListened() >= scrobbleThresholdMs(track.duration());
}

void Scrobbler::updateScrobbleThreshold()
{
    if(m_scrobbledCurrentTrack || !currentTrackReachedScrobbleThreshold()) {
        return;
    }

    m_scrobbledCurrentTrack = true;
    scrobble(m_playerController->currentTrack());
}

std::unique_ptr<ScrobblerService> Scrobbler::createCustomService(const ServiceDetails& details)
{
    switch(details.customType) {
        case ServiceDetails::CustomType::AudioScrobbler:
            return std::make_unique<LibreFmService>(details, m_network.get(), m_settings);
        case ServiceDetails::CustomType::ListenBrainz:
            return std::make_unique<ListenBrainzService>(details, m_network.get(), m_settings);
        case ServiceDetails::CustomType::None:
            break;
    }

    return nullptr;
}

ScrobblerService* Scrobbler::addCustomService(const ServiceDetails& details, bool init)
{
    ScrobblerService* service{nullptr};

    switch(details.customType) {
        case ServiceDetails::CustomType::AudioScrobbler:
            service
                = m_services.emplace_back(std::make_unique<LibreFmService>(details, m_network.get(), m_settings)).get();
            break;
        case ServiceDetails::CustomType::ListenBrainz:
            service
                = m_services.emplace_back(std::make_unique<ListenBrainzService>(details, m_network.get(), m_settings))
                      .get();
            break;
        case ServiceDetails::CustomType::None:
            break;
    }

    if(service) {
        if(init) {
            service->initialise();
            service->loadSession();
        }
        return service;
    }

    return nullptr;
}

bool Scrobbler::removeCustomService(ScrobblerService* service)
{
    const auto serviceIt = std::ranges::find_if(m_services, [service](const auto& s) { return s.get() == service; });
    if(serviceIt != m_services.cend()) {
        serviceIt->get()->deleteSession();
        m_services.erase(serviceIt);
        return true;
    }
    return false;
}

void Scrobbler::saveCache()
{
    for(const auto& service : m_services) {
        service->saveCache();
    }
}

void Scrobbler::timerEvent(QTimerEvent* event)
{
    if(event->timerId() == m_nowPlayingTimer.timerId()) {
        m_nowPlayingTimer.stop();

        for(auto& service : m_services) {
            if(service->isEnabled()) {
                service->refreshNowPlaying();
            }
        }

        updateNowPlayingTimer();
    }

    QObject::timerEvent(event);
}

int Scrobbler::nextNowPlayingRefreshDelay() const
{
    const bool shouldRefresh = std::ranges::any_of(m_services, [](const auto& service) { return service->isEnabled(); })
                            && m_playerController->playState() == Player::PlayState::Playing
                            && m_playerController->currentTrack().isValid();
    if(!shouldRefresh) {
        return 0;
    }

    const uint64_t durationMs = m_playerController->currentTrack().duration();
    if(durationMs == 0) {
        return 0;
    }

    const uint64_t positionMs = std::min<uint64_t>(m_playerController->currentPosition(), durationMs);
    if(durationMs <= positionMs + NowPlayingFinalRefreshLeadMs) {
        return 0;
    }

    // Send final update ~3 minutes before the end of the track
    const uint64_t untilFinalUpdateMs = durationMs - positionMs - NowPlayingFinalRefreshLeadMs;
    const uint64_t delayMs            = std::min<uint64_t>(NowPlayingRefreshIntervalMs, untilFinalUpdateMs);
    return static_cast<int>(std::max<uint64_t>(delayMs, MinNowPlayingRefreshDelayMs));
}

void Scrobbler::updateNowPlaying(const Track& track)
{
    for(auto& service : m_services) {
        if(service->isEnabled()) {
            service->updateNowPlaying(track);
        }
    }

    updateNowPlayingTimer(true);
}

void Scrobbler::updateNowPlayingTimer(const bool reset)
{
    const int delay = nextNowPlayingRefreshDelay();
    if(delay <= 0) {
        if(m_nowPlayingTimer.isActive()) {
            m_nowPlayingTimer.stop();
        }
        return;
    }

    if(reset && m_nowPlayingTimer.isActive()) {
        m_nowPlayingTimer.stop();
    }

    if(!m_nowPlayingTimer.isActive()) {
        m_nowPlayingTimer.start(delay, this);
    }
}

void Scrobbler::addDefaultServices()
{
    auto addService = [this]<class T>(const QString& name) {
        m_services.emplace_back(std::make_unique<T>(ServiceDetails{.name       = name,
                                                                   .url        = {},
                                                                   .token      = {},
                                                                   .customType = ServiceDetails::CustomType::None,
                                                                   .isEnabled  = true},
                                                    m_network.get(), m_settings));
    };

    addService.template operator()<LastFmService>(u"LastFM"_s);
    addService.template operator()<LibreFmService>(u"LibreFM"_s);
    addService.template operator()<ListenBrainzService>(u"ListenBrainz"_s);
}

void Scrobbler::saveServices()
{
    QList<ServiceDetails> services;

    for(const auto& service : m_services) {
        services.push_back(service->details());
    }

    QByteArray data;
    QDataStream stream{&data, QIODevice::WriteOnly};

    stream << services;

    m_settings->set<Settings::Scrobbler::ServicesData>(data);
}

void Scrobbler::restoreServices()
{
    QByteArray data = m_settings->value<Settings::Scrobbler::ServicesData>();
    QDataStream stream{&data, QIODevice::ReadOnly};

    QList<ServiceDetails> services;

    stream >> services;

    for(const auto& details : std::as_const(services)) {
        if(details.isCustom()) {
            addCustomService(details, false);
        }
        else {
            auto serviceIt = std::ranges::find_if(
                m_services, [&details](const auto& service) { return service->name() == details.name; });
            if(serviceIt != m_services.cend()) {
                serviceIt->get()->updateDetails(details);
            }
        }
    }
}
} // namespace Fooyin::Scrobbler

#include "moc_scrobbler.cpp"
