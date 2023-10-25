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

using namespace Qt::Literals::StringLiterals;

namespace Fy::Core::DB {
Playlist::Playlist(const QString& connectionName)
    : Module{connectionName}
{ }

bool Playlist::getAllPlaylists(Core::Playlist::PlaylistList& playlists)
{
    const QString query = u"SELECT PlaylistID, Name, PlaylistIndex FROM Playlists;"_s;

    Query q{this};
    q.prepareQuery(query);

    if(!q.execQuery()) {
        q.error(u"Cannot fetch all playlists"_s);
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
    const QString query = u"SELECT TrackID FROM PlaylistTracks WHERE PlaylistID=:playlistId ORDER BY TrackIndex;"_s;

    Query q{this};
    q.prepareQuery(query);
    q.bindQueryValue(u":playlistId"_s, id);

    if(!q.execQuery()) {
        q.error(u"Cannot fetch playlist tracks"_s);
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

    auto q = module()->insert(u"Playlists"_s, {{"Name", name}, {u"PlaylistIndex"_s, QString::number(index)}},
                              QString{u"Cannot insert playlist (name: %1, index: %2)"_s}.arg(name).arg(index));

    return (q.hasError()) ? -1 : q.lastInsertId().toInt();
}

bool Playlist::insertPlaylistTracks(int id, const TrackList& tracks)
{
    if(id < 0) {
        return false;
    }

    if(!db().transaction()) {
        qDebug() << "Transaction could not be started";
        return false;
    }

    auto delTracksQuery = module()->remove(u"PlaylistTracks"_s, {{u"PlaylistID"_s, QString::number(id)}},
                                           QString{u"Cannot remove old playlist %1 tracks"_s}.arg(id));

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
    auto q = module()->remove(u"Playlists"_s, {{u"PlaylistID"_s, QString::number(id)}},
                              "Cannot remove playlist " + QString::number(id));
    return !q.hasError();
}

bool Playlist::renamePlaylist(int id, const QString& name)
{
    if(name.isEmpty()) {
        return false;
    }

    auto q = module()->update(u"Playlists"_s, {{u"Name"_s, name}}, {u"PlaylistID"_s, QString::number(id)},
                              "Cannot update playlist " + QString::number(id));
    return !q.hasError();
}

bool Playlist::insertPlaylistTrack(int playlistId, const Track& track, int index)
{
    if(playlistId < 0 || !track.isValid()) {
        return false;
    }

    auto q = module()->insert(
        u"PlaylistTracks"_s,
        {{u"PlaylistID"_s, QString::number(playlistId)},
         {u"TrackID"_s, QString::number(track.id())},
         {u"TrackIndex"_s, QString::number(index)}},
        QString{u"Cannot insert into PlaylistTracks (PlaylistID: %1, TrackID: %2)"_s}.arg(playlistId).arg(track.id()));

    return !q.hasError();
}
} // namespace Fy::Core::DB
