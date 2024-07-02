/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
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

#include "fycore_export.h"

#include <core/player/playbackqueue.h>
#include <core/player/playerdefs.h>
#include <core/playlist/playlist.h>

#include <QObject>

namespace Fooyin {
class SettingsManager;

/*!
 * Handles track playback
 */
class FYCORE_EXPORT PlayerController : public QObject
{
    Q_OBJECT

public:
    explicit PlayerController(SettingsManager* settings, QObject* parent = nullptr);
    ~PlayerController() override;

    /** Returns the current state (playing, paused or stopped). */
    [[nodiscard]] PlayState playState() const;

    /** Returns the current playlist mode (shuffle and repeat flags). */
    [[nodiscard]] Playlist::PlayModes playMode() const;

    /** Returns the current playback position in ms. */
    [[nodiscard]] uint64_t currentPosition() const;

    /*!
     * Returns the currently playing track.
     * @note the track will be invalid if the state is 'Stopped'.
     */
    [[nodiscard]] Track currentTrack() const;
    /** Returns the current track id, or -1 if there isn't a valid tracks. */
    [[nodiscard]] int currentTrackId() const;
    /** Returns @c true if the current track is part of the queue. */
    [[nodiscard]] bool currentIsQueueTrack() const;
    /*!
     * Returns the currently playing playlist track.
     * @note the track will be invalid if the state is 'Stopped'.
     */
    [[nodiscard]] PlaylistTrack currentPlaylistTrack() const;

    /** Starts playback of the current playlist. */
    void play();

    /** Toggles playback. */
    void playPause();

    void pause();
    void previous();
    void next();
    void stop();

    /** Stops playback and clears position and current track. */
    void reset();

    void setPlayMode(Playlist::PlayModes mode);
    void seek(uint64_t ms);
    void seekForward(uint64_t delta);
    void seekBackward(uint64_t delta);
    void setCurrentPosition(uint64_t ms);
    void changeCurrentTrack(const Track& track);
    void changeCurrentTrack(const PlaylistTrack& track);
    void updateCurrentTrackPlaylist(const Id& playlistId);
    void updateCurrentTrackIndex(int index);

    [[nodiscard]] PlaybackQueue playbackQueue() const;

    /** Queues the @p track to be played at the end of the current track. */
    void queueTrack(const Track& track);
    void queueTrack(const PlaylistTrack& track);
    void queueTracks(const TrackList& tracks);
    void queueTracks(const QueueTracks& tracks);

    void dequeueTrack(const Track& track);
    void dequeueTrack(const PlaylistTrack& track);
    void dequeueTracks(const TrackList& tracks);
    void dequeueTracks(const QueueTracks& tracks);
    void dequeueTracks(const std::vector<int>& indexes);

    void replaceTracks(const QueueTracks& tracks);
    void clearPlaylistQueue(const Id& playlistId);

signals:
    void playStateChanged(PlayState state);
    void playModeChanged(Playlist::PlayModes mode);

    void nextTrack();
    void previousTrack();

    void positionChanged(uint64_t ms);
    void positionMoved(uint64_t ms);

    void currentTrackChanged(const Track& track);
    void playlistTrackChanged(const PlaylistTrack& track);
    void trackPlayed(const Track& track);

    void tracksQueued(const QueueTracks& tracks);
    void tracksDequeued(const QueueTracks& tracks);
    void trackIndexesDequeued(const std::vector<int>& indexes);
    void trackQueueChanged(const QueueTracks& removed, const QueueTracks& added);

private:
    struct Private;
    std::unique_ptr<Private> p;
};
} // namespace Fooyin
