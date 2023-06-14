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

namespace Fy::Core {
Album::Album()
    : Album("")
{ }

Album::Album(QString title)
    : m_title{std::move(title)}
    , m_duration{0}
    , m_trackCount{0}
    , m_discCount{0}
{ }

QString Album::title() const
{
    return m_title;
}

QString Album::subTitle() const
{
    return m_subTitle;
}

QString Album::artist() const
{
    return m_artist;
}

QString Album::date() const
{
    return m_date;
}

QStringList Album::genres() const
{
    return m_genres;
}

bool Album::hasCover() const
{
    return !m_coverPath.isEmpty();
}

QString Album::coverPath() const
{
    return m_coverPath;
}

bool Album::isSingleDiscAlbum() const
{
    return m_discCount <= 1;
}

int Album::trackCount() const
{
    return m_trackCount;
}

uint64_t Album::duration() const
{
    return m_duration;
}

void Album::setTitle(const QString& title)
{
    m_title = title;
}

void Album::setSubTitle(const QString& title)
{
    m_subTitle = title;
}

void Album::setArtist(const QString& artist)
{
    m_artist = artist;
}

void Album::setDate(const QString& date)
{
    m_date = date;
}

void Album::setGenres(const QStringList& genres)
{
    m_genres = genres;
}

void Album::setCoverPath(const QString& path)
{
    m_coverPath = path;
}

void Album::addTrack(const Track& track)
{
    ++m_trackCount;
    m_duration += track.duration();

    for(const auto& genre : track.genres()) {
        if(!m_genres.contains(genre)) {
            m_genres.emplace_back(genre);
        }
    }

    m_discCount = track.discTotal();
}

void Album::removeTrack(const Track& track)
{
    --m_trackCount;
    m_duration -= track.duration();
}

void Album::reset()
{
    m_trackCount = 0;
    m_duration   = 0;
    m_genres.clear();
}
} // namespace Fy::Core
