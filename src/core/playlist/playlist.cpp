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

#include "playlist.h"

#include <QMessageBox>

namespace Fy::Core::Playlist {
Playlist::Playlist(QString name, int index, int id)
    : m_id{id}
    , m_index{index}
    , m_name{std::move(name)}
    , m_currentTrackIndex{-1}
    , m_modified{false}
{ }

int Playlist::id() const
{
    return m_id;
}

int Playlist::index() const
{
    return m_index;
}

void Playlist::setIndex(int index)
{
    m_index = index;
}

QString Playlist::name() const
{
    return m_name;
}

void Playlist::setName(const QString& name)
{
    m_name = name;
}

TrackList Playlist::tracks() const
{
    return m_tracks;
}

int Playlist::currentTrackIndex() const
{
    return m_currentTrackIndex;
}

Track Playlist::currentTrack() const
{
    if(m_currentTrackIndex >= trackCount() || m_currentTrackIndex < 0) {
        return {};
    }
    return m_tracks.at(m_currentTrackIndex);
}

// Playlist tracks were changed
bool Playlist::wasModified() const
{
    return m_modified;
}

void Playlist::replaceTracks(const TrackList& tracks)
{
    m_tracks   = tracks;
    m_modified = true;
}

void Playlist::appendTracks(const TrackList& tracks)
{
    m_tracks.insert(m_tracks.end(), tracks.begin(), tracks.end());
    m_modified = true;
}

void Playlist::clear()
{
    m_tracks.clear();
    m_modified = true;
}

void Playlist::changeCurrentTrack(int index)
{
    m_currentTrackIndex = index;
}

void Playlist::changeCurrentTrack(const Track& track)
{
    const int index = findTrack(track);
    if(index >= 0) {
        m_currentTrackIndex = index;
    }
}

int Playlist::findTrack(const Track& track)
{
    auto it = std::find_if(m_tracks.cbegin(), m_tracks.cend(), [track](const Track& playlistTrack) {
        return playlistTrack == track;
    });
    if(it != m_tracks.cend()) {
        return static_cast<int>(std::distance(m_tracks.cbegin(), it));
    }
    return -1;
}

int Playlist::trackCount() const
{
    return static_cast<int>(m_tracks.size());
}
} // namespace Fy::Core::Playlist
