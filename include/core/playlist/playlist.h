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

#include <core/track.h>
#include <utils/id.h>

#include <QObject>

namespace Fooyin {
class PlaylistPrivate;
struct PlaylistTrack;
class SettingsManager;

using PlaylistTrackList = std::vector<PlaylistTrack>;

struct FYCORE_EXPORT PlaylistTrack
{
    Track track;
    UId playlistId;
    int indexInPlaylist{-1};

    [[nodiscard]] bool isValid() const;
    [[nodiscard]] bool isInPlaylist() const;

    static PlaylistTrackList fromTracks(const TrackList& tracks, const UId& playlistId);
    static TrackList toTracks(const PlaylistTrackList& playlistTracks);
    static PlaylistTrackList updateIndexes(const PlaylistTrackList& playlistTracks);

    static Track& extractor(PlaylistTrack& item);
    static const Track& extractorConst(const PlaylistTrack& item);

    bool operator==(const PlaylistTrack& other) const;
    bool operator!=(const PlaylistTrack& other) const;
    bool operator<(const PlaylistTrack& other) const;

    struct PlaylistTrackHash
    {
        size_t operator()(const PlaylistTrack& plTrack) const
        {
            return (std::hash<QString>{}(plTrack.track.uniqueFilepath()))
                 ^ (std::hash<int>{}(plTrack.indexInPlaylist) << 11) ^ (qHash(plTrack.playlistId) << 22);
        }
    };
};

/*!
 * Represents a list of tracks for playback.
 * Playlists are saved to the database and restored
 * on startup unless marked temporary.
 */
class FYCORE_EXPORT Playlist final
{
    struct PrivateKey;

public:
    enum PlayMode : uint16_t
    {
        Default        = 0,
        RepeatPlaylist = 1 << 0,
        RepeatAlbum    = 1 << 1,
        RepeatTrack    = 1 << 2,
        ShuffleAlbums  = 1 << 3,
        ShuffleTracks  = 1 << 4,
        Random         = 1 << 5,
    };
    Q_DECLARE_FLAGS(PlayModes, PlayMode)

    Playlist(PrivateKey, int dbId, QString name, int index, SettingsManager* settings);

    Playlist(const Playlist&)            = delete;
    Playlist& operator=(const Playlist&) = delete;

    ~Playlist();

    [[nodiscard]] UId id() const;
    [[nodiscard]] int dbId() const;
    [[nodiscard]] QString name() const;
    [[nodiscard]] int index() const;

    [[nodiscard]] TrackList tracks() const;
    [[nodiscard]] PlaylistTrackList playlistTracks() const;
    [[nodiscard]] std::optional<Track> track(int index) const;
    [[nodiscard]] std::optional<PlaylistTrack> playlistTrack(int index) const;
    [[nodiscard]] int trackCount() const;

    [[nodiscard]] int currentTrackIndex() const;
    [[nodiscard]] Track currentTrack() const;

    /** Returns @c true if this playlist's index or name have been changed. */
    [[nodiscard]] bool modified() const;
    /** Returns @c true if this playlist's tracks have been changed. */
    [[nodiscard]] bool tracksModified() const;
    /** Returns @c true if this playlist does not persist (saved to db). */
    [[nodiscard]] bool isTemporary() const;
    /** Returns @c true if this an autoplaylist (generated from a query). */
    [[nodiscard]] bool isAutoPlaylist() const;
    /** Returns the query used to generate this autoplaylist, else an empty string. */
    [[nodiscard]] QString query() const;

    /** Regenerates this autoplaylist using the tracks @p tracks. */
    bool regenerateTracks(const TrackList& tracks);

    /*!
     * Schedules the track to be played after the current track is finished.
     * @note this will be cancelled if tracks are replaced using @fn replaceTracks.
     */
    void scheduleNextIndex(int index);

    int nextIndex(int delta, PlayModes mode);
    /*!
     * Returns the next track to be played based on the @p delta from the current
     * index and the @p mode.
     * @note this will return an invalid track if @p mode is 'Default' and
     * the index +- delta is out of range.
     */
    Track nextTrack(int delta, PlayModes mode);
    /*!
     * Changes to and returns the next track to be played based on the @p delta from the current
     * index and the @p mode.
     * @note this will return an invalid track if @p mode is 'Default' and
     * the index +- delta is out of range.
     */
    Track nextTrackChange(int delta, PlayModes mode);

    void changeCurrentIndex(int index);

    /** Clears the shuffle order history */
    void reset();
    /** Resets the modified and tracksModified flags. */
    void resetFlags();
    void resetNextTrackIndex();

    static QStringList supportedPlaylistExtensions();

private:
    friend class PlaylistHandler;
    friend class PlaylistHandlerPrivate;

    static std::unique_ptr<Playlist> create(const QString& name, SettingsManager* settings);
    static std::unique_ptr<Playlist> create(int dbId, const QString& name, int index, SettingsManager* settings);
    static std::unique_ptr<Playlist> createAuto(int dbId, const QString& name, int index, const QString& query,
                                                SettingsManager* settings);

    void setName(const QString& name);
    void setIndex(int index);
    void setQuery(const QString& query);

    void setModified(bool modified);
    void setTracksModified(bool modified);

    void replaceTracks(const TrackList& tracks);
    void appendTracks(const TrackList& tracks);
    void updateTrackAtIndex(int index, const Track& track);
    std::vector<int> removeTracks(const std::vector<int>& indexes);

    std::unique_ptr<PlaylistPrivate> p;
};
using PlaylistList = std::vector<Playlist*>;
} // namespace Fooyin

Q_DECLARE_OPERATORS_FOR_FLAGS(Fooyin::Playlist::PlayModes)
