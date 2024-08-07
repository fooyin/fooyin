﻿/*
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

#pragma once

#include "fyutils_export.h"

#include <QMetaType>
#include <QString>
#include <QUuid>

#include <set>

namespace Fooyin {
class FYUTILS_EXPORT UId : public QUuid
{
public:
    struct UIdHash
    {
        std::size_t operator()(const UId& id) const
        {
            return static_cast<std::size_t>(qHash(id));
        }
    };

    UId() = default;
    explicit UId(const QUuid& id);

    static UId create();

    [[nodiscard]] bool isValid() const;
};

class FYUTILS_EXPORT Id
{
public:
    struct IdHash
    {
        size_t operator()(const Id& id) const
        {
            return (std::hash<unsigned int>{}(id.m_id)) ^ (std::hash<QString>{}(id.m_name) << 1);
        }
    };

    Id() = default;
    explicit Id(const QString& name);
    Id(const char* name);
    ~Id() = default;

    std::strong_ordering operator<=>(const Id& id) const = default;

    [[nodiscard]] bool isValid() const;

    [[nodiscard]] uint32_t id() const;
    [[nodiscard]] QString name() const;

    Id append(const Id& id);
    Id append(const QString& str);
    Id append(const char* str);
    Id append(int num);
    Id append(uintptr_t addr);

private:
    friend FYUTILS_EXPORT QDataStream& operator<<(QDataStream& stream, const Id& id);
    friend FYUTILS_EXPORT QDataStream& operator>>(QDataStream& stream, Id& id);

    uint32_t m_id;
    QString m_name;
};
FYUTILS_EXPORT size_t qHash(const Id& id);

using IdList = std::vector<Id>;
using IdSet  = std::set<Id>;

FYUTILS_EXPORT QDataStream& operator<<(QDataStream& stream, const IdSet& ids);
FYUTILS_EXPORT QDataStream& operator>>(QDataStream& stream, IdSet& ids);
} // namespace Fooyin
