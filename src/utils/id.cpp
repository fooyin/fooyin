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

#include <utils/id.h>

namespace Fy::Utils {
unsigned int idFromString(const QString& str)
{
    uint32_t result{0};
    if(!str.isEmpty()) {
        result = std::hash<QString>{}(str);
    }
    return result;
}

Id::Id(const QString& str)
    : m_id{idFromString(str)}
    , m_name{str}
{ }

Id::Id(const char* const str)
    : m_id{idFromString(str)}
    , m_name{str}
{ }

bool Id::isValid() const
{
    return (m_id > 0 && !m_name.isNull());
}

unsigned int Id::id() const
{
    return m_id;
}

QString Id::name() const
{
    return m_name;
}

Id Id::append(const Id& id)
{
    return Id{m_name.append(id.name())};
}

Id Id::append(const QString& str)
{
    return Id{m_name.append(str)};
}

Id Id::append(const char* const str)
{
    return append(QString{str});
}

Id Id::append(int num)
{
    return Id{m_name.append(QString::number(num))};
}

Id Id::append(uintptr_t addr)
{
    return Id{m_name.append(QString::number(addr))};
}
} // namespace Fy::Utils
