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
    [[nodiscard]] virtual PlayState playState() const;

    /** Returns the current playlist mode (shuffle and repeat flags). */
    [[nodiscard]] virtual Playlist::PlayModes playMode() const;

    /** Returns the current playback position in ms. */
    [[nodiscard]] virtual uint64_t currentPosition() const;

    /*!
     * Returns the currently playing track.
     * @note the track will be invalid if the state is 'Stopped'.
     */
    [[nodiscard]] virtual Track currentTrack() const;
    /*!
     * Returns the currently playing playlist track.
     * @note the track will be invalid if the state is 'Stopped'.
     */
    [[nodiscard]] virtual PlaylistTrack currentPlaylistTrack() const;

    /** Starts playback of the current playlist. */
    virtual void play();

    /** Toggles playback. */
    virtual void playPause();

    virtual void pause();
    virtual void previous();
    virtual void next();
    virtual void stop();

    /** Stops playback and clears position and current track. */
    virtual void reset();

    virtual void setPlayMode(Playlist::PlayModes mode);
    virtual void setCurrentPosition(uint64_t ms);
    virtual void changePosition(uint64_t ms);
    virtual void changeCurrentTrack(const Track& track);
    virtual void changeCurrentTrack(const PlaylistTrack& track);
    virtual void updateCurrentTrackIndex(int index);

    [[nodiscard]] virtual PlaybackQueue playbackQueue() const;

    /** Queues the @p track to be played at the end of the current track. */
    virtual void queueTrack(const Track& track);
    virtual void queueTrack(const PlaylistTrack& track);
    virtual void queueTracks(const TrackList& tracks);
    virtual void queueTracks(const QueueTracks& tracks);

    virtual void dequeueTrack(const Track& track);
    virtual void dequeueTrack(const PlaylistTrack& track);
    virtual void dequeueTracks(const TrackList& tracks);
    virtual void dequeueTracks(const QueueTracks& tracks);

    virtual void replaceTracks(const QueueTracks& tracks);
    virtual void clearPlaylistQueue(int playlistId);

signals:
    void playStateChanged(PlayState state);
    void playModeChanged(Playlist::PlayModes mode);

    void nextTrack();
    void previousTrack();

    void positionChanged(uint64_t ms);
    void positionMoved(uint64_t ms);

    void currentTrackChanged(const Track& track);
    void playlistTrackChanged(const PlaylistTrack& track);
    void playlistTrackIndexChanged(const PlaylistTrack& track);
    void trackPlayed(const Track& track);

    void tracksQueued(const QueueTracks& tracks);
    void tracksDequeued(const QueueTracks& tracks);
    void trackQueueChanged(const QueueTracks& removed, const QueueTracks& added);

private:
    struct Private;
    std::unique_ptr<Private> p;
};
} // namespace Fooyin
