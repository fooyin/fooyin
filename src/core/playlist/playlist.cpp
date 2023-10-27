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

#include <core/player/playermanager.h>
#include <utils/utils.h>

namespace {
int findTrack(const Fy::Core::Track& trackTofind, const Fy::Core::TrackList& tracks)
{
    auto it = std::ranges::find(std::as_const(tracks), trackTofind);
    if(it != tracks.end()) {
        return static_cast<int>(std::distance(tracks.cbegin(), it));
    }
    return -1;
}
} // namespace

namespace Fy::Core::Playlist {
Playlist::Playlist()
    : Playlist{{}, -1, -1}
{ }

Playlist::Playlist(QString name, int index, int id)
    : m_id{id}
    , m_index{index}
    , m_name{std::move(name)}
    , m_currentTrackIndex{-1}
    , m_nextTrackIndex{-1}
    , m_modified{false}
    , m_tracksModified{false}
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
    if(m_nextTrackIndex >= 0 && m_nextTrackIndex < trackCount()) {
        return m_tracks.at(m_nextTrackIndex);
    }
    if(m_currentTrackIndex >= 0 && m_currentTrackIndex < trackCount()) {
        return m_tracks.at(m_currentTrackIndex);
    }
    return {};
}

bool Playlist::modified() const
{
    return m_modified;
}

bool Playlist::tracksModified() const
{
    return m_tracksModified;
}

void Playlist::scheduleNextIndex(int index)
{
    if(index >= 0 && index < trackCount()) {
        m_nextTrackIndex = index;
    }
}

Track Playlist::nextTrack(Player::PlayMode mode, int delta)
{
    int index = m_currentTrackIndex;

    if(m_nextTrackIndex >= 0) {
        index            = m_nextTrackIndex;
        m_nextTrackIndex = -1;
    }
    else {
        const int count = trackCount();

        switch(mode) {
            case(Player::PlayMode::Shuffle): {
                // TODO: Implement full shuffle functionality
                index = Utils::randomNumber(0, static_cast<int>(count - 1));
                break;
            }
            case(Player::PlayMode::RepeatAll): {
                index += delta;
                if(index < 0) {
                    index = count - 1;
                }
                else if(index >= count) {
                    index = 0;
                }
                break;
            }
            case(Player::PlayMode::Default): {
                index += delta;
                if(index < 0 || index >= count) {
                    index = -1;
                }
            }
            case(Player::PlayMode::Repeat):
                break;
        }

        if(index < 0) {
            return {};
        }
    }

    changeCurrentTrack(index);

    return currentTrack();
}

void Playlist::setIndex(int index)
{
    if(std::exchange(m_index, index) != index) {
        m_modified = true;
    }
}

void Playlist::setName(const QString& name)
{
    if(std::exchange(m_name, name) != name) {
        m_modified = true;
    }
}

void Playlist::replaceTracks(const TrackList& tracks)
{
    if(std::exchange(m_tracks, tracks) != tracks) {
        m_tracksModified = true;
    }
}

void Playlist::replaceTracksSilently(const TrackList& tracks)
{
    m_tracks = tracks;
}

void Playlist::appendTracks(const TrackList& tracks)
{
    if(tracks.empty()) {
        return;
    }

    std::ranges::copy(tracks, std::back_inserter(m_tracks));
    m_modified = true;
}

void Playlist::appendTracksSilently(const TrackList& tracks)
{
    std::ranges::copy(tracks, std::back_inserter(m_tracks));
}

void Playlist::clear()
{
    if(!m_tracks.empty()) {
        m_tracks.clear();
        m_tracksModified = true;
    }
}

void Playlist::resetFlags()
{
    m_modified       = false;
    m_tracksModified = false;
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
