/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <LukeT1@proton.me>
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

#include <utils/settings/settingsentry.h>

#include <QLoggingCategory>
#include <QObject>

namespace Fooyin {
class SettingsManager;

namespace Settings::Lyrics {
Q_NAMESPACE
enum LyricsSettings : uint32_t
{
    Paths          = 1 | Type::StringList,
    Colours        = 2 | Type::Variant,
    LineFont       = 3 | Type::String,
    WordLineFont   = 4 | Type::String,
    WordFont       = 5 | Type::String,
    Alignment      = 6 | Type::Int,
    ScrollDuration = 7 | Type::Int,
    SearchTags     = 8 | Type::StringList,
    LineSpacing    = 9 | Type::Int,
    Margins        = 10 | Type::Variant,
    NoLyricsScript = 11 | Type::String,
    SeekOnClick    = 12 | Type::Bool,
    ShowScrollbar  = 13 | Type::Bool,
    ScrollMode     = 14 | Type::Int,
};
Q_ENUM_NS(LyricsSettings)
} // namespace Settings::Lyrics

namespace Lyrics {
enum class ScrollMode : uint8_t
{
    Manual = 0,
    Synced,
    Automatic
};

class LyricsSettings
{
public:
    explicit LyricsSettings(SettingsManager* settingsManager);

private:
    SettingsManager* m_settings;
};
} // namespace Lyrics
} // namespace Fooyin
