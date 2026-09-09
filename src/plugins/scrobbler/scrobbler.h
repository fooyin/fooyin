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

#pragma once

#include "services/scrobblerservice.h"
#include "services/servicedetails.h"

#include <QBasicTimer>
#include <QTimerEvent>
#include <core/player/playerdefs.h>

#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

class QString;

namespace Fooyin {
class NetworkAccessManager;
class MusicLibrary;
class PlayerController;
class SettingsManager;

namespace Scrobbler {
struct ServiceDetails;

class Scrobbler : public QObject
{
    Q_OBJECT

public:
    Scrobbler(PlayerController* playerController, MusicLibrary* library, std::shared_ptr<NetworkAccessManager> network,
              SettingsManager* settings);
    ~Scrobbler() override;

    [[nodiscard]] std::vector<ScrobblerService*> services() const;
    [[nodiscard]] ScrobblerService* service(const QString& name) const;

    void scrobble(const Track& track);

    std::unique_ptr<ScrobblerService> createCustomService(const ServiceDetails& details);
    ScrobblerService* addCustomService(const ServiceDetails& details, bool init = true);
    bool removeCustomService(ScrobblerService* service);

    void saveCache();

protected:
    void timerEvent(QTimerEvent* event) override;

private:
    struct PendingImportedLovedChange
    {
        bool loved{false};
        int64_t timestamp{0};
    };

    struct RemoteLovedState
    {
        bool loved{false};
        int playCount{-1};
        QString serviceName;
        int64_t timestamp{0};
    };

    void handlePlayStateChanged(Player::PlayState state, Player::PlayState previous);
    void handleTrackStatsChanged(const TrackList& tracks, Track::Stats stats);
    void handleFetchedTrackStats(const RemoteTrackStats& stats);

    [[nodiscard]] int nextNowPlayingRefreshDelay() const;
    bool consumePendingLovedChange(const QString& trackKey, bool loved);
    [[nodiscard]] std::optional<RemoteLovedState> preferredRemoteLovedState(const QString& trackKey) const;
    void applyRemoteLovedState(Track& track, const QString& trackKey, Track::Stats& changedStats, int64_t now);
    void pruneTrackStatsSyncState(int64_t now);
    void updateNowPlaying(const Track& track);
    void updateNowPlayingTimer(bool reset = false);
    void setupService(ScrobblerService* service);

    void addDefaultServices();
    void saveServices();
    void restoreServices();

    PlayerController* m_playerController;
    MusicLibrary* m_library;
    std::shared_ptr<NetworkAccessManager> m_network;
    SettingsManager* m_settings;

    std::vector<std::unique_ptr<ScrobblerService>> m_services;
    std::unordered_map<QString, int64_t> m_lastTrackStatsSync;
    std::unordered_map<QString, int64_t> m_recentLovedChanges;
    std::unordered_map<QString, std::vector<PendingImportedLovedChange>> m_pendingLovedChanges;
    std::unordered_map<QString, std::unordered_map<QString, RemoteLovedState>> m_remoteLovedStates;
    std::unordered_map<QString, RemoteLovedState> m_selectedRemoteLoved;
    QBasicTimer m_nowPlayingTimer;
};
} // namespace Scrobbler
} // namespace Fooyin
