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

#include "playlistdatabase.h"

#include "databasequery.h"

namespace {
bool insertPlaylistTrack(Fooyin::DatabaseModule* module, int playlistId, const Fooyin::Track& track, int index)
{
    auto q = module->insert(QStringLiteral("PlaylistTracks"),
                            {{QStringLiteral("PlaylistID"), playlistId},
                             {QStringLiteral("TrackID"), track.id()},
                             {QStringLiteral("TrackIndex"), index}},
                            QString{QStringLiteral("Cannot insert into PlaylistTracks (PlaylistID: %1, TrackID: %2)")}
                                .arg(playlistId)
                                .arg(track.id()));

    return !q.hasError();
}

bool insertPlaylistTracks(Fooyin::DatabaseModule* module, int id, const Fooyin::TrackList& tracks)
{
    if(id < 0) {
        return false;
    }

    auto delTracksQuery = module->remove(QStringLiteral("PlaylistTracks"), {{QStringLiteral("PlaylistID"), id}},
                                         QString{QStringLiteral("Cannot remove old playlist %1 tracks")}.arg(id));

    if(delTracksQuery.hasError()) {
        return false;
    }

    for(int i{0}; const auto& track : tracks) {
        if(track.isValid() && track.isInDatabase()) {
            if(!insertPlaylistTrack(module, id, track, i++)) {
                return false;
            }
        }
    }

    return true;
}

Fooyin::TrackList populatePlaylistTracks(Fooyin::DatabaseModule* module, const Fooyin::Playlist& playlist,
                                         const Fooyin::TrackIdMap& tracks)
{
    const static QString query
        = QStringLiteral("SELECT TrackID FROM PlaylistTracks WHERE PlaylistID=:playlistId ORDER BY TrackIndex;");

    Fooyin::DatabaseQuery q{module};
    q.prepareQuery(query);
    q.bindQueryValue(QStringLiteral(":playlistId"), playlist.id());

    if(!q.execQuery()) {
        q.error(QStringLiteral("Cannot fetch playlist tracks"));
        return {};
    }

    Fooyin::TrackList playlistTracks;

    while(q.next()) {
        const int trackId = q.value(0).toInt();
        if(tracks.contains(trackId)) {
            playlistTracks.push_back(tracks.at(trackId));
        }
    }

    return playlistTracks;
}
} // namespace

namespace Fooyin {
PlaylistDatabase::PlaylistDatabase(const QString& connectionName)
    : DatabaseModule{connectionName}
{ }

std::vector<PlaylistInfo> PlaylistDatabase::getAllPlaylists()
{
    const QString query
        = QStringLiteral("SELECT PlaylistID, Name, PlaylistIndex FROM Playlists ORDER BY PlaylistIndex;");

    DatabaseQuery q{this};
    q.prepareQuery(query);

    if(!q.execQuery()) {
        q.error(QStringLiteral("Cannot fetch all playlists"));
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

TrackList PlaylistDatabase::getPlaylistTracks(const Playlist& playlist, const TrackIdMap& tracks)
{
    return populatePlaylistTracks(this, playlist, tracks);
}

int PlaylistDatabase::insertPlaylist(const QString& name, int index)
{
    if(name.isEmpty() || index < 0) {
        return -1;
    }

    auto q = insert(QStringLiteral("Playlists"),
                    {{QStringLiteral("Name"), name}, {QStringLiteral("PlaylistIndex"), index}},
                    QString{QStringLiteral("Cannot insert playlist (name: %1, index: %2)")}.arg(name).arg(index));

    return (q.hasError()) ? -1 : q.lastInsertId().toInt();
}

bool PlaylistDatabase::savePlaylist(Playlist& playlist)
{
    bool updated{false};

    if(playlist.modified()) {
        const auto q = update(
            QStringLiteral("Playlists"),
            {{QStringLiteral("Name"), playlist.name()}, {QStringLiteral("PlaylistIndex"), playlist.index()}},
            {QStringLiteral("PlaylistID"), playlist.id()}, QStringLiteral("Cannot update playlist ") + playlist.name());
        updated = !q.hasError();

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
    auto q = remove(QStringLiteral("Playlists"), {{QStringLiteral("PlaylistID"), id}},
                    QStringLiteral("Cannot remove playlist ") + QString::number(id));
    return !q.hasError();
}

bool PlaylistDatabase::renamePlaylist(int id, const QString& name)
{
    if(name.isEmpty()) {
        return false;
    }

    auto q = update(QStringLiteral("Playlists"), {{QStringLiteral("Name"), name}}, {QStringLiteral("PlaylistID"), id},
                    QStringLiteral("Cannot update playlist ") + name);
    return !q.hasError();
}
} // namespace Fooyin
