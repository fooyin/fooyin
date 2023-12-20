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

    Playlist(int id, QString name, int index);
    ~Playlist();

    /** Returns @c true if this playlist has a valid id, index and name. */
    [[nodiscard]] bool isValid() const;

    [[nodiscard]] int id() const;
    [[nodiscard]] int index() const;
    [[nodiscard]] QString name() const;

    [[nodiscard]] TrackList tracks() const;
    [[nodiscard]] int trackCount() const;

    [[nodiscard]] int currentTrackIndex() const;
    [[nodiscard]] Track currentTrack() const;

    /** Returns @c true if this playlist's index or name have been changed. */
    [[nodiscard]] bool modified() const;
    /** Returns @c true if this playlist's tracks have been changed. */
    [[nodiscard]] bool tracksModified() const;

    /*!
     * Schedules the track to be played after the current track is finished.
     * @note this will be cancelled if tracks are replaced using @fn replaceTracks.
     */
    void scheduleNextIndex(int index);

    /*!
     * Returns the next track to be played based on the @p delta from the current
     * index and the @p mode.
     * @note this will return an invalid track if @p mode is 'Default' and
     * the index +- delta is out of range.
     */
    Track nextTrack(int delta, PlayModes mode);

    void setIndex(int index);
    void setName(const QString& name);

    void replaceTracks(const TrackList& tracks);
    void replaceTracksSilently(const TrackList& tracks);
    void appendTracks(const TrackList& tracks);
    void appendTracksSilently(const TrackList& tracks);
    void removeTracks(const IndexSet& indexes);

    /** Removes all tracks, including all shuffle order history */
    void clear();
    /** Clears the shuffle order history */
    void reset();
    /** Resets the modified and tracksModified flags. */
    void resetFlags();

    void changeCurrentTrack(int index);

private:
    struct Private;
    std::unique_ptr<Private> p;
};
using PlaylistList = std::vector<Playlist*>;
} // namespace Fooyin

Q_DECLARE_OPERATORS_FOR_FLAGS(Fooyin::Playlist::PlayModes)
