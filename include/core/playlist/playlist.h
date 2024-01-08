/*
 * Fooyin
 * Copyright 2022-2023, Luke Taylor <LukeT1@proton.me>
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

#include <core/trackfwd.h>

#include <QObject>

#include <set>

namespace Fooyin {
using IndexSet = std::set<int>;

class FYCORE_EXPORT Playlist
{
public:
    enum PlayMode
    {
        Default   = 0,
        RepeatAll = 1 << 0,
        Repeat    = 1 << 1,
        Shuffle   = 1 << 2,
    };
    Q_DECLARE_FLAGS(PlayModes, PlayMode)

    virtual ~Playlist() = default;

    /** Returns @c true if this playlist has a valid id, index and name. */
    virtual bool isValid() const = 0;

    virtual int id() const       = 0;
    virtual int index() const    = 0;
    virtual QString name() const = 0;

    virtual TrackList tracks() const                    = 0;
    virtual std::optional<Track> track(int index) const = 0;
    virtual int trackCount() const                      = 0;

    virtual int currentTrackIndex() const = 0;
    virtual Track currentTrack() const    = 0;

    /** Returns @c true if this playlist's index or name have been changed. */
    virtual bool modified() const = 0;
    /** Returns @c true if this playlist's tracks have been changed. */
    virtual bool tracksModified() const = 0;

    /*!
     * Schedules the track to be played after the current track is finished.
     * @note this will be cancelled if tracks are replaced using @fn replaceTracks.
     */
    virtual void scheduleNextIndex(int index) = 0;

    /*!
     * Returns the next track to be played based on the @p delta from the current
     * index and the @p mode.
     * @note this will return an invalid track if @p mode is 'Default' and
     * the index +- delta is out of range.
     */
    virtual Track nextTrack(int delta, PlayModes mode) = 0;

    virtual void setIndex(int index)          = 0;
    virtual void setName(const QString& name) = 0;

    virtual void replaceTracks(const TrackList& tracks)         = 0;
    virtual void replaceTracksSilently(const TrackList& tracks) = 0;
    virtual void appendTracks(const TrackList& tracks)          = 0;
    virtual void appendTracksSilently(const TrackList& tracks)  = 0;
    virtual void removeTracks(const IndexSet& indexes)          = 0;

    /** Removes all tracks, including all shuffle order history */
    virtual void clear() = 0;
    /** Clears the shuffle order history */
    virtual void reset() = 0;
    /** Resets the modified and tracksModified flags. */
    virtual void resetFlags() = 0;

    virtual void changeCurrentTrack(int index) = 0;
};
using PlaylistList = std::vector<Playlist*>;
} // namespace Fooyin

Q_DECLARE_OPERATORS_FOR_FLAGS(Fooyin::Playlist::PlayModes)
