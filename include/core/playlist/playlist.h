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

#include <core/trackfwd.h>

#include <QObject>

namespace Fooyin {
class FYCORE_EXPORT Playlist
{
public:
    enum PlayMode : uint32_t
    {
        Default        = 0,
        RepeatPlaylist = 1 << 0,
        RepeatAlbum    = 1 << 1, // Not implemented
        RepeatTrack    = 1 << 2,
        ShuffleAlbums  = 1 << 3, // Not implemented
        ShuffleTracks  = 1 << 4,
        Random         = 1 << 5, // Not implemented
    };
    Q_DECLARE_FLAGS(PlayModes, PlayMode)

    Playlist();
    Playlist(int id, QString name, int index);
    ~Playlist();

    /** Returns @c true if this playlist has a valid id, index and name. */
    [[nodiscard]] bool isValid() const;

    [[nodiscard]] int id() const;
    [[nodiscard]] QString name() const;
    [[nodiscard]] int index() const;

    [[nodiscard]] TrackList tracks() const;
    [[nodiscard]] Track track(int index) const;
    [[nodiscard]] int trackCount() const;

    [[nodiscard]] int currentTrackIndex() const;
    [[nodiscard]] Track currentTrack() const;

    /** Returns @c true if this playlist's index or name have been changed. */
    [[nodiscard]] bool modified() const;
    /** Returns @c true if this playlist's tracks have been changed. */
    [[nodiscard]] bool tracksModified() const;

    /** Returns @c true if this playlist is visible. */
    [[nodiscard]] bool isVisible() const;

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

    void changeCurrentIndex(int index);

    /** Clears the shuffle order history */
    void reset();
    /** Resets the modified and tracksModified flags. */
    void resetFlags();

protected:
    friend class PlaylistHandler;

    void setId(int id);
    void setName(const QString& name);
    void setIndex(int index);

    void setModified(bool modified);
    void setTracksModified(bool modified);

    void replaceTracks(const TrackList& tracks);
    void appendTracks(const TrackList& tracks);
    std::vector<int> removeTracks(const std::vector<int>& indexes);

    /** Removes all tracks, including all shuffle order history */
    void clear();

private:
    struct Private;
    std::unique_ptr<Private> p;
};
using PlaylistList = std::vector<Playlist*>;
} // namespace Fooyin

Q_DECLARE_OPERATORS_FOR_FLAGS(Fooyin::Playlist::PlayModes)
