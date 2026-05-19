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

QStringList splitSlashSeparatedValues(const QStringList& values)
{
    QStringList result;
    for(const QString& value : values) {
        const QStringList parts = value.split(" / "_L1, Qt::SkipEmptyParts);
        for(const QString& part : parts) {
            result.append(part.trimmed());
        }
    }
    return result;
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

bool isLyricsField(const QString& field)
{
    return field == "LYRICS"_L1 || field == "SYNCEDLYRICS"_L1 || field == "SYNCED LYRICS"_L1
        || field == "UNSYNCEDLYRICS"_L1 || field == "UNSYNCED LYRICS"_L1 || field == "UNSYNCHRONIZEDLYRICS"_L1
        || field == "UNSYNCHRONIZED LYRICS"_L1 || field == "USLT"_L1 || field == "SYLT"_L1;
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

    if(isLyricsField(upperField)) {
        return values;
    }

    return splitSemicolonSeparated ? splitValues(values, u';') : values;
}
} // namespace Fooyin::Id3Utils
