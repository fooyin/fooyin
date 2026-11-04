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

#include <core/track.h>

namespace Fooyin::TrackPlayback {
constexpr auto ExclusionProperty          = "_PLAYBACK_EXCLUSION";
constexpr auto DisableCrossfadingProperty = "_PLAYBACK_DISABLE_CROSSFADING";

inline bool isExcluded(const Track& track, bool shuffling)
{
    if(!track.hasExtraProperty(QString::fromLatin1(ExclusionProperty))) {
        return false;
    }

    const QString value = track.extraProperties().value(QString::fromLatin1(ExclusionProperty));
    return value == QLatin1StringView{"always"} || (shuffling && value == QLatin1StringView{"shuffle"});
}

inline bool crossfadingDisabled(const Track& track)
{
    return track.hasExtraProperty(QString::fromLatin1(DisableCrossfadingProperty))
        && track.extraProperties().value(QString::fromLatin1(DisableCrossfadingProperty)) == QLatin1StringView{"1"};
}
} // namespace Fooyin::TrackPlayback
