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

#include <core/constants.h>
#include <core/library/musiclibrary.h>
#include <core/player/playercontroller.h>
#include <utils/settings/settingsmanager.h>

#include <QDateTime>
#include <QIODevice>

#include <algorithm>
#include <ranges>

using namespace Qt::StringLiterals;

constexpr auto NowPlayingRefreshIntervalMs  = 180000;
constexpr auto NowPlayingFinalRefreshLeadMs = 180000;
constexpr auto MinNowPlayingRefreshDelayMs  = 1000;
constexpr auto TrackStatsSyncIntervalMs     = 60 * 60 * 1000;
constexpr auto RecentLovedChangeIntervalMs  = 60 * 1000;

namespace Fooyin::Scrobbler {
namespace {
QString trackSyncKey(const Track& track)
{
    if(!track.hash().isEmpty()) {
        return track.hash();
    }
    return QString::number(track.id()) + u':' + track.uniqueFilepath();
}

QString trackStatsSyncKey(const ScrobblerService* service, const Fooyin::Track& track)
{
    return service->name() + QLatin1StringView{Constants::UnitSeparator} + trackSyncKey(track);
}
} // namespace

Scrobbler::Scrobbler(PlayerController* playerController, MusicLibrary* library,
                     std::shared_ptr<NetworkAccessManager> network, SettingsManager* settings)
    : m_playerController{playerController}
    , m_library{library}
    , m_network{std::move(network)}
    , m_settings{settings}
{
    addDefaultServices();
    restoreServices();

    for(auto& service : m_services) {
        setupService(service.get());
        service->initialise();
        service->loadSession();
        service->resumePendingSubmissions();
    }

    QObject::connect(m_playerController, &PlayerController::currentTrackChanged, this, &Scrobbler::updateNowPlaying);
    QObject::connect(m_playerController, &PlayerController::trackPlayed, this, &Scrobbler::scrobble);
    QObject::connect(m_playerController, &PlayerController::playStateChanged, this, &Scrobbler::handlePlayStateChanged);
    QObject::connect(m_library, &MusicLibrary::tracksStatsChanged, this, &Scrobbler::handleTrackStatsChanged);

    m_settings->subscribe<Settings::Scrobbler::SyncPlaybackStats>(this, [this]() {
        m_lastTrackStatsSync.clear();
        m_remoteLovedStates.clear();
        m_selectedRemoteLoved.clear();
    });
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

void Scrobbler::scrobble(const Track& track)
{
    for(auto& service : m_services) {
        if(service->isEnabled()) {
            service->scrobble(track);
        }
    }
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
        setupService(service);
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

void Scrobbler::handlePlayStateChanged(Player::PlayState state, Player::PlayState previous)
{
    if(previous == Player::PlayState::Stopped && state == Player::PlayState::Playing) {
        const Track track = m_playerController->currentTrack();
        for(auto& service : m_services) {
            service->restartScrobbleSession(track);
        }
    }

    updateNowPlayingTimer();
}

void Scrobbler::handleTrackStatsChanged(const TrackList& tracks, const Track::Stats stats)
{
    if(!stats.testFlag(Track::Stat::Loved)) {
        return;
    }

    const auto now = static_cast<int64_t>(QDateTime::currentMSecsSinceEpoch());
    pruneTrackStatsSyncState(now);

    for(const Track& track : tracks) {
        const QString key = trackSyncKey(track);
        if(consumePendingLovedChange(key, track.isLoved())) {
            continue;
        }

        m_selectedRemoteLoved.erase(key);
        m_recentLovedChanges.insert_or_assign(key, now);

        for(auto& service : m_services) {
            if(!service->isEnabled() || !service->supportsLoved()) {
                continue;
            }
            service->updateLoved(track);
        }
    }
}

void Scrobbler::handleFetchedTrackStats(const RemoteTrackStats& stats)
{
    auto* service = qobject_cast<ScrobblerService*>(sender());
    if(!service) {
        return;
    }

    const QString trackKey = trackSyncKey(stats.track);
    m_lastTrackStatsSync.insert_or_assign(trackStatsSyncKey(service, stats.track), QDateTime::currentMSecsSinceEpoch());

    if(!service->isEnabled() || !service->details().syncPlaybackStats
       || !m_settings->value<Settings::Scrobbler::SyncPlaybackStats>()) {
        return;
    }

    Track track = stats.track.id() >= 0 ? m_library->trackForId(stats.track.id()) : stats.track;
    if(!track.isValid()) {
        return;
    }

    Track::Stats changedStats;
    if(stats.playCount && *stats.playCount > track.playCount()) {
        track.setPlayCount(*stats.playCount);
        changedStats |= Track::Stat::Playcount;
    }

    if(stats.loved && stats.playCount) {
        const auto now = static_cast<int64_t>(QDateTime::currentMSecsSinceEpoch());
        m_remoteLovedStates[trackKey].insert_or_assign(service->name(), RemoteLovedState{.loved     = *stats.loved,
                                                                                         .playCount = *stats.playCount,
                                                                                         .serviceName = service->name(),
                                                                                         .timestamp   = now});
        applyRemoteLovedState(track, trackKey, changedStats, now);
    }

    if(changedStats != Track::Stat::None) {
        m_library->updateTrackStats(track, changedStats);
    }
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

bool Scrobbler::consumePendingLovedChange(const QString& trackKey, const bool loved)
{
    const auto pending = m_pendingLovedChanges.find(trackKey);
    if(pending == m_pendingLovedChanges.end()) {
        return false;
    }

    auto& changes    = pending->second;
    const auto match = std::ranges::find(changes, loved, &PendingImportedLovedChange::loved);
    if(match == changes.end()) {
        return false;
    }

    changes.erase(match);
    if(changes.empty()) {
        m_pendingLovedChanges.erase(pending);
    }
    return true;
}

std::optional<Scrobbler::RemoteLovedState> Scrobbler::preferredRemoteLovedState(const QString& trackKey) const
{
    const auto states = m_remoteLovedStates.find(trackKey);
    if(states == m_remoteLovedStates.cend()) {
        return {};
    }

    std::optional<RemoteLovedState> selected;
    for(const auto& candidate : states->second | std::views::values) {
        const auto* candidateService = service(candidate.serviceName);
        if(!candidateService || !candidateService->isEnabled() || !candidateService->isAuthenticated()
           || !candidateService->details().syncPlaybackStats) {
            continue;
        }
        if(!selected || candidate.playCount > selected->playCount
           || (candidate.playCount == selected->playCount && candidate.serviceName < selected->serviceName)) {
            selected = candidate;
        }
    }
    return selected;
}

void Scrobbler::applyRemoteLovedState(Track& track, const QString& trackKey, Track::Stats& changedStats,
                                      const int64_t now)
{
    const auto selected = preferredRemoteLovedState(trackKey);
    if(!selected) {
        return;
    }

    const auto recentChange = m_recentLovedChanges.find(trackKey);
    if(recentChange != m_recentLovedChanges.end() && now - recentChange->second < RecentLovedChangeIntervalMs) {
        return;
    }

    const bool hasPendingChange = std::ranges::any_of(m_services, [&track](const auto& candidateService) {
        return candidateService->isEnabled() && candidateService->hasPendingLoved(track);
    });
    if(hasPendingChange) {
        return;
    }

    const auto previous = m_selectedRemoteLoved.find(trackKey);
    const bool selectionChanged
        = previous == m_selectedRemoteLoved.cend() || previous->second.serviceName != selected->serviceName
       || previous->second.playCount != selected->playCount || previous->second.loved != selected->loved;
    // The previous winner may still be queued for a database write, so a new winner must always be queued after it
    const bool mayOverrideQueuedImport = previous != m_selectedRemoteLoved.cend() && selectionChanged;

    m_selectedRemoteLoved.insert_or_assign(trackKey, *selected);

    if(selectionChanged && (mayOverrideQueuedImport || track.isLoved() != selected->loved)) {
        track.setLoved(selected->loved);
        changedStats |= Track::Stat::Loved;
        m_pendingLovedChanges[trackKey].push_back({.loved = selected->loved, .timestamp = now});
    }
}

void Scrobbler::pruneTrackStatsSyncState(const int64_t now)
{
    std::erase_if(m_lastTrackStatsSync,
                  [now](const auto& item) { return now - item.second >= TrackStatsSyncIntervalMs; });
    std::erase_if(m_recentLovedChanges,
                  [now](const auto& item) { return now - item.second >= RecentLovedChangeIntervalMs; });

    for(auto& changes : m_pendingLovedChanges | std::views::values) {
        std::erase_if(changes,
                      [now](const auto& change) { return now - change.timestamp >= RecentLovedChangeIntervalMs; });
    }
    std::erase_if(m_pendingLovedChanges, [](const auto& item) { return item.second.empty(); });

    for(auto& states : m_remoteLovedStates | std::views::values) {
        std::erase_if(states,
                      [now](const auto& item) { return now - item.second.timestamp >= TrackStatsSyncIntervalMs; });
    }
    std::erase_if(m_remoteLovedStates, [this](const auto& item) {
        if(item.second.empty()) {
            m_selectedRemoteLoved.erase(item.first);
            return true;
        }
        return false;
    });
}

void Scrobbler::updateNowPlaying(const Track& track)
{
    const auto now = static_cast<int64_t>(QDateTime::currentMSecsSinceEpoch());
    pruneTrackStatsSyncState(now);

    for(auto& service : m_services) {
        if(service->isEnabled()) {
            service->updateNowPlaying(track);

            if(m_settings->value<Settings::Scrobbler::SyncPlaybackStats>() && service->supportsTrackStatsSync()
               && service->details().syncPlaybackStats && service->isAuthenticated()) {
                const QString key   = trackStatsSyncKey(service.get(), track);
                const auto lastSync = m_lastTrackStatsSync.find(key);
                if(lastSync == m_lastTrackStatsSync.cend() || now - lastSync->second >= TrackStatsSyncIntervalMs) {
                    service->fetchTrackStats(track);
                }
            }
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

void Scrobbler::setupService(ScrobblerService* service)
{
    QObject::connect(service, &ScrobblerService::trackStatsFetched, this, &Scrobbler::handleFetchedTrackStats,
                     Qt::UniqueConnection);
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
