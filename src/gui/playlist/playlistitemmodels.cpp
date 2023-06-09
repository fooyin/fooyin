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

#include "playlistitemmodels.h"

#include <utils/utils.h>

namespace Fy::Gui::Widgets::Playlist {
Container::Container()
    : m_duration{0}
{ }

Core::TrackList Container::tracks() const
{
    return m_tracks;
}

int Container::trackCount() const
{
    return static_cast<int>(m_tracks.size());
}

uint64_t Container::duration() const
{
    return m_duration;
}

void Container::addTrack(const Core::Track& track)
{
    m_tracks.emplace_back(track);
    m_duration += track.duration();
}

void Container::removeTrack(const Core::Track& trackToRemove)
{
    m_tracks.erase(std::remove_if(m_tracks.begin(), m_tracks.end(),
                                  [trackToRemove](const Core::Track& track) {
                                      return track == trackToRemove;
                                  }),
                   m_tracks.end());

    m_duration -= trackToRemove.duration();
}

Header::Header(QString title, QString subtitle, QString rightText, QString coverPath)
    : m_title{std::move(title)}
    , m_subtitle{std::move(subtitle)}
    , m_rightText{std::move(rightText)}
    , m_coverPath{std::move(coverPath)}
{ }

QString Header::title() const
{
    return m_title;
}

QString Header::subtitle() const
{
    return m_subtitle;
}

QString Header::rightText() const
{
    return m_rightText;
}

QString Header::info() const
{
    const auto genres = m_genres.join(" / ");
    const auto count  = trackCount();

    QString dur = genres;
    if(!genres.isEmpty()) {
        dur += " | ";
    }
    dur += QString(QString::number(count) + (count > 1 ? " Tracks" : " Track") + " | " + Utils::msToString(duration()));
    return dur;
}

bool Header::hasCover() const
{
    return !m_coverPath.isEmpty();
}

QString Header::coverPath() const
{
    return m_coverPath;
}

void Header::addTrack(const Core::Track& track)
{
    Container::addTrack(track);

    for(const auto& genre : track.genres()) {
        if(!m_genres.contains(genre)) {
            m_genres.emplace_back(genre);
        }
    }
}

void Header::removeTrack(const Core::Track& trackToRemove)
{
    Container::removeTrack(trackToRemove);

    m_genres.clear();

    const auto containerTracks = tracks();
    for(const Core::Track& track : containerTracks) {
        const auto genres = track.genres();
        for(const QString& genre : genres) {
            if(!m_genres.contains(genre)) {
                m_genres.emplace_back(genre);
            }
        }
    }
}

Subheader::Subheader(const QString& title)
    : m_title{title}
{ }

QString Subheader::title() const
{
    return m_title;
}

Track::Track(const Core::Track& track, QStringList leftSide, QStringList rightSide)
    : m_track{track}
    , m_leftSide{std::move(leftSide)}
    , m_rightSide{std::move(rightSide)}
{ }

Core::Track Track::track() const
{
    return m_track;
}

QStringList Track::leftSide() const
{
    return m_leftSide;
}

QStringList Track::rightSide() const
{
    return m_rightSide;
}
} // namespace Fy::Gui::Widgets::Playlist
