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

#include <QLoggingCategory>
#include <QMargins>
#include <QStringList>

namespace Fooyin {
class SettingsManager;

namespace Lyrics {
namespace Settings {
constexpr auto Paths           = u"Lyrics/Paths";
constexpr auto Colours         = u"Lyrics/Colours";
constexpr auto LineFont        = u"Lyrics/CurrentLineFont";
constexpr auto WordLineFont    = u"Lyrics/CurrentWordLineFont";
constexpr auto WordFont        = u"Lyrics/CurrentWordFont";
constexpr auto Alignment       = u"Lyrics/Alignment";
constexpr auto ScrollDuration  = u"Lyrics/ScrollDuration";
constexpr auto SearchTags      = u"Lyrics/SearchTags";
constexpr auto LineSpacing     = u"Lyrics/LineSpacing";
constexpr auto Margins         = u"Lyrics/Margins";
constexpr auto NoLyricsScript  = u"Lyrics/NoLyricsScript";
constexpr auto SeekOnClick     = u"Lyrics/SeekOnClick";
constexpr auto ShowScrollbar   = u"Lyrics/ShowScrollbar";
constexpr auto ScrollMode      = u"Lyrics/ScrollMode";
constexpr auto TitleField      = u"Lyrics/TitleField";
constexpr auto AlbumField      = u"Lyrics/AlbumField";
constexpr auto ArtistField     = u"Lyrics/ArtistField";
constexpr auto SaveScheme      = u"Lyrics/SaveScheme";
constexpr auto SaveMethod      = u"Lyrics/SaveMethod";
constexpr auto SavePrefer      = u"Lyrics/SavePrefer";
constexpr auto SaveSyncedTag   = u"Lyrics/SaveSyncedTag";
constexpr auto SaveUnsyncedTag = u"Lyrics/SaveUnsyncedTag";
constexpr auto SaveDir         = u"Lyrics/SaveDir";
constexpr auto SaveFilename    = u"Lyrics/SaveFilename";
constexpr auto SkipRemaining   = u"Lyrics/SkipRemaining";
constexpr auto SkipExternal    = u"Lyrics/SkipExternal";
constexpr auto AutoSearch      = u"Lyrics/AutoSearch";
constexpr auto SaveOptions     = u"Lyrics/SaveOptions";
constexpr auto MatchThreshold  = u"Lyrics/MatchThreshold";
} // namespace Settings

enum class ScrollMode : uint8_t
{
    Manual = 0,
    Synced,
    Automatic
};

enum class SaveScheme : uint8_t
{
    Manual = 0,
    Autosave,
    AutosavePeriod
};

enum class SaveMethod : uint8_t
{
    Tag = 0,
    Directory
};

enum class SavePrefer : uint8_t
{
    None = 0,
    Synced,
    Unsynced
};

namespace Defaults {
QStringList paths();
QStringList searchTags();
QMargins margins();
} // namespace Defaults

class LyricsSettings
{
public:
    explicit LyricsSettings(SettingsManager* settingsManager);

private:
    SettingsManager* m_settings;
};
} // namespace Lyrics
} // namespace Fooyin
