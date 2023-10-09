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

#include <core/playlist/playlist.h>

namespace Fy::Core::Playlist {
int findTrack(const Track& trackTofind, const TrackList& tracks)
{
    auto it = std::ranges::find(std::as_const(tracks), trackTofind);
    if(it != tracks.end()) {
        return static_cast<int>(std::distance(tracks.cbegin(), it));
    }
    return -1;
}

Playlist::Playlist()
    : Playlist{{}, -1, -1}
{ }

Playlist::Playlist(QString name, int index, int id)
    : m_id{id}
    , m_index{index}
    , m_name{std::move(name)}
    , m_currentTrackIndex{-1}
    , m_modified{false}
{ }

bool Playlist::isValid() const
{
    return m_id >= 0 && m_index >= 0 && !m_name.isEmpty();
}

int Playlist::id() const
{
    return m_id;
}

int Playlist::index() const
{
    return m_index;
}

QString Playlist::name() const
{
    return m_name;
}

TrackList Playlist::tracks() const
{
    return m_tracks;
}

int Playlist::trackCount() const
{
    return static_cast<int>(m_tracks.size());
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

void Playlist::setIndex(int index)
{
    m_index = index;
}

void Playlist::setName(const QString& name)
{
    m_name = name;
}

void Playlist::setModified(bool modified)
{
    m_modified = modified;
}

void Playlist::replaceTracks(const TrackList& tracks)
{
    m_tracks   = tracks;
    m_modified = true;
}

void Playlist::appendTracks(const TrackList& tracks)
{
    std::ranges::copy(tracks, std::back_inserter(m_tracks));
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
    const int index = findTrack(track, m_tracks);
    if(index >= 0) {
        m_currentTrackIndex = index;
    }
}
} // namespace Fy::Core::Playlist
