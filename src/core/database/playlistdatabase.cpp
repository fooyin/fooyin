/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <luket@pm.me>
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

#include "playlistdatabase.h"

#include "databasehelpers.h"

#include <utils/database/dbquery.h>
#include <utils/database/dbtransaction.h>

using namespace Qt::StringLiterals;

namespace Fooyin {
std::vector<PlaylistInfo> PlaylistDatabase::getAllPlaylists()
{
    static const QString query
        = u"SELECT PlaylistID, Name, PlaylistIndex, IsAutoPlaylist, Query, SortQuery, ForceSorted "
          "FROM Playlists ORDER BY PlaylistIndex;"_s;

    DbQuery q{db(), query};

    if(!q.exec()) {
        return {};
    }

    std::vector<PlaylistInfo> playlists;

    while(q.next()) {
        PlaylistInfo playlist;

        playlist.dbId           = q.value(0).toInt();
        playlist.name           = q.value(1).toString();
        playlist.index          = q.value(2).toInt();
        playlist.isAutoPlaylist = q.value(3).toBool();
        playlist.query          = q.value(4).toString();
        playlist.sortQuery      = q.value(5).toString();
        playlist.forceSorted    = q.value(6).toBool();

        playlists.emplace_back(playlist);
    }

    return playlists;
}

TrackList PlaylistDatabase::getPlaylistTracks(const Playlist& playlist, const std::unordered_map<int, Track>& tracks)
{
    return populatePlaylistTracks(playlist, tracks);
}

int PlaylistDatabase::insertPlaylist(const QString& name, int index, bool isAutoPlaylist, const QString& autoQuery,
                                     const QString& autoSortQuery, bool forceSorted)
{
    if(name.isEmpty() || index < 0) {
        return -1;
    }

    static const QString statement = u"INSERT INTO Playlists (Name, PlaylistIndex, IsAutoPlaylist, Query, SortQuery, "
                                     "ForceSorted) VALUES (:name, :index, :isAutoPlaylist, :query, :sortQuery, "
                                     ":forceSorted);"_s;

    DbQuery query{db(), statement};
    query.bindValue(u":name"_s, name);
    query.bindValue(u":index"_s, index);
    query.bindValue(u":isAutoPlaylist"_s, isAutoPlaylist);
    query.bindValue(u":query"_s, autoQuery);
    query.bindValue(u":sortQuery"_s, autoSortQuery);
    query.bindValue(u":forceSorted"_s, forceSorted);

    if(!query.exec()) {
        return -1;
    }

    return query.lastInsertId().toInt();
}

bool PlaylistDatabase::savePlaylist(Playlist& playlist)
{
    bool updated{false};

    if(playlist.modified()) {
        static const QString statement = u"UPDATE Playlists SET Name = :name, PlaylistIndex = :index, IsAutoPlaylist = "
                                         ":isAutoPlaylist, Query = :query, SortQuery = :sortQuery, "
                                         "ForceSorted = :forceSorted WHERE PlaylistID = :id;"_s;

        DbQuery query{db(), statement};

        query.bindValue(u":name"_s, playlist.name());
        query.bindValue(u":index"_s, playlist.index());
        query.bindValue(u":isAutoPlaylist"_s, playlist.isAutoPlaylist());
        query.bindValue(u":query"_s, playlist.query());
        query.bindValue(u":sortQuery"_s, playlist.sortQuery());
        query.bindValue(u":forceSorted"_s, playlist.forceSorted());
        query.bindValue(u":id"_s, playlist.dbId());

        updated = query.exec();
    }

    if(playlist.tracksModified()) {
        updated = insertPlaylistTracks(playlist.dbId(), playlist.tracks());
    }

    if(updated) {
        playlist.resetFlags();
        return true;
    }

    return false;
}

bool PlaylistDatabase::saveModifiedPlaylists(const PlaylistList& playlists)
{
    DbTransaction transaction{db()};

    for(const auto& playlist : playlists) {
        savePlaylist(*playlist);
    }

    return transaction.commit();
}

bool PlaylistDatabase::removePlaylist(int id)
{
    static const QString statement = u"DELETE FROM Playlists WHERE PlaylistID = :id;"_s;

    DbQuery query{db(), statement};
    query.bindValue(u":id"_s, id);

    return query.exec();
}

bool PlaylistDatabase::renamePlaylist(int id, const QString& name)
{
    if(name.isEmpty()) {
        return false;
    }

    static const QString statement = u"UPDATE Playlists SET Name = :name WHERE PlaylistID = :id;"_s;

    DbQuery query{db(), statement};
    query.bindValue(u":name"_s, name);
    query.bindValue(u":id"_s, id);

    return query.exec();
}

Track PlaylistDatabase::ensureTrack(Track track) const
{
    if(!track.isValid() || track.id() >= 0) {
        return track;
    }

    track.generateHash();

    static const QString insertStatement = u"INSERT OR IGNORE INTO Tracks ("
                                           "FilePath,"
                                           "Subsong,"
                                           "Title,"
                                           "TrackNumber,"
                                           "TrackTotal,"
                                           "Artists,"
                                           "AlbumArtist,"
                                           "Album,"
                                           "DiscNumber,"
                                           "DiscTotal,"
                                           "Date,"
                                           "Composer,"
                                           "Performer,"
                                           "Genres,"
                                           "Comment,"
                                           "CuePath,"
                                           "Offset,"
                                           "Duration,"
                                           "FileSize,"
                                           "BitRate,"
                                           "SampleRate,"
                                           "Channels,"
                                           "BitDepth,"
                                           "Codec,"
                                           "CodecProfile,"
                                           "Tool,"
                                           "TagTypes,"
                                           "Encoding,"
                                           "ExtraTags,"
                                           "ExtraProperties,"
                                           "ModifiedDate,"
                                           "TrackHash,"
                                           "LibraryID,"
                                           "RGTrackGain,"
                                           "RGAlbumGain,"
                                           "RGTrackPeak,"
                                           "RGAlbumPeak,"
                                           "CreatedDate"
                                           ") "
                                           "VALUES ("
                                           ":filePath,"
                                           ":subsong,"
                                           ":title,"
                                           ":trackNumber,"
                                           ":trackTotal,"
                                           ":artists,"
                                           ":albumArtist,"
                                           ":album,"
                                           ":discNumber,"
                                           ":discTotal,"
                                           ":date,"
                                           ":composer,"
                                           ":performer,"
                                           ":genres,"
                                           ":comment,"
                                           ":cuePath,"
                                           ":offset,"
                                           ":duration,"
                                           ":fileSize,"
                                           ":bitRate,"
                                           ":sampleRate,"
                                           ":channels,"
                                           ":bitDepth,"
                                           ":codec,"
                                           ":codecProfile,"
                                           ":tool,"
                                           ":tagTypes,"
                                           ":encoding,"
                                           ":extraTags,"
                                           ":extraProperties,"
                                           ":modifiedDate,"
                                           ":trackHash,"
                                           ":libraryID,"
                                           ":rgTrackGain,"
                                           ":rgAlbumGain,"
                                           ":rgTrackPeak,"
                                           ":rgAlbumPeak,"
                                           ":createdDate"
                                           ");"_s;

    DbQuery insertQuery{db(), insertStatement};

    const auto bindings = Database::trackBindings(track);
    for(const auto& [name, value] : bindings) {
        insertQuery.bindValue(name, value);
    }

    if(!insertQuery.exec()) {
        return track;
    }

    if(insertQuery.numRowsAffected() > 0) {
        track.setId(insertQuery.lastInsertId().toInt());
        return track;
    }

    static const QString idStatement
        = u"SELECT TrackID FROM Tracks WHERE FilePath = :path AND Offset = :offset AND Subsong = :subsong;"_s;

    DbQuery idQuery{db(), idStatement};
    idQuery.bindValue(u":path"_s, track.filepath());
    idQuery.bindValue(u":offset"_s, static_cast<quint64>(track.offset()));
    idQuery.bindValue(u":subsong"_s, track.subsong());

    if(!idQuery.exec() || !idQuery.next()) {
        return track;
    }

    track.setId(idQuery.value(0).toInt());
    return track;
}

bool PlaylistDatabase::insertPlaylistTrack(int playlistId, int trackId, int index)
{
    static const QString statement
        = u"INSERT INTO PlaylistTracks (PlaylistID, TrackID, TrackIndex) VALUES (:playlistId, :trackId, :index);"_s;

    DbQuery query{db(), statement};
    query.bindValue(u":playlistId"_s, playlistId);
    query.bindValue(u":trackId"_s, trackId);
    query.bindValue(u":index"_s, index);

    return query.exec();
}

bool PlaylistDatabase::insertPlaylistTracks(int playlistId, const TrackList& tracks)
{
    if(playlistId < 0) {
        return false;
    }

    // Remove current playlist tracks
    static const QString statement = u"DELETE FROM PlaylistTracks WHERE PlaylistID = :id;"_s;

    DbQuery query{db(), statement};
    query.bindValue(u":id"_s, playlistId);

    if(!query.exec()) {
        return false;
    }

    for(int i{0}; const auto& track : tracks) {
        const Track storedTrack = ensureTrack(track);
        if(storedTrack.isValid() && storedTrack.isInDatabase()) {
            if(!insertPlaylistTrack(playlistId, storedTrack.id(), i++)) {
                return false;
            }
        }
    }

    return true;
}

TrackList PlaylistDatabase::populatePlaylistTracks(const Playlist& playlist,
                                                   const std::unordered_map<int, Track>& tracks)
{
    static const QString statement
        = u"SELECT TrackID FROM PlaylistTracks WHERE PlaylistID=:playlistId ORDER BY TrackIndex;"_s;

    DbQuery query{db(), statement};
    query.bindValue(u":playlistId"_s, playlist.dbId());

    if(!query.exec()) {
        return {};
    }

    TrackList playlistTracks;

    while(query.next()) {
        const int trackId = query.value(0).toInt();
        if(tracks.contains(trackId)) {
            playlistTracks.push_back(tracks.at(trackId));
        }
    }

    return playlistTracks;
}
} // namespace Fooyin
