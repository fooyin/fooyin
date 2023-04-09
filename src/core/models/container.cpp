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

#include "container.h"

#include "track.h"

namespace Fy::Core {
Container::Container(QString title)
    : m_title{std::move(title)}
    , m_duration{0}
    , m_trackCount{0}
{ }

QString Container::title() const
{
    return m_title;
}

QString Container::subTitle() const
{
    return m_subTitle;
}

int Container::trackCount() const
{
    return m_trackCount;
}

uint64_t Container::duration() const
{
    return m_duration;
}

void Container::setSubTitle(const QString& title)
{
    m_subTitle = title;
}

void Container::setTrackCount(int count)
{
    m_trackCount = count;
}

void Container::setDuration(uint64_t duration)
{
    m_duration = duration;
}

void Container::addTrack(const Track& track)
{
    ++m_trackCount;
    m_duration += track.duration();
}

void Container::removeTrack(const Track& track)
{
    --m_trackCount;
    m_duration -= track.duration();
}
} // namespace Fy::Core
