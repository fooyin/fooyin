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

#include "album.h"

#include "track.h"

#include <utils/helpers.h>

namespace Core {
Album::Album(const QString& title)
    : Container{title}
    , m_id{-1}
    , m_artistId{-1}
    , m_discCount{0}
{ }

int Album::id() const
{
    return m_id;
}

int Album::artistId() const
{
    return m_artistId;
}

QString Album::artist() const
{
    return m_artist;
}

QString Album::date() const
{
    return m_date;
}

int Album::year() const
{
    return m_year;
}

GenreList Album::genres() const
{
    return m_genres;
}

int Album::discCount() const
{
    return m_discCount;
}

bool Album::isSingleDiscAlbum() const
{
    return m_discCount <= 1;
}

bool Album::hasCover() const
{
    return !m_coverPath.isEmpty();
}

QString Album::coverPath() const
{
    return m_coverPath;
}

void Album::setId(int id)
{
    m_id = id;
}

void Album::setArtistId(int id)
{
    m_artistId = id;
}

void Album::setArtist(const QString& artist)
{
    m_artist = artist;
}

void Album::setDate(const QString& date)
{
    m_date = date;
    m_year = date.toInt();
}

void Album::setGenres(const GenreList& genres)
{
    m_genres = genres;
}

void Album::setDiscCount(int count)
{
    m_discCount = count;
}

void Album::setCoverPath(const QString& path)
{
    m_coverPath = path;
}

void Album::addTrack(Track* track)
{
    Container::addTrack(track);

    const int trackDisc = track->discNumber();
    if(trackDisc > m_discCount) {
        m_discCount = trackDisc;
    }

    for(const auto& genre : track->genres()) {
        if(!Utils::contains(m_genres, genre)) {
            m_genres.emplace_back(genre);
        }
    }
}

void Album::removeTrack(Track* track)
{
    Container::removeTrack(track);
}

void Album::reset()
{
    Container::reset();
    m_discCount = 0;
}
} // namespace Core
