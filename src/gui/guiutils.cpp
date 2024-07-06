/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <LukeT1@proton.me>
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

#include "guiutils.h"

#include <core/library/musiclibrary.h>
#include <utils/datastream.h>

#include <QIODevice>

namespace Fooyin::Gui {
TrackList tracksFromMimeData(MusicLibrary* library, QByteArray data)
{
    QDataStream stream(&data, QIODevice::ReadOnly);

    TrackIds ids;
    stream >> ids;
    TrackList tracks = library->tracksForIds(ids);

    return tracks;
}

QByteArray queueTracksToMimeData(const QueueTracks& tracks)
{
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);

    for(const auto& track : tracks) {
        stream << track.track.id();
        stream << track.playlistId;
        stream << track.indexInPlaylist;
    }

    return data;
}

QueueTracks queueTracksFromMimeData(MusicLibrary* library, QByteArray data)
{
    QDataStream stream(&data, QIODevice::ReadOnly);

    QueueTracks tracks;

    while(!stream.atEnd()) {
        PlaylistTrack track;

        int id{-1};
        stream >> id;
        stream >> track.playlistId;
        stream >> track.indexInPlaylist;

        track.track = library->trackForId(id);
        tracks.push_back(track);
    }

    return tracks;
}
} // namespace Fooyin::Gui
