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

#pragma once

#include <QList>
#include <QMetaType>
#include <QString>

namespace Utils {
class Id
{
public:
    struct IdHash
    {
        size_t operator()(const Id& id) const
        {
            size_t rowHash = std::hash<unsigned int>()(id.m_id);
            size_t colHash = std::hash<QString>()(id.m_name) << 1;
            return rowHash ^ colHash;
        }
    };

    Id() = default;
    explicit Id(const QString& name);
    Id(const char* name);
    ~Id() = default;

    [[nodiscard]] bool isValid() const;

    [[nodiscard]] unsigned int id() const;
    [[nodiscard]] QString name() const;

    Id append(const Id& id);
    Id append(const QString& str);
    Id append(const char* str);
    Id append(int num);
    Id append(quintptr addr);

    bool operator==(const Id& id) const;
    bool operator!=(const Id& id) const;

    friend size_t qHash(const Id& id) noexcept;

private:
    unsigned int m_id;
    QString m_name;
};
} // namespace Utils

Q_DECLARE_METATYPE(Utils::Id)
Q_DECLARE_METATYPE(QList<Utils::Id>)
