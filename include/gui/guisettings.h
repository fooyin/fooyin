/*
 * Fooyin
 * Copyright 2022-2023, Luke Taylor <LukeT1@proton.me>
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

#include <utils/settings/settingtypes.h>

#include <QObject>

namespace Fy {
namespace Utils {
class SettingsManager;
}

namespace Gui::Settings {
Q_NAMESPACE
enum Gui : uint32_t
{
    LayoutEditing              = 1 | Utils::Settings::Bool,
    StartupBehaviour           = 2 | Utils::Settings::Int,
    WaitForTracks              = 3 | Utils::Settings::Bool,
    Geometry                   = 4 | Utils::Settings::ByteArray,
    SettingsGeometry           = 5 | Utils::Settings::ByteArray,
    EditingMenuLevels          = 6 | Utils::Settings::Int,
    SplitterHandles            = 7 | Utils::Settings::Bool,
    PlaylistAltColours         = 8 | Utils::Settings::Bool,
    PlaylistHeader             = 9 | Utils::Settings::Bool,
    PlaylistScrollBar          = 10 | Utils::Settings::Bool,
    PlaylistPresets            = 11 | Utils::Settings::ByteArray,
    CurrentPreset              = 12 | Utils::Settings::String,
    ElapsedTotal               = 13 | Utils::Settings::Bool,
    InfoAltColours             = 14 | Utils::Settings::Bool,
    InfoHeader                 = 15 | Utils::Settings::Bool,
    InfoScrollBar              = 16 | Utils::Settings::Bool,
    IconTheme                  = 17 | Utils::Settings::String,
    LastPlaylistId             = 18 | Utils::Settings::Int,
    LibraryTreeGrouping        = 19 | Utils::Settings::ByteArray,
    LibraryTreeDoubleClick     = 20 | Utils::Settings::Int,
    LibraryTreeMiddleClick     = 21 | Utils::Settings::Int,
    LibraryTreePlaylistEnabled = 22 | Utils::Settings::Bool,
    LibraryTreeAutoSwitch      = 23 | Utils::Settings::Bool,
    LibraryTreeAutoPlaylist    = 24 | Utils::Settings::String,
    LibraryTreeHeader          = 25 | Utils::Settings::Bool,
    LibraryTreeScrollBar       = 26 | Utils::Settings::Bool,
    LibraryTreeAltColours      = 27 | Utils::Settings::Bool,
    ScriptSandboxState         = 28 | Utils::Settings::ByteArray,
};
Q_ENUM_NS(Gui)

class GuiSettings
{
public:
    explicit GuiSettings(Utils::SettingsManager* settingsManager);

private:
    Utils::SettingsManager* m_settings;
};
} // namespace Gui::Settings
} // namespace Fy
