/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
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

#include "fycore_export.h"

#include <QDataStream>
#include <QString>

#include <vector>

namespace Fooyin {
struct FYCORE_EXPORT LibraryFilter
{
    int id{-1};
    int index{-1};
    bool isDefault{false};
    QString name;
    QString expression;
    bool enabled{true};

    bool operator==(const LibraryFilter& other) const = default;

    [[nodiscard]] bool isValid() const;
};

FYCORE_EXPORT QDataStream& operator<<(QDataStream& stream, const LibraryFilter& preset);
FYCORE_EXPORT QDataStream& operator>>(QDataStream& stream, LibraryFilter& preset);

using LibraryFilterList = std::vector<LibraryFilter>;
} // namespace Fooyin
