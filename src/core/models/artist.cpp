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

#include "artist.h"

namespace Fy::Core {
Artist::Artist(QString name)
    : MusicItem()
    , m_id{-1}
    , m_name{std::move(name)}
{ }

int Artist::id() const
{
    return m_id;
}

QString Artist::name() const
{
    return m_name;
}

void Artist::setId(int id)
{
    m_id = id;
}

} // namespace Fy::Core
