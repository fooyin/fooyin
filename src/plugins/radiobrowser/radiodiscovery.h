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

#include <QMetaType>
#include <QString>

#include <vector>

namespace Fooyin::RadioBrowser {
enum class RadioCategoryType : uint8_t
{
    Country = 0,
    Language,
    Tag,
    Codec,
};

struct RadioCategory
{
    RadioCategoryType type{RadioCategoryType::Country};
    QString name;
    QString value;
    int stationCount{0};
};
using RadioCategoryList = std::vector<RadioCategory>;

[[nodiscard]] QString displayNameForCountry(const QString& countryName, const QString& countryCode = {});
} // namespace Fooyin::RadioBrowser

Q_DECLARE_METATYPE(Fooyin::RadioBrowser::RadioCategory)
Q_DECLARE_METATYPE(Fooyin::RadioBrowser::RadioCategoryList)
