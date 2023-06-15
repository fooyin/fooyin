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
    LayoutEditing       = 1 | Utils::Settings::Bool,
    Geometry            = 2 | Utils::Settings::ByteArray,
    SettingsGeometry    = 3 | Utils::Settings::ByteArray,
    SplitterHandles     = 4 | Utils::Settings::Bool,
    DiscHeaders         = 5 | Utils::Settings::Bool,
    SplitDiscs          = 6 | Utils::Settings::Bool,
    SimplePlaylist      = 7 | Utils::Settings::Bool,
    PlaylistAltColours  = 8 | Utils::Settings::Bool,
    PlaylistHeader      = 9 | Utils::Settings::Bool,
    PlaylistScrollBar   = 10 | Utils::Settings::Bool,
    ElapsedTotal        = 11 | Utils::Settings::Bool,
    InfoAltColours      = 12 | Utils::Settings::Bool,
    InfoHeader          = 13 | Utils::Settings::Bool,
    InfoScrollBar       = 14 | Utils::Settings::Bool,
    EditingMenuLevels   = 15 | Utils::Settings::Int,
    IconTheme           = 16 | Utils::Settings::String,
    LastPlaylistId      = 17 | Utils::Settings::Int,
    LibraryTreeGrouping = 18 | Utils::Settings::String,
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
