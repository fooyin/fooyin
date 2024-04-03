/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
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

namespace Fooyin {
class SettingsManager;

namespace Settings::Gui::Internal {
Q_NAMESPACE

enum class IconThemeOption
{
    AutoDetect = 0,
    System,
    Light,
    Dark,
};

enum GuiInternalSettings : uint32_t
{
    EditingMenuLevels      = 1 | Type::Int,
    SplitterHandles        = 2 | Type::Bool,
    PlaylistAltColours     = 3 | Type::Bool,
    PlaylistHeader         = 4 | Type::Bool,
    PlaylistScrollBar      = 5 | Type::Bool,
    PlaylistCurrentPreset  = 6 | Type::Int,
    PlaylistThumbnailSize  = 7 | Type::Int,
    PlaylistTabsHide       = 8 | Type::Bool,
    SeekBarElapsedTotal    = 9 | Type::Bool,
    InfoAltColours         = 10 | Type::Bool,
    InfoHeader             = 11 | Type::Bool,
    InfoScrollBar          = 12 | Type::Bool,
    StatusPlayingScript    = 13 | Type::String,
    StatusSelectionScript  = 14 | Type::String,
    StatusShowIcon         = 15 | Type::Bool,
    StatusShowSelection    = 16 | Type::Bool,
    LibTreeDoubleClick     = 17 | Type::Int,
    LibTreeMiddleClick     = 18 | Type::Int,
    LibTreePlaylistEnabled = 19 | Type::Bool,
    LibTreeAutoSwitch      = 20 | Type::Bool,
    LibTreeAutoPlaylist    = 21 | Type::String,
    LibTreeScrollBar       = 22 | Type::Bool,
    LibTreeAltColours      = 23 | Type::Bool,
    LibTreeFont            = 24 | Type::String,
    LibTreeColour          = 25 | Type::String,
    LibTreeRowHeight       = 26 | Type::Int,
    LibTreeAppearance      = 27 | Type::Variant,
    SystemIconTheme        = 28 | Type::String,
    SeekBarLabels          = 29 | Type::Bool,
    DirBrowserPath         = 30 | Type::String,
    DirBrowserIcons        = 31 | Type::Bool,
    DirBrowserDoubleClick  = 32 | Type::Int,
    DirBrowserMiddleClick  = 33 | Type::Int,
    DirBrowserMode         = 34 | Type::Int,
    DirBrowserListIndent   = 35 | Type::Bool,
    DirBrowserControls     = 36 | Type::Bool,
    DirBrowserLocation     = 37 | Type::Bool,
    WindowTitleTrackScript = 38 | Type::String,
};
Q_ENUM_NS(GuiInternalSettings)
} // namespace Settings::Gui::Internal

class GuiSettings
{
public:
    explicit GuiSettings(SettingsManager* settingsManager);

private:
    SettingsManager* m_settings;
};
} // namespace Fooyin
