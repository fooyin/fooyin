/*
 * Fooyin
 * Copyright © 2025, Luke Taylor <LukeT1@proton.me>
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

#include <QString>

namespace Fooyin {
class StringCollatorPrivate;

class FYUTILS_EXPORT StringCollator
{
public:
    StringCollator();
    ~StringCollator();

    [[nodiscard]] Qt::CaseSensitivity caseSensitivity() const;
    void setCaseSensitivity(Qt::CaseSensitivity sensitivity);

    [[nodiscard]] bool numericMode() const;
    void setNumericMode(bool enabled);

    [[nodiscard]] int compare(QStringView s1, QStringView s2) const;

private:
    std::unique_ptr<StringCollatorPrivate> p;
};
} // namespace Fooyin
