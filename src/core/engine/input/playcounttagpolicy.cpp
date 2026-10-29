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

#include "playcounttagpolicy.h"

#include "ratingtagpolicy.h"

#include <core/coresettings.h>

using namespace Qt::StringLiterals;

namespace Fooyin {
bool PlaycountTagPolicy::automaticRead() const
{
    return readTag.isEmpty() || readTag == "AUTOMATIC"_L1;
}

bool PlaycountTagPolicy::shouldReadTag(const QString& tag, bool currentPlaycountSet) const
{
    if(automaticRead()) {
        return tag == "FMPS_PLAYCOUNT"_L1 || (tag == "PLAYCOUNT"_L1 && !currentPlaycountSet);
    }
    return tag == readTag;
}

QString PlaycountTagPolicy::effectiveWriteTag() const
{
    return writeTag == "DONOTWRITE"_L1 ? QString{} : writeTag;
}

PlaycountTagPolicy playcountTagPolicy()
{
    const FySettings settings;
    const bool readId3Popm
        = settings.contains(PlaycountSettings::ReadId3Popm)
            ? settings.value(PlaycountSettings::ReadId3Popm, PlaycountSettings::DefaultReadId3Popm).toBool()
            : settings.value(RatingSettings::ReadId3Popm, RatingSettings::DefaultReadId3Popm).toBool();
    const bool writeId3Popm
        = settings.value(PlaycountSettings::WriteId3Popm, PlaycountSettings::DefaultWriteId3Popm).toBool();

    return {
        .readTag  = settings.value(PlaycountSettings::ReadTag, QLatin1StringView{PlaycountSettings::DefaultAutomatic})
                        .toString()
                        .trimmed()
                        .toUpper(),
        .writeTag = settings.value(PlaycountSettings::WriteTag, QLatin1StringView{PlaycountSettings::DefaultFmpsTag})
                        .toString()
                        .trimmed()
                        .toUpper(),
        .readId3Popm  = readId3Popm,
        .writeId3Popm = writeId3Popm,
    };
}
} // namespace Fooyin
