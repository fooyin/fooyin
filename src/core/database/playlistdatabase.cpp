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

#include "playlistdatabase.h"

#include "query.h"

namespace Fy::Core::DB {
Playlist::Playlist(const QString& connectionName)
    : Module{connectionName}
{ }

bool Playlist::getAllPlaylists(Core::Playlist::PlaylistList& playlists)
{
    const QString query = "SELECT PlaylistID, Name, PlaylistIndex FROM Playlists;";

    Query q{this};
    q.prepareQuery(query);

    if(!q.execQuery()) {
        q.error("Cannot fetch all playlists");
        return false;
    }

    while(q.next()) {
        const int id       = q.value(0).toInt();
        const QString name = q.value(1).toString();
        const int index    = q.value(2).toInt();

        playlists.emplace_back(std::make_unique<Core::Playlist::Playlist>(name, index, id));
    }
    return true;
}

bool Playlist::getPlaylistTracks(int id, std::vector<int>& ids)
{
    const QString query = "SELECT TrackID FROM PlaylistTracks WHERE PlaylistID=:playlistId ORDER BY TrackIndex;";

    Query q{this};
    q.prepareQuery(query);
    q.bindQueryValue(":playlistId", id);

    if(!q.execQuery()) {
        q.error("Cannot fetch playlist tracks");
        return false;
    }

    while(q.next()) {
        ids.emplace_back(q.value(0).toInt());
    }
    return true;
}

int Playlist::insertPlaylist(const QString& name, int index)
{
    if(name.isEmpty() || index < 0) {
        return -1;
    }

    const QString query = "INSERT INTO Playlists "
                          "(Name, PlaylistIndex) "
                          "VALUES "
                          "(:playlistName, :playlistIndex);";

    Query q{this};

    q.prepareQuery(query);
    q.bindQueryValue(":playlistName", name);
    q.bindQueryValue(":playlistIndex", index);

    if(!q.execQuery()) {
        q.error(QString("Cannot insert playlist (name: %1, index: %2)").arg(name).arg(index));
        return -1;
    }
    return q.lastInsertId().toInt();
}

bool Playlist::insertPlaylistTracks(int id, const TrackList& tracks)
{
    if(id < 0 || tracks.empty()) {
        return false;
    }

    if(!db().transaction()) {
        qDebug() << "Transaction could not be started";
        return false;
    }

    // Remove old tracks first
    Query delTracks{this};
    const QString delPlaylistQuery = "DELETE FROM PlaylistTracks WHERE PlaylistID=:playlistId;";
    delTracks.prepareQuery(delPlaylistQuery);
    delTracks.bindQueryValue(":playlistId", id);

    if(!delTracks.execQuery()) {
        delTracks.error(QString{"Cannot remove old playlist %1 tracks"}.arg(id));
        return false;
    }

    for(auto [it, end, i] = std::tuple{tracks.cbegin(), tracks.cend(), 0}; it != end; ++it, ++i) {
        const Track& track = *it;
        if(!track.isValid() || !track.enabled()) {
            continue;
        }
        insertPlaylistTrack(id, track, i);
    }

    if(!db().commit()) {
        qDebug() << "Transaction could not be commited";
        return false;
    }

    return true;
}

bool Playlist::removePlaylist(int id)
{
    //    Query delTracks(this);
    //    auto delTracksQuery = QStringLiteral("DELETE FROM PlaylistTracks WHERE PlaylistID=:playlistId;");
    //    delTracks.prepareQuery(delTracksQuery);
    //    delTracks.bindQueryValue(":playlistId", id);

    //    if(!delTracks.execQuery()) {
    //        delTracks.error(QString{"Cannot delete playlist (%1) tracks"}.arg(id));
    //        return false;
    //    }

    Query delPlaylist{this};
    const QString delPlaylistQuery = "DELETE FROM Playlists WHERE PlaylistID=:playlistId;";
    delPlaylist.prepareQuery(delPlaylistQuery);
    delPlaylist.bindQueryValue(":playlistId", id);

    if(!delPlaylist.execQuery()) {
        delPlaylist.error(QString{"Cannot remove playlist %1"}.arg(id));
        return false;
    }
    return true;
}

bool Playlist::renamePlaylist(int id, const QString& name)
{
    if(name.isEmpty()) {
        return false;
    }

    const QString query = "UPDATE Playlists "
                          "SET Name = :playlistName "
                          "WHERE PlaylistID=:playlistId;";

    Query q{this};
    q.prepareQuery(query);
    q.bindQueryValue(":playlistId", id);
    q.bindQueryValue(":playlistName", name);

    if(!q.execQuery()) {
        q.error(QString{"Cannot update playlist (name: %1)"}.arg(name));
        return false;
    }
    return true;
}

bool Playlist::insertPlaylistTrack(int playlistId, const Track& track, int index)
{
    if(playlistId < 0 || !track.isValid()) {
        return false;
    }

    const QString query = "INSERT INTO PlaylistTracks "
                          "(PlaylistID, TrackID, TrackIndex) "
                          "VALUES "
                          "(:playlistId, :trackId, :trackIndex);";

    Query q{this};
    q.prepareQuery(query);
    q.bindQueryValue(":playlistId", playlistId);
    q.bindQueryValue(":trackId", track.id());
    q.bindQueryValue(":trackIndex", index);

    if(!q.execQuery()) {
        q.error(QString("Cannot insert into PlaylistTracks (PlaylistID: %1, TrackID: %2)").arg(playlistId, track.id()));
        return false;
    }
    return true;
}
} // namespace Fy::Core::DB
