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

#include "core/player/playermanager.h"

#include <utils/utils.h>

#include <QMessageBox>

namespace Core::Playlist {
Playlist::Playlist(Player::PlayerManager* playerManager, int idx, QString name)
    : m_playerManager{playerManager}
    , m_name{std::move(name)}
    , m_playlistIndex{idx}
    , m_playingTrack{nullptr}
{ }

QString Playlist::name()
{
    return m_name;
}

int Playlist::createPlaylist(const TrackPtrList& tracks)
{
    m_tracks.insert(m_tracks.end(), tracks.begin(), tracks.end());
    return static_cast<int>(m_tracks.size());
}

int Playlist::currentTrackIndex() const
{
    if(!m_playingTrack) {
        return -1;
    }

    auto it = std::find_if(m_tracks.cbegin(), m_tracks.cend(), [this](Track* track) {
        return (track->id() == m_playingTrack->id());
    });

    if(it == m_tracks.end()) {
        return -1;
    }

    return static_cast<int>(std::distance(m_tracks.cbegin(), it));
}

Track* Playlist::currentTrack() const
{
    const auto trackIndex = currentTrackIndex();
    if(trackIndex >= numberOfTracks() || trackIndex < 0) {
        return {};
    }

    return m_tracks.at(trackIndex);
}

int Playlist::index() const
{
    return m_playlistIndex;
}

void Playlist::insertTracks(const TrackPtrList& tracks)
{
    m_tracks = tracks;
}

void Playlist::appendTracks(const TrackPtrList& tracks)
{
    m_tracks.insert(m_tracks.end(), tracks.begin(), tracks.end());
}

void Playlist::clear()
{
    m_tracks.clear();
}

void Playlist::setCurrentTrack(int index)
{
    if(index < 0 || index >= numberOfTracks()) {
        stop();
    }

    else {
        Track* track   = m_tracks[index];
        m_playingTrack = track;
        m_playerManager->changeCurrentTrack(track);
    }
}

bool Playlist::changeTrack(int index)
{
    setCurrentTrack(index);

    if(index < 0 || index >= numberOfTracks()) {
        stop();
        return false;
    }

    while(!Utils::File::exists(m_tracks[index]->filepath())) {
        QMessageBox message;
        message.setText(QString("Track %1 cannot be found.").arg(index));
        message.setInformativeText(m_tracks[index]->filepath());
        message.exec();

        setCurrentTrack(++index);
    }

    m_playerManager->play();

    return true;
}

void Playlist::play()
{
    if(currentTrackIndex() < 0) {
        next();
    }
}

void Playlist::stop()
{
    m_playingTrack = nullptr;
}

int Playlist::next()
{
    if(m_tracks.empty()) {
        stop();
        return -1;
    }
    const int index = nextIndex();
    changeTrack(index);
    return index;
}

int Playlist::previous()
{
    int index = currentTrackIndex();
    if(m_playerManager->currentPosition() > 5000) {
        m_playerManager->changePosition(0);
        return index;
    }
    --index;
    changeTrack(index);
    return index;
}

int Playlist::nextIndex()
{
    const int currentIndex = currentTrackIndex();
    const bool isLastTrack = (currentIndex >= numberOfTracks() - 1);
    const auto mode        = m_playerManager->playMode();
    int index              = currentIndex + 1;

    if(mode == Player::PlayMode::Repeat) {
        index = currentIndex;
    }
    // TODO: Implement full shuffle functionality
    else if(mode == Player::PlayMode::Shuffle) {
        index = Utils::randomNumber(0, static_cast<int>(m_tracks.size()) - 1);
    }

    else if(isLastTrack) {
        index = mode == Player::PlayMode::RepeatAll ? 0 : -1;
    }

    return index;
}

int Playlist::numberOfTracks() const
{
    return static_cast<int>(m_tracks.size());
}
} // namespace Core::Playlist
