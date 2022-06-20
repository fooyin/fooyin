/*
 * Fooyin
 * Copyright 2022, Luke Taylor <LukeT1@proton.me>
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
#include "models/track.h"
#include "utils/utils.h"

#include <QMessageBox>

Playlist::Playlist::Playlist(int idx, QString name, PlayerManager* playerManager)
    : m_name(std::move(name))
    , m_playerManager(playerManager)
    , m_playlistIndex(idx)
    , m_playingTrack(nullptr)
{ }

QString Playlist::Playlist::name()
{
    return m_name;
}

Playlist::Playlist::~Playlist() = default;

int Playlist::Playlist::createPlaylist(const TrackPtrList& tracks)
{
    m_tracks << tracks;
    return static_cast<int>(m_tracks.count());
}

int Playlist::Playlist::currentTrackIndex() const
{
    auto it = std::find_if(m_tracks.constBegin(), m_tracks.constEnd(), [&](Track* track) {
        return (track->id() == m_playingTrack->id());
    });

    if(it == m_tracks.end())
    {
        return -1;
    }

    return static_cast<int>(std::distance(m_tracks.constBegin(), it));
}

Track* Playlist::Playlist::currentTrack() const
{
    const auto trackIndex = currentTrackIndex();
    if(trackIndex >= m_tracks.size() || trackIndex < 0)
    {
        return {};
    }

    return m_tracks.at(trackIndex);
}

int Playlist::Playlist::index() const
{
    return m_playlistIndex;
}

void Playlist::Playlist::insertTracks(const TrackPtrList& tracks)
{
    m_tracks = tracks;
}

void Playlist::Playlist::appendTracks(const TrackPtrList& tracks)
{
    m_tracks.append(tracks);
}

void Playlist::Playlist::clear()
{
    m_tracks.clear();
}

void Playlist::Playlist::setCurrentTrack(int index)
{
    if(index < 0 || index >= m_tracks.size())
    {
        stop();
    }

    else
    {
        Track* track = m_tracks[index];
        m_playingTrack = track;
        m_playerManager->changeCurrentTrack(track);
    }
}

bool Playlist::Playlist::changeTrack(int index)
{
    setCurrentTrack(index);

    if(index < 0 || index >= m_tracks.size())
    {
        stop();
        return false;
    }

    while(!Util::File::exists(m_tracks[index]->filepath()))
    {
        QMessageBox message;
        message.setText(QString("Track %1 cannot be found.").arg(index));
        message.setInformativeText(m_tracks[index]->filepath());
        message.exec();

        setCurrentTrack(++index);
    }

    m_playerManager->play();

    return true;
}

void Playlist::Playlist::play()
{
    if(currentTrackIndex() < 0)
    {
        next();
    }
}

void Playlist::Playlist::stop()
{
    m_playingTrack = nullptr;
}

void Playlist::Playlist::next()
{
    if(m_tracks.isEmpty())
    {
        stop();
        return;
    }

    int index = nextIndex();

    changeTrack(index);
}

void Playlist::Playlist::previous()
{
    if(m_playerManager->currentPosition() > 5000)
    {
        m_playerManager->changePosition(0);
        return;
    }

    changeTrack(currentTrackIndex() - 1);
}

int Playlist::Playlist::nextIndex()
{
    const auto mode = m_playerManager->playMode();
    const auto isLastTrack = (currentTrackIndex() == m_tracks.count() - 1);
    int index;

    if((currentTrackIndex() == -1) && mode != Player::PlayMode::Shuffle)
    {
        index = 0;
    }

    else if(mode == Player::PlayMode::Repeat)
    {
        index = currentTrackIndex();
    }
    // TODO: implement full shuffle functionality
    else if(mode == Player::PlayMode::Shuffle)
    {
        index = Util::randomNumber(0, static_cast<int>(m_tracks.size()) - 1);
    }

    else if(isLastTrack)
    {
        index = mode == Player::PlayMode::RepeatAll ? 0 : -1;
    }

    else
    {
        index = currentTrackIndex() + 1;
    }

    return index;
}
