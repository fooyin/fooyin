/*
 * Fooyin
 * Copyright © 2022, Luke Taylor <luket@pm.me>
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

#include <core/player/playercontroller.h>

#include "internalcoresettings.h"
#include "playbackcursor.h"
#include "playbackordernavigator.h"
#include "playbackprogresstracker.h"
#include "playbacksession.h"
#include "playbackstatestore.h"

#include <core/coresettings.h>
#include <core/player/playbackqueue.h>
#include <core/playlist/playlisthandler.h>
#include <core/track.h>
#include <utils/helpers.h>
#include <utils/settings/settingsmanager.h>

#include <QLoggingCategory>

#include <optional>

Q_LOGGING_CATEGORY(PLAYER_CONTROLLER, "fy.playercontroller")

namespace Fooyin {
namespace {
std::optional<PlaylistTrack> remapPlaylistTrackReference(PlaylistHandler* playlistHandler, const PlaylistTrack& track)
{
    if(!track.playlistId.isValid()) {
        return {};
    }

    auto* playlist = playlistHandler->playlistById(track.playlistId);
    if(!playlist) {
        return {};
    }

    if(track.entryId.isValid()) {
        if(const auto playlistTrack = playlist->playlistTrack(track.entryId)) {
            return playlistTrack;
        }
    }

    if(track.indexInPlaylist >= 0) {
        if(const auto playlistTrack = playlist->playlistTrack(track.indexInPlaylist);
           playlistTrack && playlistTrack->track.sameIdentityAs(track.track)) {
            return playlistTrack;
        }
    }

    return {};
}
} // namespace

class PlayerControllerPrivate
{
public:
    using RequestedTrack = PlaybackOrderNavigator::RequestedTrack;

    struct TransportAction
    {
        enum class Type : uint8_t
        {
            None = 0,
            Stop,
            ResetAndStop,
            AdvancePlaybackPositionAndStop,
            StopAtBoundary,
            KeepCurrentTrack,
            RequestSelection,
            RestartAtBoundary,
            ClearCurrentTrack,
        };

        Type type{Type::None};
        PlaybackCursor::BoundaryStop boundary{PlaybackCursor::BoundaryStop::None};
        PlaybackOrderNavigator::RestartTarget restartTarget{PlaybackOrderNavigator::RestartTarget::None};
        std::optional<RequestedTrack> selection;
    };

    PlayerControllerPrivate(PlayerController* self, SettingsManager* settings, PlaylistHandler* playlistHandler);

    [[nodiscard]] uint64_t nextPlaybackItemId();
    bool updateBitrate(int bitrate);

    void setStopAfterCurrentArmed(bool armed);
    void queueStopAfterCurrentReset();
    void finishPendingStopAfterCurrentReset();
    void clearStopAfterCurrentForManualSkip();
    [[nodiscard]] bool trackEndAutoTransitionsEnabled() const;
    [[nodiscard]] bool canRewindCurrentTrack() const;
    [[nodiscard]] bool canPerformPrevious() const;
    [[nodiscard]] Playlist* targetPlaybackPlaylist() const;
    [[nodiscard]] int targetPlaybackIndex(Playlist* playlist) const;

    [[nodiscard]] Player::UpcomingTrack resolveUpcomingTrack() const;
    void emitUpcomingTrackChangedIfNeeded();

    void requestTrackChange(const Player::TrackChangeRequest& request);
    bool updatePlaystate(Player::PlayState state);
    void restartCurrentTrackProgressIfNeeded();
    void restoreRemoteCurrentTrackFromPlaylist();
    bool enterStoppedState(bool requestTransportStop);
    void emitPositionSignals(const PlaybackProgressTracker::PositionUpdate& update);
    void syncCommittedPlaylistTrack() const;
    void updateCurrentTrackPlaylist(const UId& playlistId);
    void updateCurrentTrackEntry(const UId& entryId);
    void updateCurrentTrackIndex(int index);
    void syncPlaylistTrackState(const UId& playlistId);
    void remapPlaylistReferences(const UId& fromPlaylistId, const UId& toPlaylistId);

    bool requestSelectedTrack(const RequestedTrack& selection);
    bool requestSelectedTrack(const RequestedTrack& selection, const Player::TrackChangeContext& context);
    bool requestSelectedTrack(const std::optional<RequestedTrack>& selection);
    bool requestSelectedTrack(const std::optional<RequestedTrack>& selection,
                              const Player::TrackChangeContext& context);
    [[nodiscard]] std::optional<RequestedTrack> requestedPlaylistTrack(Playlist* playlist, int index) const;
    bool requestPlaylistTrack(Playlist* playlist, int index, const Player::TrackChangeContext& context);
    bool requestBoundaryRestart(PlaybackOrderNavigator::RestartTarget target);
    bool requestBoundaryRestart(PlaybackOrderNavigator::RestartTarget target,
                                const Player::TrackChangeContext& context);

    bool applyTransportAction(const TransportAction& action);
    bool stopAtBoundary(PlaybackCursor::BoundaryStop boundary);

    [[nodiscard]] TransportAction selectPlayAction();
    [[nodiscard]] TransportAction selectPreviousAction();
    [[nodiscard]] TransportAction selectAdvanceAction(Player::AdvanceReason reason);

    void saveActiveTrack() const;
    void restoreActiveTrack() const;

    PlayerController* m_self;
    SettingsManager* m_settings;
    PlaylistHandler* m_playlistHandler{nullptr};

    Player::PlayState m_playState{Player::PlayState::Stopped};
    Playlist::PlayModes m_playMode;

    PlaybackSession m_session;
    PlaybackProgressTracker m_progressTracker;
    bool m_stopCurrentSkip{false};
    bool m_stopAfterCurrentArmed{false};
    bool m_resetStopAfterCurrentPending{false};
    PlaybackCursor m_cursor;

    Player::UpcomingTrack m_lastUpcomingTrack;
    uint64_t m_nextPlaybackItemId{1};
    bool m_currentTrackSeekable{false};

    PlaybackQueue m_queue;
    PlaybackOrderNavigator m_navigator;
};

PlayerControllerPrivate::PlayerControllerPrivate(PlayerController* self, SettingsManager* settings,
                                                 PlaylistHandler* playlistHandler)
    : m_self{self}
    , m_settings{settings}
    , m_playlistHandler{playlistHandler}
    , m_playMode{static_cast<Playlist::PlayModes>(m_settings->value<Settings::Core::PlayMode>())}
    , m_stopAfterCurrentArmed{m_settings->value<Settings::Core::StopAfterCurrent>()}
    , m_navigator{m_settings,
                  m_playlistHandler,
                  &m_queue,
                  {
                      .currentTrack   = m_session.currentTrackPtr(),
                      .playMode       = &m_playMode,
                      .isQueueTrack   = m_session.isQueueTrackPtr(),
                      .scheduledTrack = m_session.scheduledTrackPtr(),
                  }}
{ }

uint64_t PlayerControllerPrivate::nextPlaybackItemId()
{
    uint64_t itemId = m_nextPlaybackItemId++;
    if(itemId == 0) {
        itemId = m_nextPlaybackItemId++;
    }
    if(m_nextPlaybackItemId == 0) {
        m_nextPlaybackItemId = 1;
    }
    return itemId;
}

bool PlayerControllerPrivate::updateBitrate(int bitrate)
{
    if(m_progressTracker.updateBitrate(bitrate)) {
        Q_EMIT m_self->bitrateChanged(bitrate);
        return true;
    }

    return false;
}

void PlayerControllerPrivate::setStopAfterCurrentArmed(bool armed)
{
    if(std::exchange(m_stopAfterCurrentArmed, armed) != armed) {
        Q_EMIT m_self->trackEndAutoTransitionsEnabledChanged(trackEndAutoTransitionsEnabled());
    }

    if(!armed) {
        m_resetStopAfterCurrentPending = false;
    }
}

void PlayerControllerPrivate::queueStopAfterCurrentReset()
{
    m_resetStopAfterCurrentPending
        = m_stopAfterCurrentArmed && m_settings->value<Settings::Core::ResetStopAfterCurrent>();
}

void PlayerControllerPrivate::finishPendingStopAfterCurrentReset()
{
    if(m_resetStopAfterCurrentPending && m_settings->value<Settings::Core::StopAfterCurrent>()) {
        m_settings->set<Settings::Core::StopAfterCurrent>(false);
    }

    m_resetStopAfterCurrentPending = false;
}

void PlayerControllerPrivate::clearStopAfterCurrentForManualSkip()
{
    if(m_stopAfterCurrentArmed && m_settings->value<Settings::Core::ResetStopAfterCurrent>()) {
        m_settings->set<Settings::Core::StopAfterCurrent>(false);
    }
}

bool PlayerControllerPrivate::trackEndAutoTransitionsEnabled() const
{
    return !m_stopAfterCurrentArmed;
}

bool PlayerControllerPrivate::canRewindCurrentTrack() const
{
    return m_settings->value<Settings::Core::RewindPreviousTrack>() && m_session.currentTrack().isValid()
        && m_progressTracker.position() > 5000;
}

bool PlayerControllerPrivate::canPerformPrevious() const
{
    if(canRewindCurrentTrack()) {
        return true;
    }

    const auto restartTarget = m_cursor.restartTargetFor(PlaybackCursor::Command::Previous, m_playState);
    if(m_playState == Player::PlayState::Stopped && restartTarget != PlaybackOrderNavigator::RestartTarget::None
       && !m_session.isQueueTrack()) {
        return true;
    }

    return m_navigator.previewPlaybackRelativeTrack(-1).isValid();
}

Playlist* PlayerControllerPrivate::targetPlaybackPlaylist() const
{
    if(auto* playlist = m_navigator.playbackPlaylist()) {
        return playlist;
    }

    return m_playlistHandler ? m_playlistHandler->activePlaylist() : nullptr;
}

int PlayerControllerPrivate::targetPlaybackIndex(Playlist* playlist) const
{
    if(!playlist) {
        return -1;
    }

    if(m_session.currentTrack().isValid() && !m_session.isQueueTrack()
       && m_session.currentTrack().playlistId == playlist->id()) {
        return m_session.currentTrack().indexInPlaylist;
    }

    return playlist->currentTrackIndex();
}

Player::UpcomingTrack PlayerControllerPrivate::resolveUpcomingTrack() const
{
    if(m_stopAfterCurrentArmed) {
        return {};
    }

    if(m_session.scheduledTrack().isValid()) {
        return {
            .track        = m_session.scheduledTrack(),
            .isQueueTrack = false,
        };
    }

    if(!m_queue.empty()) {
        return {
            .track        = m_queue.nextTrack(),
            .isQueueTrack = true,
        };
    }

    if(!m_playlistHandler) {
        return {};
    }

    if(m_queue.empty() && m_session.isQueueTrack()) {
        if(m_settings->value<Settings::Core::PlaybackQueueStopWhenFinished>()) {
            return {};
        }
        if(m_session.currentTrack().isInPlaylist() && m_settings->value<Settings::Core::FollowPlaybackQueue>()) {
            if(const auto currentTrack = remapPlaylistTrackReference(m_playlistHandler, m_session.currentTrack())) {
                auto* playlist = m_playlistHandler->playlistById(currentTrack->playlistId);
                if(!playlist) {
                    return {};
                }

                const int nextIndex = playlist->nextIndexFrom(currentTrack->indexInPlaylist, 1, m_playMode);
                return {
                    .track        = playlist->playlistTrack(nextIndex).value_or(PlaylistTrack{}),
                    .isQueueTrack = false,
                };
            }
        }
    }

    if(auto* playbackPlaylist = m_navigator.playbackPlaylist()) {
        const int previewIndex
            = playbackPlaylist->nextIndexFrom(m_session.currentTrack().indexInPlaylist, 1, m_playMode);
        return {
            .track        = playbackPlaylist->playlistTrack(previewIndex).value_or(PlaylistTrack{}),
            .isQueueTrack = false,
        };
    }

    if(!m_playlistHandler->activePlaylist()) {
        return {};
    }

    return {
        .track        = m_playlistHandler->peekRelativeTrack(m_playMode, 1),
        .isQueueTrack = false,
    };
}

void PlayerControllerPrivate::emitUpcomingTrackChangedIfNeeded()
{
    Player::UpcomingTrack upcoming = resolveUpcomingTrack();

    if(upcoming.track.isValid()) {
        const bool reuseLastUpcoming
            = m_lastUpcomingTrack.track == upcoming.track && m_lastUpcomingTrack.isQueueTrack == upcoming.isQueueTrack
           && m_lastUpcomingTrack.itemId != 0 && m_lastUpcomingTrack.itemId != m_session.currentItemId();
        upcoming.itemId = reuseLastUpcoming ? m_lastUpcomingTrack.itemId : nextPlaybackItemId();
    }

    const bool upcomingChanged
        = m_lastUpcomingTrack.track != upcoming.track || m_lastUpcomingTrack.isQueueTrack != upcoming.isQueueTrack;

    if(!upcomingChanged) {
        return;
    }

    m_lastUpcomingTrack = upcoming;
    Q_EMIT m_self->upcomingTrackChanged(upcoming);
}

void PlayerControllerPrivate::requestTrackChange(const Player::TrackChangeRequest& request)
{
    const auto requestWithId
        = m_session.requestTrackChange(m_session.withPlaybackItemId(request, nextPlaybackItemId()));
    Q_EMIT m_self->trackChangeRequested(requestWithId);
}

bool PlayerControllerPrivate::updatePlaystate(Player::PlayState state)
{
    const auto prevState = std::exchange(m_playState, state);
    if(prevState != state) {
        Q_EMIT m_self->playStateChanged(state, prevState);
        Q_EMIT m_self->playbackSnapshotChanged(m_self->playbackSnapshot());
        return true;
    }

    return false;
}

void PlayerControllerPrivate::restartCurrentTrackProgressIfNeeded()
{
    if(!m_session.currentTrack().isValid()) {
        return;
    }

    if(m_session.hasPendingRequest() && !m_session.pendingRequestMatchesCurrentTrack()) {
        return;
    }

    m_progressTracker.restartTracking();
}

void PlayerControllerPrivate::restoreRemoteCurrentTrackFromPlaylist()
{
    const PlaylistTrack currentTrack = m_session.currentTrack();
    if(!currentTrack.isValid() || !currentTrack.track.isRemote()) {
        return;
    }

    const auto playlistTrack = remapPlaylistTrackReference(m_playlistHandler, currentTrack);
    if(!playlistTrack.has_value()) {
        return;
    }

    bool currentTrackChanged{false};
    bool playlistTrackChanged{false};

    currentTrackChanged |= m_session.updateCurrentTrack(playlistTrack->track);
    playlistTrackChanged |= m_session.updateCurrentTrackPlaylist(playlistTrack->playlistId);
    playlistTrackChanged |= m_session.updateCurrentTrackEntry(playlistTrack->entryId);
    playlistTrackChanged |= m_session.updateCurrentTrackIndex(playlistTrack->indexInPlaylist);

    if(!currentTrackChanged && !playlistTrackChanged) {
        return;
    }

    syncCommittedPlaylistTrack();

    if(currentTrackChanged) {
        Q_EMIT m_self->currentTrackUpdated(m_session.currentTrack().track);
    }
    Q_EMIT m_self->playlistTrackUpdated(m_session.currentTrack());
}

bool PlayerControllerPrivate::enterStoppedState(bool requestTransportStop)
{
    restoreRemoteCurrentTrackFromPlaylist();

    if(!updatePlaystate(Player::PlayState::Stopped)) {
        return false;
    }

    m_self->setCurrentTrackSeekable(false);
    emitPositionSignals(m_progressTracker.resetPosition());

    if(requestTransportStop) {
        Q_EMIT m_self->transportStopRequested();
    }

    return true;
}

void PlayerControllerPrivate::emitPositionSignals(const PlaybackProgressTracker::PositionUpdate& update)
{
    Q_EMIT m_self->positionChanged(update.positionMs);

    if(update.positionSeconds.has_value()) {
        Q_EMIT m_self->positionChangedSeconds(*update.positionSeconds);
    }
}

void PlayerControllerPrivate::syncCommittedPlaylistTrack() const
{
    if(!m_playlistHandler || m_session.isQueueTrack()) {
        return;
    }

    auto* activePlaylist = m_playlistHandler->activePlaylist();
    if(!activePlaylist) {
        return;
    }

    const PlaylistTrack& currentTrack = m_session.currentTrack();
    if(!currentTrack.isValid() || !currentTrack.playlistId.isValid() || currentTrack.indexInPlaylist < 0) {
        return;
    }

    if(activePlaylist->id() != currentTrack.playlistId) {
        return;
    }

    if(activePlaylist->currentTrackIndex() != currentTrack.indexInPlaylist) {
        activePlaylist->changeCurrentIndex(currentTrack.indexInPlaylist);
    }
}

void PlayerControllerPrivate::updateCurrentTrackPlaylist(const UId& playlistId)
{
    if(m_session.updateCurrentTrackPlaylist(playlistId)) {
        syncCommittedPlaylistTrack();
        Q_EMIT m_self->playlistTrackUpdated(m_session.currentTrack());
    }
}

void PlayerControllerPrivate::updateCurrentTrackEntry(const UId& entryId)
{
    if(m_session.updateCurrentTrackEntry(entryId)) {
        Q_EMIT m_self->playlistTrackUpdated(m_session.currentTrack());
    }
}

void PlayerControllerPrivate::updateCurrentTrackIndex(int index)
{
    if(m_session.updateCurrentTrackIndex(index)) {
        syncCommittedPlaylistTrack();
        Q_EMIT m_self->playlistTrackUpdated(m_session.currentTrack());
    }
}

void PlayerControllerPrivate::syncPlaylistTrackState(const UId& playlistId)
{
    if(!playlistId.isValid() || !m_playlistHandler) {
        return;
    }

    bool playlistTrackChanged{false};
    bool currentTrackChanged{false};

    const auto syncCurrentTrack = [this, &playlistTrackChanged, &currentTrackChanged](const PlaylistTrack& track) {
        currentTrackChanged |= m_session.updateCurrentTrack(track.track);
        playlistTrackChanged |= m_session.updateCurrentTrackPlaylist(track.playlistId);
        playlistTrackChanged |= m_session.updateCurrentTrackEntry(track.entryId);
        playlistTrackChanged |= m_session.updateCurrentTrackIndex(track.indexInPlaylist);
    };

    if(m_session.currentTrack().playlistId == playlistId) {
        if(const auto remappedTrack = remapPlaylistTrackReference(m_playlistHandler, m_session.currentTrack())) {
            syncCurrentTrack(*remappedTrack);

            const auto& detachedTrack = m_session.detachedCurrentPlaylistTrack();
            if(detachedTrack.has_value() && detachedTrack->entryId == remappedTrack->entryId
               && detachedTrack->playlistId == remappedTrack->playlistId) {
                m_session.clearDetachedCurrentPlaylistTrack();
            }
        }
        else {
            m_session.setDetachedCurrentPlaylistTrack(m_session.currentTrack());
            playlistTrackChanged |= m_session.updateCurrentTrackPlaylist({});
            playlistTrackChanged |= m_session.updateCurrentTrackEntry({});
            playlistTrackChanged |= m_session.updateCurrentTrackIndex(-1);
        }

        syncCommittedPlaylistTrack();

        if(currentTrackChanged) {
            Q_EMIT m_self->currentTrackUpdated(m_session.currentTrack().track);
        }
        if(playlistTrackChanged) {
            Q_EMIT m_self->playlistTrackUpdated(m_session.currentTrack());
        }
    }
    else if(const auto& detachedTrack = m_session.detachedCurrentPlaylistTrack();
            !m_session.currentTrack().isInPlaylist() && detachedTrack.has_value()
            && detachedTrack->playlistId == playlistId
            && m_session.currentTrack().track.sameIdentityAs(detachedTrack->track)) {
        if(const auto remappedTrack = remapPlaylistTrackReference(m_playlistHandler, *detachedTrack)) {
            syncCurrentTrack(*remappedTrack);
            m_session.clearDetachedCurrentPlaylistTrack();
            syncCommittedPlaylistTrack();
            if(currentTrackChanged) {
                Q_EMIT m_self->currentTrackUpdated(m_session.currentTrack().track);
            }
            if(playlistTrackChanged) {
                Q_EMIT m_self->playlistTrackUpdated(m_session.currentTrack());
            }
        }
    }

    bool queueChanged{false};
    auto queueTracks = m_queue.tracks();
    for(auto& track : queueTracks) {
        if(track.playlistId != playlistId) {
            continue;
        }

        if(const auto remappedTrack = remapPlaylistTrackReference(m_playlistHandler, track)) {
            if(track != *remappedTrack) {
                track        = *remappedTrack;
                queueChanged = true;
            }
        }
        else if(track.playlistId.isValid() || track.entryId.isValid() || track.indexInPlaylist >= 0) {
            track.playlistId      = {};
            track.entryId         = {};
            track.indexInPlaylist = -1;
            queueChanged          = true;
        }
    }

    if(queueChanged) {
        m_self->replaceTracks(queueTracks);
    }

    auto* scheduledTrack = m_session.scheduledTrackPtr();
    if(scheduledTrack && scheduledTrack->playlistId == playlistId) {
        if(const auto remappedTrack = remapPlaylistTrackReference(m_playlistHandler, *scheduledTrack)) {
            *scheduledTrack = *remappedTrack;
        }
        else {
            scheduledTrack->playlistId      = {};
            scheduledTrack->entryId         = {};
            scheduledTrack->indexInPlaylist = -1;
        }
    }

    emitUpcomingTrackChangedIfNeeded();
}

void PlayerControllerPrivate::remapPlaylistReferences(const UId& fromPlaylistId, const UId& toPlaylistId)
{
    if(!fromPlaylistId.isValid() || !toPlaylistId.isValid() || fromPlaylistId == toPlaylistId) {
        return;
    }

    auto queueTracks = m_queue.tracks();
    bool updated{false};
    for(auto& track : queueTracks) {
        if(track.playlistId == fromPlaylistId) {
            track.playlistId = toPlaylistId;
            updated          = true;
        }
    }

    if(updated) {
        m_self->replaceTracks(queueTracks);
    }

    if(m_session.currentTrack().playlistId == fromPlaylistId) {
        updateCurrentTrackPlaylist(toPlaylistId);
    }

    emitUpcomingTrackChangedIfNeeded();
}

bool PlayerControllerPrivate::requestSelectedTrack(const RequestedTrack& selection)
{
    return requestSelectedTrack(selection, m_session.pendingChangeContext());
}

bool PlayerControllerPrivate::requestSelectedTrack(const RequestedTrack& selection,
                                                   const Player::TrackChangeContext& context)
{
    if(!selection.track.isValid() || !m_session.canAcceptRequest()) {
        return false;
    }

    requestTrackChange({
        .track        = selection.track,
        .context      = context,
        .isQueueTrack = selection.isQueueTrack,
    });

    return true;
}

bool PlayerControllerPrivate::requestSelectedTrack(const std::optional<RequestedTrack>& selection)
{
    return selection.has_value() && requestSelectedTrack(*selection);
}

bool PlayerControllerPrivate::requestSelectedTrack(const std::optional<RequestedTrack>& selection,
                                                   const Player::TrackChangeContext& context)
{
    return selection.has_value() && requestSelectedTrack(*selection, context);
}

std::optional<PlayerControllerPrivate::RequestedTrack>
PlayerControllerPrivate::requestedPlaylistTrack(Playlist* playlist, int index) const
{
    if(!playlist || index < 0) {
        return {};
    }

    if(const auto track = playlist->playlistTrack(index); track && track->isValid()) {
        return RequestedTrack{
            .track        = *track,
            .isQueueTrack = false,
        };
    }

    return {};
}

bool PlayerControllerPrivate::requestPlaylistTrack(Playlist* playlist, const int index,
                                                   const Player::TrackChangeContext& context)
{
    const auto selection = requestedPlaylistTrack(playlist, index);
    if(!selection || !m_session.canAcceptRequest()) {
        return false;
    }

    if(m_playlistHandler->activePlaylist() != playlist) {
        m_playlistHandler->changeActivePlaylist(playlist);
    }

    playlist->changeCurrentIndex(index);
    return requestSelectedTrack(*selection, context);
}

bool PlayerControllerPrivate::requestBoundaryRestart(PlaybackOrderNavigator::RestartTarget target)
{
    return requestBoundaryRestart(target, m_session.pendingChangeContext());
}

bool PlayerControllerPrivate::requestBoundaryRestart(PlaybackOrderNavigator::RestartTarget target,
                                                     const Player::TrackChangeContext& context)
{
    if(target == PlaybackOrderNavigator::RestartTarget::None || !m_session.canAcceptRequest()) {
        return false;
    }

    const PlaylistTrack track = m_navigator.restartPlaylist(target);
    if(!track.isValid()) {
        return false;
    }

    m_cursor.reset();
    return requestSelectedTrack({.track = track, .isQueueTrack = false}, context);
}

bool PlayerControllerPrivate::applyTransportAction(const TransportAction& action)
{
    switch(action.type) {
        case TransportAction::Type::Stop:
            m_self->stop();
            return false;
        case TransportAction::Type::ResetAndStop:
            m_self->reset();
            m_self->stop();
            return false;
        case TransportAction::Type::AdvancePlaybackPositionAndStop:
            m_navigator.selectPlaybackOrderTrack(1);
            m_self->reset();
            m_self->stop();
            return false;
        case TransportAction::Type::StopAtBoundary:
            return stopAtBoundary(action.boundary);
        case TransportAction::Type::KeepCurrentTrack:
            return false;
        case TransportAction::Type::RequestSelection:
            return requestSelectedTrack(action.selection);
        case TransportAction::Type::RestartAtBoundary:
            return requestBoundaryRestart(action.restartTarget);
        case TransportAction::Type::ClearCurrentTrack:
            m_session.clearCurrentTrack();
            return false;
        case TransportAction::Type::None:
        default:
            return false;
    }
}

bool PlayerControllerPrivate::stopAtBoundary(PlaybackCursor::BoundaryStop boundary)
{
    m_cursor.stopAt(boundary);
    m_self->stop();
    return true;
}

void PlayerControllerPrivate::saveActiveTrack() const
{
    PlaybackState::clearActiveTrackIndex();

    if(!m_settings->fileValue(Settings::Core::Internal::SaveActivePlaylistState).toBool()) {
        return;
    }

    auto* playlist = m_playlistHandler->activePlaylist();
    if(!playlist || playlist->isTemporary()) {
        return;
    }

    const auto track = m_self->currentPlaylistTrack();
    if(track.isValid() && track.isInPlaylist() && track.playlistId == playlist->id()) {
        PlaybackState::saveActiveTrackIndex(track.indexInPlaylist);
        return;
    }

    const int currentIndex = playlist->currentTrackIndex();
    if(currentIndex >= 0 && currentIndex < playlist->trackCount()) {
        PlaybackState::saveActiveTrackIndex(currentIndex);
    }
}

void PlayerControllerPrivate::restoreActiveTrack() const
{
    if(!m_settings->fileValue(Settings::Core::Internal::SaveActivePlaylistState).toBool()) {
        PlaybackState::clearActiveTrackIndex();
        return;
    }

    auto* playlist = m_playlistHandler->activePlaylist();
    if(!playlist || playlist->isTemporary()) {
        return;
    }

    if(const auto lastIndex = PlaybackState::activeTrackIndex(); lastIndex.has_value()) {
        const int count = playlist->trackCount();
        if(*lastIndex >= 0 && *lastIndex < count) {
            playlist->changeCurrentIndex(*lastIndex);

            const auto state = PlaybackState::playbackState();
            if(!state.has_value() || *state == Player::PlayState::Stopped) {
                return;
            }

            if(const auto track = playlist->playlistTrack(*lastIndex); track.has_value() && track->isValid()) {
                m_self->changeCurrentTrack(*track,
                                           {.reason = Player::AdvanceReason::StartupRestore, .userInitiated = false});
            }
        }
        else {
            qCInfo(PLAYER_CONTROLLER) << "Ignoring stale saved track index" << *lastIndex;
            PlaybackState::clearActiveTrackIndex();
        }
    }
}

PlayerControllerPrivate::TransportAction PlayerControllerPrivate::selectAdvanceAction(Player::AdvanceReason reason)
{
    if(!m_stopCurrentSkip && m_stopAfterCurrentArmed) {
        queueStopAfterCurrentReset();

        if(reason == Player::AdvanceReason::NaturalEnd && m_queue.empty() && !m_session.scheduledTrack().isValid()
           && !m_session.isQueueTrack()) {
            if(m_navigator.previewPlaybackRelativeTrack(1).isValid()) {
                return {
                    .type      = TransportAction::Type::AdvancePlaybackPositionAndStop,
                    .selection = std::nullopt,
                };
            }

            m_session.pendingChangeContext() = {};
            return {
                .type      = TransportAction::Type::StopAtBoundary,
                .boundary  = PlaybackCursor::BoundaryStop::End,
                .selection = std::nullopt,
            };
        }

        return {
            .type      = TransportAction::Type::ResetAndStop,
            .selection = std::nullopt,
        };
    }

    if(reason == Player::AdvanceReason::NaturalEnd && !m_self->hasNextTrack()) {
        m_session.pendingChangeContext() = {};
        return {
            .type      = TransportAction::Type::StopAtBoundary,
            .boundary  = PlaybackCursor::BoundaryStop::End,
            .selection = std::nullopt,
        };
    }

    const auto restartTarget = m_cursor.restartTargetFor(PlaybackCursor::Command::Next, m_playState);
    if(reason == Player::AdvanceReason::ManualNext && restartTarget != PlaybackOrderNavigator::RestartTarget::None
       && m_playState == Player::PlayState::Stopped && m_queue.empty() && !m_session.scheduledTrack().isValid()
       && !m_session.isQueueTrack()) {
        return {
            .type          = TransportAction::Type::RestartAtBoundary,
            .restartTarget = restartTarget,
            .selection     = std::nullopt,
        };
    }

    if(reason == Player::AdvanceReason::ManualNext && m_session.canAcceptRequest()) {
        if(m_session.scheduledTrack().isValid()) {
            return {
                .type      = TransportAction::Type::RequestSelection,
                .selection = m_navigator.selectScheduledTrack(),
            };
        }

        if(const auto selection = m_navigator.selectPlaybackOrderTrack(1); selection && selection->track.isValid()) {
            return {
                .type      = TransportAction::Type::RequestSelection,
                .selection = selection,
            };
        }

        if(m_navigator.previewPlaybackRelativeTrack(1).isValid()) {
            return {
                .type      = TransportAction::Type::KeepCurrentTrack,
                .selection = std::nullopt,
            };
        }

        return {
            .type      = TransportAction::Type::StopAtBoundary,
            .boundary  = PlaybackCursor::BoundaryStop::End,
            .selection = std::nullopt,
        };
    }

    if(m_session.canAcceptRequest() && m_session.scheduledTrack().isValid()) {
        return {
            .type      = TransportAction::Type::RequestSelection,
            .selection = m_navigator.selectScheduledTrack(),
        };
    }

    if(m_session.canAcceptRequest()) {
        if(m_session.isQueueTrack() && m_settings->value<Settings::Core::PlaybackQueueStopWhenFinished>()) {
            return {
                .type      = TransportAction::Type::ResetAndStop,
                .selection = std::nullopt,
            };
        }

        return {
            .type      = TransportAction::Type::RequestSelection,
            .selection = m_navigator.selectPlaybackOrderTrack(1),
        };
    }

    return {};
}

PlayerControllerPrivate::TransportAction PlayerControllerPrivate::selectPreviousAction()
{
    const auto restartTarget = m_cursor.restartTargetFor(PlaybackCursor::Command::Previous, m_playState);
    if(m_playState == Player::PlayState::Stopped && restartTarget != PlaybackOrderNavigator::RestartTarget::None
       && !m_session.isQueueTrack()) {
        return {
            .type          = TransportAction::Type::RestartAtBoundary,
            .restartTarget = restartTarget,
            .selection     = std::nullopt,
        };
    }

    if(!m_self->hasPreviousTrack()) {
        if(m_navigator.previewPlaybackRelativeTrack(-1).isValid()) {
            return {
                .type      = TransportAction::Type::KeepCurrentTrack,
                .selection = std::nullopt,
            };
        }

        return {
            .type      = TransportAction::Type::StopAtBoundary,
            .boundary  = PlaybackCursor::BoundaryStop::Start,
            .selection = std::nullopt,
        };
    }

    if(const auto selection = m_navigator.selectPlaybackOrderTrack(-1)) {
        return {
            .type      = TransportAction::Type::RequestSelection,
            .selection = selection,
        };
    }

    if(!m_playlistHandler) {
        return {
            .type      = TransportAction::Type::ClearCurrentTrack,
            .selection = std::nullopt,
        };
    }

    return {};
}

PlayerControllerPrivate::TransportAction PlayerControllerPrivate::selectPlayAction()
{
    const auto restartTarget = m_cursor.restartTargetFor(PlaybackCursor::Command::Play, m_playState);
    if(m_playState == Player::PlayState::Stopped && restartTarget != PlaybackOrderNavigator::RestartTarget::None
       && m_session.hasCurrentTrack() && m_session.canAcceptRequest() && m_queue.empty()
       && !m_session.scheduledTrack().isValid() && !m_session.isQueueTrack()) {
        return {
            .type          = TransportAction::Type::RestartAtBoundary,
            .restartTarget = restartTarget,
            .selection     = std::nullopt,
        };
    }

    if(m_session.isIdle()) {
        return {
            .type      = TransportAction::Type::RequestSelection,
            .selection = m_navigator.selectPlayFromIdleState(),
        };
    }

    return {};
}

PlayerController::PlayerController(SettingsManager* settings, PlaylistHandler* playlistHandler, QObject* parent)
    : QObject{parent}
    , p{std::make_unique<PlayerControllerPrivate>(this, settings, playlistHandler)}
{
    settings->subscribe<Settings::Core::PlayMode>(this, [this]() {
        const auto mode = static_cast<Playlist::PlayModes>(p->m_settings->value<Settings::Core::PlayMode>());
        if(std::exchange(p->m_playMode, mode) != mode) {
            Q_EMIT playModeChanged(mode);
            p->emitUpcomingTrackChangedIfNeeded();
        }
    });
    settings->subscribe<Settings::Core::StopAfterCurrent>(this, [this](bool enabled) {
        p->setStopAfterCurrentArmed(enabled);
        p->emitUpcomingTrackChangedIfNeeded();
    });
    settings->subscribe<Settings::Core::Shutdown>(this, [this]() { p->saveActiveTrack(); });

    if(!playlistHandler) {
        return;
    }

    const auto changeUpcomingTrack = [this, playlistHandler](Playlist* playlist) {
        if(playlist == playlistHandler->activePlaylist()) {
            p->emitUpcomingTrackChangedIfNeeded();
        }
    };

    QObject::connect(playlistHandler, &PlaylistHandler::playlistsPopulated, this,
                     [this]() { p->restoreActiveTrack(); });
    QObject::connect(playlistHandler, &PlaylistHandler::activePlaylistChanged, this,
                     [this]() { p->emitUpcomingTrackChangedIfNeeded(); });
    QObject::connect(playlistHandler, &PlaylistHandler::tracksAdded, this, changeUpcomingTrack);
    QObject::connect(playlistHandler, &PlaylistHandler::tracksPatched, this, changeUpcomingTrack);
    QObject::connect(playlistHandler, &PlaylistHandler::tracksChanged, this, changeUpcomingTrack);
    QObject::connect(playlistHandler, &PlaylistHandler::tracksUpdated, this, changeUpcomingTrack);
    QObject::connect(playlistHandler, &PlaylistHandler::tracksRemoved, this, changeUpcomingTrack);

    QObject::connect(playlistHandler, &PlaylistHandler::activePlaylistDeleted, this, [this]() {
        if(p->m_settings->value<Settings::Core::StopIfActivePlaylistDeleted>()) {
            stop();
        }
    });

    QObject::connect(playlistHandler, &PlaylistHandler::playlistReferencesRemapRequested, this,
                     [this](const UId& fromPlaylistId, const UId& toPlaylistId) {
                         p->remapPlaylistReferences(fromPlaylistId, toPlaylistId);
                     });
    QObject::connect(playlistHandler, &PlaylistHandler::tracksAdded, this,
                     [this](Playlist* playlist) { p->syncPlaylistTrackState(playlist ? playlist->id() : UId{}); });
    QObject::connect(playlistHandler, &PlaylistHandler::tracksChanged, this,
                     [this](Playlist* playlist) { p->syncPlaylistTrackState(playlist ? playlist->id() : UId{}); });
    QObject::connect(playlistHandler, &PlaylistHandler::tracksPatched, this,
                     [this](Playlist* playlist) { p->syncPlaylistTrackState(playlist ? playlist->id() : UId{}); });
    QObject::connect(playlistHandler, &PlaylistHandler::tracksRemoved, this,
                     [this](Playlist* playlist) { p->syncPlaylistTrackState(playlist ? playlist->id() : UId{}); });
}

PlayerController::~PlayerController() = default;

void PlayerController::reset()
{
    p->m_session.resetCurrentTrackState();
    p->m_progressTracker.reset();
    p->m_cursor.reset();
    p->m_session.clearPendingRequest();
    p->updateBitrate(0);
    setCurrentTrackSeekable(false);

    p->emitPositionSignals(p->m_progressTracker.resetPosition());
    Q_EMIT playbackSnapshotChanged(playbackSnapshot());
    p->emitUpcomingTrackChangedIfNeeded();
}

void PlayerController::play()
{
    const bool wasStopped = p->m_playState == Player::PlayState::Stopped;

    p->applyTransportAction(p->selectPlayAction());

    if(!p->m_session.isIdle()) {
        if(wasStopped) {
            p->restartCurrentTrackProgressIfNeeded();
        }
        if(p->updatePlaystate(Player::PlayState::Playing)) {
            Q_EMIT transportPlayRequested();
        }
    }
    else {
        p->m_session.clearCurrentTrack();
        Q_EMIT playbackSnapshotChanged(playbackSnapshot());
    }
}

void PlayerController::playPause()
{
    switch(p->m_playState) {
        case Player::PlayState::Playing:
            pause();
            break;
        case Player::PlayState::Paused:
        case Player::PlayState::Stopped:
            play();
            break;
        default:
            break;
    }
}

void PlayerController::pause()
{
    if(p->updatePlaystate(Player::PlayState::Paused)) {
        Q_EMIT transportPauseRequested();
    }
}

void PlayerController::previous()
{
    p->clearStopAfterCurrentForManualSkip();

    if(p->canRewindCurrentTrack()) {
        seek(0);
        return;
    }

    // Temporarily disable repeating track when user clicks 'previous'.
    const Utils::ScopedValueRestore playModeRestore{p->m_playMode};

    p->m_playMode &= ~Playlist::RepeatTrack;

    const PlayerControllerPrivate::TransportAction action = p->selectPreviousAction();
    const Player::TrackChangeContext context{
        .reason        = Player::AdvanceReason::ManualPrevious,
        .userInitiated = true,
    };

    if(action.type == PlayerControllerPrivate::TransportAction::Type::RequestSelection) {
        p->requestSelectedTrack(action.selection, context);
    }
    else if(action.type == PlayerControllerPrivate::TransportAction::Type::RestartAtBoundary) {
        p->requestBoundaryRestart(action.restartTarget, context);
    }
    else {
        p->applyTransportAction(action);
    }

    if(action.type == PlayerControllerPrivate::TransportAction::Type::StopAtBoundary
       || action.type == PlayerControllerPrivate::TransportAction::Type::KeepCurrentTrack) {
        return;
    }

    if(!p->m_session.isIdle()) {
        play();
    }
}

void PlayerController::next()
{
    // Temporarily disable repeating track and 'stop after current' when user clicks 'next'.
    const Utils::ScopedValueRestore playModeRestore{p->m_playMode};
    const Utils::ScopedValueRestore stopCurrentSkipRestore{p->m_stopCurrentSkip};

    p->m_playMode &= ~Playlist::RepeatTrack;
    p->m_stopCurrentSkip = true;
    p->clearStopAfterCurrentForManualSkip();

    advance(Player::AdvanceReason::ManualNext);
}

void PlayerController::randomTrack()
{
    p->clearStopAfterCurrentForManualSkip();

    auto* playlist = p->targetPlaybackPlaylist();
    if(!playlist) {
        return;
    }

    const int targetIndex = playlist->randomTrackIndexFrom(p->targetPlaybackIndex(playlist));
    if(targetIndex < 0) {
        return;
    }

    if(p->requestPlaylistTrack(playlist, targetIndex,
                               {.reason = Player::AdvanceReason::ManualSelection, .userInitiated = true})) {
        play();
    }
}

void PlayerController::randomAlbum()
{
    p->clearStopAfterCurrentForManualSkip();

    auto* playlist = p->targetPlaybackPlaylist();
    if(!playlist) {
        return;
    }

    const int targetIndex = playlist->randomAlbumIndexFrom(p->targetPlaybackIndex(playlist));
    if(targetIndex < 0) {
        return;
    }

    if(p->requestPlaylistTrack(playlist, targetIndex,
                               {.reason = Player::AdvanceReason::ManualSelection, .userInitiated = true})) {
        play();
    }
}

void PlayerController::previousAlbum()
{
    p->clearStopAfterCurrentForManualSkip();

    auto* playlist = p->targetPlaybackPlaylist();
    if(!playlist) {
        return;
    }

    const int targetIndex = playlist->previousAlbumIndexFrom(p->targetPlaybackIndex(playlist), p->m_playMode);
    if(targetIndex < 0) {
        return;
    }

    if(p->requestPlaylistTrack(playlist, targetIndex,
                               {.reason = Player::AdvanceReason::ManualPrevious, .userInitiated = true})) {
        play();
    }
}

void PlayerController::nextAlbum()
{
    p->clearStopAfterCurrentForManualSkip();

    auto* playlist = p->targetPlaybackPlaylist();
    if(!playlist) {
        return;
    }

    const int targetIndex = playlist->nextAlbumIndexFrom(p->targetPlaybackIndex(playlist), p->m_playMode);
    if(targetIndex < 0) {
        return;
    }

    if(p->requestPlaylistTrack(playlist, targetIndex,
                               {.reason = Player::AdvanceReason::ManualNext, .userInitiated = true})) {
        play();
    }
}

void PlayerController::advance(Player::AdvanceReason reason)
{
    const bool userInitiated
        = reason == Player::AdvanceReason::ManualNext || reason == Player::AdvanceReason::ManualPrevious;
    p->m_session.pendingChangeContext() = {.reason = reason, .userInitiated = userInitiated};

    const PlayerControllerPrivate::TransportAction action = p->selectAdvanceAction(reason);
    p->applyTransportAction(action);

    if(action.type == PlayerControllerPrivate::TransportAction::Type::Stop
       || action.type == PlayerControllerPrivate::TransportAction::Type::ResetAndStop
       || action.type == PlayerControllerPrivate::TransportAction::Type::StopAtBoundary
       || action.type == PlayerControllerPrivate::TransportAction::Type::KeepCurrentTrack) {
        return;
    }

    if(p->m_session.hasPendingRequest()) {
        play();
    }
    else {
        stop();
    }
}

Player::TrackChangeContext PlayerController::lastTrackChangeContext() const
{
    return p->m_session.lastChangeContext();
}

void PlayerController::stop()
{
    p->queueStopAfterCurrentReset();
    p->enterStoppedState(true);
}

void PlayerController::syncPlayStateFromEngine(Player::PlayState state)
{
    switch(state) {
        case Player::PlayState::Playing:
            if(p->m_playState == Player::PlayState::Stopped) {
                p->restartCurrentTrackProgressIfNeeded();
            }
            p->updatePlaystate(Player::PlayState::Playing);
            break;
        case Player::PlayState::Paused:
            p->updatePlaystate(Player::PlayState::Paused);
            break;
        case Player::PlayState::Stopped:
            p->enterStoppedState(false);
            p->finishPendingStopAfterCurrentReset();
            break;
    }
}

void PlayerController::setCurrentPosition(uint64_t ms)
{
    const PlaybackProgressTracker::PositionUpdate update = p->m_progressTracker.updatePosition(ms);

    if(update.reachedPlayedThreshold && p->m_session.currentTrack().isValid()
       && !p->m_session.currentTrack().track.isRemote()) {
        qCDebug(PLAYER_CONTROLLER) << "Track reached played threshold:" << "id="
                                   << p->m_session.currentTrack().track.id()
                                   << "path=" << p->m_session.currentTrack().track.uniqueFilepath()
                                   << "timeListened=" << p->m_progressTracker.timeListened()
                                   << "playedThreshold=" << p->m_progressTracker.playedThreshold()
                                   << "playCount=" << p->m_session.currentTrack().track.playCount();
        Q_EMIT trackPlayed(p->m_session.currentTrack().track);
    }

    p->emitPositionSignals(update);
}

void PlayerController::restoreCurrentPosition(uint64_t ms)
{
    p->emitPositionSignals(p->m_progressTracker.restorePosition(ms));
}

void PlayerController::restorePlaybackProgress(uint64_t positionMs, uint64_t timeListenedMs)
{
    p->emitPositionSignals(p->m_progressTracker.restoreProgress(positionMs, timeListenedMs));
}

void PlayerController::setBitrate(int bitrate)
{
    if(p->updateBitrate(bitrate)) {
        Q_EMIT playbackSnapshotChanged(playbackSnapshot());
    }
}

void PlayerController::changeCurrentTrack(const Track& track)
{
    changeCurrentTrack(PlaylistTrack{.track = track, .playlistId = {}, .entryId = {}});
}

void PlayerController::changeCurrentTrack(const PlaylistTrack& track, const Player::TrackChangeContext& context)
{
    if(!track.isValid()) {
        return;
    }

    p->requestTrackChange({.track = track, .context = context, .isQueueTrack = p->m_queue.containsTrack(track)});
}

void PlayerController::commitCurrentTrack(const Track& track)
{
    commitCurrentTrack(PlaylistTrack{.track = track, .playlistId = {}, .entryId = {}});
}

void PlayerController::commitCurrentTrack(const Player::TrackChangeRequest& request)
{
    const auto requestWithId = p->m_session.withPlaybackItemId(request, p->nextPlaybackItemId());
    if(!requestWithId.track.isValid()) {
        reset();
        return;
    }

    const auto result = p->m_session.commitRequest(requestWithId);
    p->syncCommittedPlaylistTrack();
    p->m_cursor.onTrackCommitted();
    p->m_progressTracker.onTrackCommitted(p->m_session.currentTrack().track.duration(),
                                          p->m_settings->value<Settings::Core::PlayedThreshold>());
    p->updateBitrate(0);

    if(result.isQueueTrack) {
        if(const auto removedTrack = p->m_queue.removeFirstMatchingTrack(requestWithId.track);
           removedTrack.has_value()) {
            Q_EMIT tracksDequeued({*removedTrack});
        }
    }

    p->m_settings->set<Settings::Core::ActiveTrack>(QVariant::fromValue(p->m_session.currentTrack().track));
    p->m_settings->set<Settings::Core::ActiveTrackId>(p->m_session.currentTrack().track.id());

    Q_EMIT currentTrackChanged(p->m_session.currentTrack().track);
    Q_EMIT playlistTrackChanged(p->m_session.currentTrack());
    Q_EMIT playlistTrackUpdated(p->m_session.currentTrack());
    Q_EMIT playbackSnapshotChanged(playbackSnapshot());
    p->emitUpcomingTrackChangedIfNeeded();
}

void PlayerController::commitCurrentTrack(const PlaylistTrack& track, const Player::TrackChangeContext& context)
{
    commitCurrentTrack(Player::TrackChangeRequest{
        .track        = track,
        .context      = context,
        .isQueueTrack = false,
        .itemId       = p->nextPlaybackItemId(),
    });
}

void PlayerController::updateCurrentTrack(const Track& track)
{
    if(p->m_session.updateCurrentTrack(track)) {
        Q_EMIT currentTrackUpdated(p->m_session.currentTrack().track);
        Q_EMIT playlistTrackUpdated(p->m_session.currentTrack());
    }
}

void PlayerController::scheduleNextTrack(const PlaylistTrack& track)
{
    p->m_session.scheduleTrack(track);
    p->emitUpcomingTrackChangedIfNeeded();
}

Track PlayerController::upcomingTrack() const
{
    return p->resolveUpcomingTrack().track.track;
}

PlaylistTrack PlayerController::upcomingPlaylistTrack() const
{
    return p->resolveUpcomingTrack().track;
}

bool PlayerController::hasNextTrack() const
{
    return upcomingTrack().isValid();
}

bool PlayerController::hasPreviousTrack() const
{
    return p->canPerformPrevious();
}

bool PlayerController::trackEndAutoTransitionsEnabled() const
{
    return p->trackEndAutoTransitionsEnabled();
}

Player::PlaybackSnapshot PlayerController::playbackSnapshot() const
{
    return {
        .playState       = p->m_playState,
        .track           = p->m_session.currentTrack().track,
        .playlistId      = p->m_session.currentTrack().playlistId,
        .indexInPlaylist = p->m_session.currentTrack().indexInPlaylist,
        .positionMs      = p->m_progressTracker.position(),
        .durationMs      = p->m_progressTracker.totalDuration(),
        .bitrate         = p->m_progressTracker.bitrate(),
        .isQueueTrack    = p->m_session.isQueueTrack(),
    };
}

const PlaybackQueue& PlayerController::playbackQueue() const
{
    return p->m_queue;
}

int PlayerController::queuedTracksCount() const
{
    return p->m_queue.trackCount();
}

void PlayerController::setPlayMode(Playlist::PlayModes mode)
{
    p->m_settings->set<Settings::Core::PlayMode>(static_cast<int>(mode));
}

void PlayerController::seek(uint64_t ms)
{
    if(!p->m_currentTrackSeekable || p->m_progressTracker.totalDuration() < 100) {
        return;
    }

    p->m_cursor.onSeek();

    if(ms >= p->m_progressTracker.totalDuration() - 100) {
        next();
        return;
    }

    if(p->m_progressTracker.markSeekPosition(ms)) {
        Q_EMIT positionMoved(ms);
    }
}

void PlayerController::seekForward(uint64_t delta)
{
    seek(p->m_progressTracker.position() + delta);
}

void PlayerController::seekBackward(uint64_t delta)
{
    if(delta > p->m_progressTracker.position()) {
        seek(0);
    }
    else {
        seek(p->m_progressTracker.position() - delta);
    }
}

void PlayerController::startPlayback(const UId& playlistId)
{
    if(!p->m_playlistHandler) {
        return;
    }

    if(auto* playlist = p->m_playlistHandler->playlistById(playlistId)) {
        p->m_playlistHandler->changeActivePlaylist(playlistId);
        playlist->reset();

        const PlaylistTrack currentTrack = p->m_playlistHandler->currentTrack();
        changeCurrentTrack(currentTrack, {.reason = Player::AdvanceReason::ManualSelection, .userInitiated = true});
        play();
    }
}

void PlayerController::startPlayback(Playlist* playlist)
{
    if(!playlist || !p->m_playlistHandler) {
        return;
    }

    if(!playlist->id().isValid() || p->m_playlistHandler->playlistById(playlist->id()) != playlist) {
        return;
    }

    p->m_playlistHandler->changeActivePlaylist(playlist);
    playlist->reset();
    if(playlist->currentTrackIndex() < 0) {
        playlist->changeCurrentIndex(0);
    }

    const PlaylistTrack currentTrack = p->m_playlistHandler->currentTrack();
    changeCurrentTrack(currentTrack, {.reason = Player::AdvanceReason::ManualSelection, .userInitiated = true});
    play();
}

Player::PlayState PlayerController::playState() const
{
    return p->m_playState;
}

Playlist::PlayModes PlayerController::playMode() const
{
    return p->m_playMode;
}

uint64_t PlayerController::currentPosition() const
{
    return p->m_progressTracker.position();
}

uint64_t PlayerController::currentTimeListened() const
{
    return p->m_progressTracker.timeListened();
}

bool PlayerController::playedThresholdReached() const
{
    return p->m_progressTracker.playedThresholdReached();
}

int PlayerController::bitrate() const
{
    return p->m_progressTracker.bitrate();
}

bool PlayerController::currentTrackSeekable() const
{
    return p->m_currentTrackSeekable;
}

Track PlayerController::currentTrack() const
{
    return p->m_session.currentTrack().track;
}

int PlayerController::currentTrackId() const
{
    return p->m_session.currentTrack().isValid() ? p->m_session.currentTrack().track.id() : -1;
}

bool PlayerController::currentIsQueueTrack() const
{
    return p->m_session.isQueueTrack();
}

PlaylistTrack PlayerController::currentPlaylistTrack() const
{
    return p->m_session.currentTrack();
}

void PlayerController::setCurrentTrackSeekable(bool seekable)
{
    if(std::exchange(p->m_currentTrackSeekable, seekable) != seekable) {
        Q_EMIT currentTrackSeekableChanged(seekable);
    }
}

void PlayerController::queueTrack(const Track& track)
{
    queueTrack(PlaylistTrack{.track = track, .playlistId = {}, .entryId = {}});
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

    const int index = p->m_queue.trackCount();

    p->m_queue.addTracks(tracks);
    Q_EMIT tracksQueued(tracks, index);
    p->emitUpcomingTrackChangedIfNeeded();
}

void PlayerController::queueTrackNext(const Track& track)
{
    queueTrackNext(PlaylistTrack{.track = track, .playlistId = {}, .entryId = {}});
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

    p->m_queue.addTracks(tracks, 0);
    Q_EMIT trackQueueChanged({}, p->m_queue.tracks());
    p->emitUpcomingTrackChangedIfNeeded();
}

void PlayerController::dequeueTrack(const Track& track)
{
    dequeueTrack(PlaylistTrack{.track = track, .playlistId = {}, .entryId = {}});
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
        Q_EMIT tracksDequeued(removedTracks);
        p->emitUpcomingTrackChangedIfNeeded();
    }
}

void PlayerController::dequeueTracks(const std::vector<int>& indexes)
{
    if(indexes.empty()) {
        return;
    }

    PlaylistIndexes dequeuedIndexes;

    std::vector<int> sortedIndexes{indexes};
    std::ranges::sort(sortedIndexes, std::greater{}); // Reverse sort

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
        Q_EMIT trackIndexesDequeued(dequeuedIndexes);
        p->emitUpcomingTrackChangedIfNeeded();
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

    Q_EMIT trackQueueChanged(removed, tracks);
    p->emitUpcomingTrackChangedIfNeeded();
}

void PlayerController::clearPlaylistQueue(const UId& playlistId)
{
    const auto removedTracks = p->m_queue.removePlaylistTracks(playlistId);
    if(!removedTracks.empty()) {
        Q_EMIT tracksDequeued(removedTracks);
        p->emitUpcomingTrackChangedIfNeeded();
    }
}

void PlayerController::clearQueue()
{
    const auto removedTracks = p->m_queue.tracks();
    p->m_queue.clear();
    if(!removedTracks.empty()) {
        Q_EMIT tracksDequeued(removedTracks);
    }
    p->emitUpcomingTrackChangedIfNeeded();
}
} // namespace Fooyin

#include "core/player/moc_playercontroller.cpp"
