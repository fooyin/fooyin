/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
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

namespace {
uint32_t idFromString(const QString& str)
{
    uint32_t result{0};
    if(!str.isEmpty()) {
        result = std::hash<QString>{}(str);
    }
    return result;
}
} // namespace

namespace Fooyin {
UId::UId(const QUuid& id)
    : QUuid{id}
{ }

UId UId::create()
{
    return UId{QUuid::createUuid()};
}

bool UId::isValid() const
{
    return !isNull();
}

Id::Id(const QString& str)
    : m_id{idFromString(str)}
    , m_name{str}
{ }

Id::Id(const char* const str)
    : m_id{idFromString(QString::fromLatin1(str))}
    , m_name{QString::fromLatin1(str)}
{ }

bool Id::isValid() const
{
    return (m_id > 0 && !m_name.isNull());
}

uint32_t Id::id() const
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
    return append(QString::fromLatin1(str));
}

Id Id::append(int num)
{
    return Id{m_name.append(QString::number(num))};
}

Id Id::append(uintptr_t addr)
{
    return Id{m_name.append(QString::number(addr))};
}

QDataStream& operator<<(QDataStream& stream, const Id& id)
{
    stream << id.name();

    return stream;
}

QDataStream& operator>>(QDataStream& stream, Id& id)
{
    QString name;

    stream >> name;
    id = Id{name};

    return stream;
}

size_t qHash(const Id& id)
{
    return Id::IdHash{}(id);
}

QDataStream& operator<<(QDataStream& stream, const IdSet& ids)
{
    stream << static_cast<int>(ids.size());

    for(const auto& id : ids) {
        stream << id;
    }

    return stream;
}

QDataStream& operator>>(QDataStream& stream, IdSet& ids)
{
    int size;
    stream >> size;

    for(int i{0}; i < size; ++i) {
        Id id;
        stream >> id;

        ids.emplace(id);
    }

    return stream;
}
} // namespace Fooyin
