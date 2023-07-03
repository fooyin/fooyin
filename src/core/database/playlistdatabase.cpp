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

        playlists.emplace_back(name, index, id);
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

    auto q = module()->insert("Playlists", {{"Name", name}, {"PlaylistIndex", QString::number(index)}},
                              QString{"Cannot insert playlist (name: %1, index: %2)"}.arg(name).arg(index));

    return (q.hasError()) ? -1 : q.lastInsertId().toInt();
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

    auto delTracksQuery = module()->remove("PlaylistTracks", {{"PlaylistID", QString::number(id)}},
                                           QString{"Cannot remove old playlist %1 tracks"}.arg(id));

    if(delTracksQuery.hasError()) {
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
    auto q = module()->remove("Playlists", {{"PlaylistID", QString::number(id)}},
                              QString{"Cannot remove playlist %1"}.arg(id));
    return !q.hasError();
}

bool Playlist::renamePlaylist(int id, const QString& name)
{
    if(name.isEmpty()) {
        return false;
    }

    auto q = module()->update("Playlists", {{"Name", name}}, {"PlaylistID", QString::number(id)},
                              QString{"Cannot update playlist %1"}.arg(id));
    return !q.hasError();
}

bool Playlist::insertPlaylistTrack(int playlistId, const Track& track, int index)
{
    if(playlistId < 0 || !track.isValid()) {
        return false;
    }

    auto q = module()->insert(
        "PlaylistTracks",
        {{"PlaylistID", QString::number(playlistId)},
         {"TrackID", QString::number(track.id())},
         {"TrackIndex", QString::number(index)}},
        QString{"Cannot insert into PlaylistTracks (PlaylistID: %1, TrackID: %2)"}.arg(playlistId).arg(track.id()));

    return !q.hasError();
}
} // namespace Fy::Core::DB
