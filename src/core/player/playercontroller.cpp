/*
 * Fooyin
 * Copyright © 2022, Luke Taylor <LukeT1@proton.me>
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

#include "core/playlist/playlisthandler.h"
#include <core/player/playercontroller.h>

#include <core/player/playbackqueue.h>

#include <core/coresettings.h>
#include <core/track.h>
#include <utils/settings/settingsmanager.h>

namespace Fooyin {
class PlayerControllerPrivate
{
public:
    PlayerControllerPrivate(PlayerController* self, SettingsManager* settings)
        : m_self{self}
        , m_settings{settings}
        , m_playMode{static_cast<Playlist::PlayModes>(m_settings->value<Settings::Core::PlayMode>())}
    { }

    void changeTrack(const PlaylistTrack& track)
    {
        m_currentTrack  = track;
        m_totalDuration = m_currentTrack.track.duration();
        m_position      = 0;
        m_timeListened  = 0;
        m_counted       = false;

        if(m_totalDuration > 200) {
            m_playedThreshold = static_cast<uint64_t>(static_cast<double>(m_totalDuration - 200)
                                                      * m_settings->value<Settings::Core::PlayedThreshold>());
        }

        m_scheduledTrack = {};
    }

    void loadScheduledTrack() const
    {
        if(m_playlistHandler) {
            if(auto* playlist = m_playlistHandler->playlistById(m_scheduledTrack.playlistId)) {
                m_playlistHandler->changeActivePlaylist(playlist);
                playlist->changeCurrentIndex(m_scheduledTrack.indexInPlaylist);
            }
        }
        m_self->changeCurrentTrack(m_scheduledTrack);
    }

    void updatePosition(uint64_t pos)
    {
        if(!m_seeking && pos > m_position) {
            m_timeListened += (pos - m_position);
        }
        m_seeking = false;

        m_position = pos;
    }

    PlayerController* m_self;
    SettingsManager* m_settings;
    PlaylistHandler* m_playlistHandler{nullptr};

    Player::PlayState m_playStatus{Player::PlayState::Stopped};
    Playlist::PlayModes m_playMode;

    PlaylistTrack m_currentTrack;
    uint64_t m_totalDuration{0};
    uint64_t m_position{0};
    int m_bitrate{0};

    uint64_t m_timeListened{0};
    uint64_t m_playedThreshold{0};

    bool m_seeking{false};
    bool m_counted{false};
    bool m_isQueueTrack{false};

    PlaybackQueue m_queue;
    PlaylistTrack m_scheduledTrack;
};

PlayerController::PlayerController(SettingsManager* settings, QObject* parent)
    : QObject{parent}
    , p{std::make_unique<PlayerControllerPrivate>(this, settings)}
{
    settings->subscribe<Settings::Core::PlayMode>(this, [this]() {
        const auto mode = static_cast<Playlist::PlayModes>(p->m_settings->value<Settings::Core::PlayMode>());
        if(std::exchange(p->m_playMode, mode) != mode) {
            emit playModeChanged(mode);
        }
    });
}

PlayerController::~PlayerController() = default;

void PlayerController::reset()
{
    stop();
    p->m_currentTrack = {};
    p->m_playStatus   = Player::PlayState::Stopped;
    p->m_position     = 0;
    p->m_bitrate      = 0;
}

void PlayerController::play()
{
    if(!p->m_currentTrack.isValid()) {
        if(p->m_scheduledTrack.isValid()) {
            p->loadScheduledTrack();
        }
        else if(!p->m_queue.empty()) {
            const auto& nextTrack = p->m_queue.nextTrack();
            Playlist* playlist{nullptr};

            if(p->m_playlistHandler) {
                playlist = p->m_playlistHandler->activePlaylist();
                if(playlist && playlist->id() != nextTrack.playlistId && nextTrack.playlistId.isValid()) {
                    p->m_playlistHandler->changeActivePlaylist(nextTrack.playlistId);
                }

                if(p->m_queue.trackCount() == 1 && p->m_settings->value<Settings::Core::FollowPlaybackQueue>()) {
                    const auto index = nextTrack.indexInPlaylist;
                    if(playlist->id() != nextTrack.playlistId) {
                        playlist = p->m_playlistHandler->playlistById(nextTrack.playlistId);
                    }

                    if(playlist && index != -1) {
                        playlist->changeCurrentIndex(index);
                    }
                }
            }
            changeCurrentTrack(p->m_queue.nextTrackChange());
            emit tracksDequeued({p->m_currentTrack});
        }
        else if(p->m_playlistHandler) {
            const PlaylistTrack track = p->m_playlistHandler->changeNextTrack();
            changeCurrentTrack(track);
        }
    }

    if(p->m_currentTrack.isValid()) {
        if(std::exchange(p->m_playStatus, Player::PlayState::Playing) != Player::PlayState::Playing) {
            emit playStateChanged(p->m_playStatus);
        }
    }
    else {
        p->m_currentTrack = {};
    }
}

void PlayerController::playPause()
{
    switch(p->m_playStatus) {
        case(Player::PlayState::Playing):
            pause();
            break;
        case(Player::PlayState::Paused):
        case(Player::PlayState::Stopped):
            play();
            break;
        default:
            break;
    }
}

void PlayerController::pause()
{
    if(std::exchange(p->m_playStatus, Player::PlayState::Paused) != p->m_playStatus) {
        emit playStateChanged(p->m_playStatus);
    }
}

void PlayerController::previous()
{
    if(p->m_settings->value<Settings::Core::RewindPreviousTrack>() && currentPosition() > 5000) {
        seek(0);
        return;
    }

    // Temporarily disable repeating track when user clicks 'previous'.
    const auto playMode = p->m_playMode;
    p->m_playMode &= ~Playlist::RepeatTrack;

    if(p->m_playlistHandler) {
        const PlaylistTrack track = p->m_playlistHandler->changePreviousTrack();
        changeCurrentTrack(track);
    }
    else {
        p->m_currentTrack = {};
    }

    if(p->m_currentTrack.isValid()) {
        play();
    }

    p->m_playMode = playMode;
}

void PlayerController::next()
{
    // Temporarily disable repeating track when user clicks 'next'.
    const auto playMode = p->m_playMode;
    p->m_playMode &= ~Playlist::RepeatTrack;
    nextAuto();
    p->m_playMode = playMode;
}

void PlayerController::nextAuto()
{
    if(p->m_settings->value<Settings::Core::StopAfterCurrent>()) {
        p->m_settings->set<Settings::Core::StopAfterCurrent>(false);
        reset();
        return;
    }

    if(p->m_scheduledTrack.isValid()) {
        p->loadScheduledTrack();
    }
    else if(p->m_queue.empty() && p->m_playlistHandler) {
        p->m_isQueueTrack         = false;
        const PlaylistTrack track = p->m_playlistHandler->changeNextTrack();
        changeCurrentTrack(track);
    }
    else {
        p->m_currentTrack = {};
        p->m_isQueueTrack = true;
        play();
    }

    if(p->m_currentTrack.isValid()) {
        play();
    }
}

void PlayerController::stop()
{
    if(std::exchange(p->m_playStatus, Player::PlayState::Stopped) != p->m_playStatus) {
        p->m_position = 0;

        emit playStateChanged(p->m_playStatus);
        emit positionChanged(0);
    }
}

void PlayerController::setCurrentPosition(uint64_t ms)
{
    p->updatePosition(ms);

    if(!p->m_counted && p->m_timeListened >= p->m_playedThreshold) {
        p->m_counted = true;
        if(p->m_currentTrack.isValid()) {
            emit trackPlayed(p->m_currentTrack.track);
        }
    }

    emit positionChanged(ms);
}

void PlayerController::setBitrate(int bitrate)
{
    p->m_bitrate = bitrate;
}

void PlayerController::changeCurrentTrack(const Track& track)
{
    changeCurrentTrack(PlaylistTrack{.track = track, .playlistId = {}});
}

void PlayerController::changeCurrentTrack(const PlaylistTrack& track)
{
    if(!track.isValid()) {
        reset();
        stop();
    }

    p->changeTrack(track);

    p->m_settings->set<Settings::Core::ActiveTrack>(QVariant::fromValue(p->m_currentTrack.track));
    p->m_settings->set<Settings::Core::ActiveTrackId>(p->m_currentTrack.track.id());

    emit currentTrackChanged(p->m_currentTrack.track);
    emit playlistTrackChanged(p->m_currentTrack);
}

void PlayerController::updateCurrentTrackPlaylist(const UId& playlistId)
{
    if(std::exchange(p->m_currentTrack.playlistId, playlistId) != playlistId) {
        emit playlistTrackChanged(p->m_currentTrack);
    }
}

void PlayerController::updateCurrentTrackIndex(int index)
{
    if(std::exchange(p->m_currentTrack.indexInPlaylist, index) != index) {
        emit playlistTrackChanged(p->m_currentTrack);
    }
}

void PlayerController::scheduleNextTrack(const PlaylistTrack& track)
{
    p->m_scheduledTrack = track;
}

Track PlayerController::upcomingTrack() const
{
    if(p->m_settings->value<Settings::Core::StopAfterCurrent>()) {
        return {};
    }

    if(p->m_scheduledTrack.isValid()) {
        return p->m_scheduledTrack.track;
    }

    if(p->m_queue.empty() && p->m_playlistHandler) {
        return p->m_playlistHandler->nextTrack().track;
    }

    return p->m_queue.nextTrack().track;
}

PlaybackQueue PlayerController::playbackQueue() const
{
    return p->m_queue;
}

void PlayerController::setPlayMode(Playlist::PlayModes mode)
{
    p->m_settings->set<Settings::Core::PlayMode>(static_cast<int>(mode));
}

void PlayerController::seek(uint64_t ms)
{
    if(p->m_totalDuration < 100) {
        return;
    }

    if(ms >= p->m_totalDuration - 100) {
        next();
        return;
    }

    if(std::exchange(p->m_position, ms) != ms) {
        p->m_seeking = true;
        emit positionMoved(ms);
    }
}

void PlayerController::seekForward(uint64_t delta)
{
    seek(p->m_position + delta);
}

void PlayerController::seekBackward(uint64_t delta)
{
    if(delta > p->m_position) {
        seek(0);
    }
    else {
        seek(p->m_position - delta);
    }
}

Player::PlayState PlayerController::playState() const
{
    return p->m_playStatus;
}

Playlist::PlayModes PlayerController::playMode() const
{
    return p->m_playMode;
}

uint64_t PlayerController::currentPosition() const
{
    return p->m_position;
}

int PlayerController::bitrate() const
{
    return p->m_bitrate;
}

Track PlayerController::currentTrack() const
{
    return p->m_currentTrack.track;
}

int PlayerController::currentTrackId() const
{
    return p->m_currentTrack.isValid() ? p->m_currentTrack.track.id() : -1;
}

bool PlayerController::currentIsQueueTrack() const
{
    return p->m_isQueueTrack;
}

PlaylistTrack PlayerController::currentPlaylistTrack() const
{
    return p->m_currentTrack;
}

void PlayerController::queueTrack(const Track& track)
{
    queueTrack(PlaylistTrack{track, {}});
}

void PlayerController::queueTrack(const PlaylistTrack& track)
{
    queueTracks({track});
}

void PlayerController::queueTracks(const TrackList& tracks)
{
    if(tracks.empty()) {
        return;
    }

    QueueTracks tracksToQueue;
    for(const Track& track : tracks) {
        tracksToQueue.emplace_back(track);
    }

    queueTracks(tracksToQueue);
}

void PlayerController::queueTracks(const QueueTracks& tracks)
{
    if(tracks.empty()) {
        return;
    }

    QueueTracks tracksToAdd{tracks};

    const int freeTracks = p->m_queue.freeSpace();
    if(std::cmp_greater_equal(tracks.size(), freeTracks)) {
        tracksToAdd = {tracks.begin(), tracks.begin() + freeTracks};
    }

    const int index = p->m_queue.trackCount();

    p->m_queue.addTracks(tracksToAdd);
    emit tracksQueued(tracksToAdd, index);
}

void PlayerController::queueTrackNext(const Track& track)
{
    queueTrackNext(PlaylistTrack{track, {}});
}

void PlayerController::queueTrackNext(const PlaylistTrack& track)
{
    queueTracksNext({track});
}

void PlayerController::queueTracksNext(const TrackList& tracks)
{
    if(tracks.empty()) {
        return;
    }

    QueueTracks tracksToQueue;
    for(const Track& track : tracks) {
        tracksToQueue.emplace_back(track);
    }

    queueTracksNext(tracksToQueue);
}

void PlayerController::queueTracksNext(const QueueTracks& tracks)
{
    if(tracks.empty()) {
        return;
    }

    QueueTracks tracksToAdd{tracks};

    const int freeTracks = p->m_queue.freeSpace();
    if(std::cmp_greater_equal(tracks.size(), freeTracks)) {
        tracksToAdd = {tracks.begin(), tracks.begin() + freeTracks};
    }

    p->m_queue.addTracks(tracksToAdd, 0);
    emit trackQueueChanged({}, p->m_queue.tracks());
}

void PlayerController::dequeueTrack(const Track& track)
{
    dequeueTrack(PlaylistTrack{track, {}});
}

void PlayerController::dequeueTrack(const PlaylistTrack& track)
{
    dequeueTracks({track});
}

void PlayerController::dequeueTracks(const TrackList& tracks)
{
    if(tracks.empty()) {
        return;
    }

    QueueTracks tracksToDequeue;
    for(const Track& track : tracks) {
        tracksToDequeue.emplace_back(track);
    }

    dequeueTracks(tracksToDequeue);
}

void PlayerController::dequeueTracks(const QueueTracks& tracks)
{
    if(tracks.empty()) {
        return;
    }

    const auto removedTracks = p->m_queue.removeTracks(tracks);
    if(!removedTracks.empty()) {
        emit tracksDequeued(removedTracks);
    }
}

void PlayerController::dequeueTracks(const std::vector<int>& indexes)
{
    if(indexes.empty()) {
        return;
    }

    PlaylistIndexes dequeuedIndexes;

    std::vector<int> sortedIndexes{indexes};
    std::sort(sortedIndexes.rbegin(), sortedIndexes.rend());

    auto tracks      = p->m_queue.tracks();
    const auto count = static_cast<int>(tracks.size());
    for(const int index : sortedIndexes) {
        if(index >= 0 && index < count) {
            const auto track = p->m_queue.track(index);
            dequeuedIndexes[track.playlistId].emplace_back(track.indexInPlaylist);
            tracks.erase(tracks.begin() + index);
        }
    }

    p->m_queue.replaceTracks(tracks);

    if(!dequeuedIndexes.empty()) {
        emit trackIndexesDequeued(dequeuedIndexes);
    }
}

void PlayerController::replaceTracks(const TrackList& tracks)
{
    QueueTracks tracksToQueue;
    for(const Track& track : tracks) {
        tracksToQueue.emplace_back(track);
    }

    replaceTracks(tracksToQueue);
}

void PlayerController::replaceTracks(const QueueTracks& tracks)
{
    QueueTracks removed;

    const auto currentTracks = p->m_queue.tracks();
    const std::set<PlaylistTrack> newTracks{tracks.cbegin(), tracks.cend()};

    std::ranges::copy_if(currentTracks, std::back_inserter(removed),
                         [&newTracks](const PlaylistTrack& oldTrack) { return !newTracks.contains(oldTrack); });

    p->m_queue.replaceTracks(tracks);

    emit trackQueueChanged(removed, tracks);
}

void PlayerController::clearPlaylistQueue(const UId& playlistId)
{
    const auto removedTracks = p->m_queue.removePlaylistTracks(playlistId);
    if(!removedTracks.empty()) {
        emit tracksDequeued(removedTracks);
    }
}

void PlayerController::clearQueue()
{
    const auto removedTracks = p->m_queue.tracks();
    p->m_queue.clear();
    if(!removedTracks.empty()) {
        emit tracksDequeued(removedTracks);
    }
}

void PlayerController::setPlaylistHandler(PlaylistHandler* handler)
{
    p->m_playlistHandler = handler;
}
} // namespace Fooyin

#include "core/player/moc_playercontroller.cpp"
