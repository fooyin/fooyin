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

#include "databasequery.h"

using namespace Qt::Literals::StringLiterals;

namespace {
bool insertPlaylistTrack(Fooyin::DatabaseModule* module, int playlistId, const Fooyin::Track& track, int index)
{
    if(playlistId < 0 || !track.isValid()) {
        return false;
    }

    auto q = module->insert(
        u"PlaylistTracks"_s,
        {{u"PlaylistID"_s, QString::number(playlistId)},
         {u"TrackID"_s, QString::number(track.id())},
         {u"TrackIndex"_s, QString::number(index)}},
        QString{u"Cannot insert into PlaylistTracks (PlaylistID: %1, TrackID: %2)"_s}.arg(playlistId).arg(track.id()));

    return !q.hasError();
}

bool insertPlaylistTracks(Fooyin::DatabaseModule* module, int id, const Fooyin::TrackList& tracks)
{
    if(id < 0) {
        return false;
    }

    auto delTracksQuery = module->remove(u"PlaylistTracks"_s, {{u"PlaylistID"_s, QString::number(id)}},
                                         QString{u"Cannot remove old playlist %1 tracks"_s}.arg(id));

    if(delTracksQuery.hasError()) {
        return false;
    }

    for(int i{0}; const auto& track : tracks) {
        if(track.isValid()) {
            if(!insertPlaylistTrack(module, id, track, i++)) {
                return false;
            }
        }
    }

    return true;
}

bool populatePlaylistTracks(Fooyin::DatabaseModule* module, const auto& playlist, const Fooyin::TrackIdMap& tracks)
{
    const static QString query
        = u"SELECT TrackID FROM PlaylistTracks WHERE PlaylistID=:playlistId ORDER BY TrackIndex;"_s;

    Fooyin::DatabaseQuery q{module};
    q.prepareQuery(query);
    q.bindQueryValue(u":playlistId"_s, playlist->id());

    if(!q.execQuery()) {
        q.error(u"Cannot fetch playlist tracks"_s);
        return false;
    }

    Fooyin::TrackList playlistTracks;

    while(q.next()) {
        const int trackId = q.value(0).toInt();
        if(tracks.contains(trackId)) {
            playlistTracks.push_back(tracks.at(trackId));
        }
    }

    playlist->appendTracksSilently(playlistTracks);
    return true;
}
} // namespace

namespace Fooyin {
PlaylistDatabase::PlaylistDatabase(const QString& connectionName)
    : DatabaseModule{connectionName}
{ }

std::vector<PlaylistInfo> PlaylistDatabase::getAllPlaylists()
{
    const QString query = u"SELECT PlaylistID, Name, PlaylistIndex FROM Playlists ORDER BY PlaylistIndex;"_s;

    DatabaseQuery q{this};
    q.prepareQuery(query);

    if(!q.execQuery()) {
        q.error(u"Cannot fetch all playlists"_s);
        return {};
    }

    std::vector<PlaylistInfo> playlists;

    while(q.next()) {
        const int id       = q.value(0).toInt();
        const QString name = q.value(1).toString();
        const int index    = q.value(2).toInt();

        playlists.emplace_back(id, name, index);
    }

    return playlists;
}

bool PlaylistDatabase::getPlaylistTracks(const PlaylistList& playlists, const TrackIdMap& tracks)
{
    for(const auto& playlist : playlists) {
        populatePlaylistTracks(this, playlist, tracks);
    }

    return true;
}

int PlaylistDatabase::insertPlaylist(const QString& name, int index)
{
    if(name.isEmpty() || index < 0) {
        return -1;
    }

    auto q = insert(u"Playlists"_s, {{"Name", name}, {u"PlaylistIndex"_s, QString::number(index)}},
                    QString{u"Cannot insert playlist (name: %1, index: %2)"_s}.arg(name).arg(index));

    return (q.hasError()) ? -1 : q.lastInsertId().toInt();
}

bool PlaylistDatabase::savePlaylist(Playlist& playlist)
{
    if(!playlist.modified() && !playlist.tracksModified()) {
        return true;
    }

    bool updated{false};

    if(playlist.modified()) {
        const auto q = update(u"Playlists"_s,
                              {{u"Name"_s, playlist.name()}, {u"PlaylistIndex"_s, QString::number(playlist.index())}},
                              {u"PlaylistID"_s, QString::number(playlist.id())},
                              "Cannot update playlist " + QString::number(playlist.id()));
        updated      = !q.hasError();

        if(!updated) {
            return false;
        }
    }

    if(playlist.tracksModified()) {
        updated = insertPlaylistTracks(this, playlist.id(), playlist.tracks());
    }

    if(updated) {
        playlist.resetFlags();
        return true;
    }

    return false;
}

bool PlaylistDatabase::saveModifiedPlaylists(const PlaylistList& playlists)
{
    if(!db().transaction()) {
        qDebug() << "Transaction could not be started";
        return false;
    }

    for(const auto& playlist : playlists) {
        savePlaylist(*playlist);
    }

    if(!db().commit()) {
        qDebug() << "Transaction could not be commited";
        return false;
    }

    return true;
}

bool PlaylistDatabase::removePlaylist(int id)
{
    auto q = remove(u"Playlists"_s, {{u"PlaylistID"_s, QString::number(id)}},
                    "Cannot remove playlist " + QString::number(id));
    return !q.hasError();
}

bool PlaylistDatabase::renamePlaylist(int id, const QString& name)
{
    if(name.isEmpty()) {
        return false;
    }

    auto q = update(u"Playlists"_s, {{u"Name"_s, name}}, {u"PlaylistID"_s, QString::number(id)},
                    "Cannot update playlist " + QString::number(id));
    return !q.hasError();
}
} // namespace Fooyin
