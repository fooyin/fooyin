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

#include "id3utils.h"

#include <core/engine/tagdefs.h>

using namespace Qt::StringLiterals;

namespace Fooyin::Id3Utils {
namespace {
QStringList splitValues(const QStringList& values, QChar separator)
{
    QStringList result;
    for(const QString& value : values) {
        const QStringList parts = value.split(separator, Qt::SkipEmptyParts);
        for(const QString& part : parts) {
            result.append(part.trimmed());
        }
    }
    return result;
}

bool containsSlashSeparatedValue(const QStringList& values)
{
    return std::ranges::any_of(values, [](const QString& value) {
        return value.contains('/'_L1) && !value.contains("AC/DC"_L1) && !value.contains("AC / DC"_L1);
    });
}

QStringList splitSlashSeparatedValues(const QStringList& values)
{
    if(containsSlashSeparatedValue(values)) {
        return splitValues(values, u'/');
    }
    return values;
}

bool isSlashSeparatedField(const QString& field)
{
    return field == QLatin1StringView{Tag::Artist} || field == QLatin1StringView{Tag::Composer}
        || field == QLatin1StringView{Tag::Performer};
}

bool isSlashSeparatedExtraField(const QString& field)
{
    return field == "LYRICIST"_L1 || field == "ORIGINALLYRICIST"_L1 || field == "ORIGINALARTIST"_L1
        || field == "TEXT"_L1 || field == "TOLY"_L1 || field == "TOPE"_L1;
}
} // namespace

QStringList splitStandardField(const QString& field, const QStringList& values, bool splitSemicolonSeparated)
{
    const QString upperField = field.toUpper();

    if(isSlashSeparatedField(upperField)) {
        return splitSlashSeparatedValues(values);
    }

    if(splitSemicolonSeparated && upperField == QLatin1StringView{Tag::Genre}) {
        return splitValues(values, u';');
    }

    return values;
}

QStringList splitExtraField(const QString& field, const QStringList& values, bool splitSemicolonSeparated)
{
    const QString upperField = field.toUpper();

    if(isSlashSeparatedExtraField(upperField)) {
        return splitSlashSeparatedValues(values);
    }

    return splitSemicolonSeparated ? splitValues(values, u';') : values;
}
} // namespace Fooyin::Id3Utils
