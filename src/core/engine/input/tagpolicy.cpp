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

#include "tagpolicy.h"

#include "internalcoresettings.h"

#include <core/coresettings.h>

namespace Fooyin {
namespace {
template <typename Enum>
Enum enumSetting(const FySettings& settings, auto key, Enum defaultValue, Enum min, Enum max)
{
    const int value = settings.value(key, static_cast<int>(defaultValue)).toInt();

    if(value < static_cast<int>(min) || value > static_cast<int>(max)) {
        return defaultValue;
    }

    return static_cast<Enum>(value);
}
} // namespace

TagPolicy tagPolicy()
{
    const FySettings settings;

    const auto id3v2Version = enumSetting(settings, Settings::Core::Internal::Id3v2WriteVersion, Id3v2WriteVersion::V4,
                                          Id3v2WriteVersion::V3, Id3v2WriteVersion::V4);

    const auto mp3TagWritingScheme
        = enumSetting(settings, Settings::Core::Internal::Mp3TagWritingScheme, Mp3TagWritingScheme::Id3v2AndId3v1,
                      Mp3TagWritingScheme::Id3v2AndId3v1, Mp3TagWritingScheme::Ape);

    return {
        .rating    = ratingTagPolicy(),
        .playcount = playcountTagPolicy(),
        .splitId3v23SemicolonSeparatedTags
        = settings.value(Settings::Core::Internal::SplitId3v23SemicolonSeparatedTags, true).toBool(),
        .id3v2WriteVersion   = id3v2Version,
        .mp3TagWritingScheme = mp3TagWritingScheme,
    };
}
} // namespace Fooyin
