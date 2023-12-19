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

#include <utils/settings/settingsentry.h>

namespace Fooyin::Settings::Gui::Internal {
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
    PlaylistCurrentPreset  = 6 | Type::String,
    PlaylistThumbnailSize  = 7 | Type::Int,
    PlaylistTabsHide       = 8 | Type::Bool,
    ElapsedTotal           = 9 | Type::Bool,
    InfoAltColours         = 10 | Type::Bool,
    InfoHeader             = 11 | Type::Bool,
    InfoScrollBar          = 12 | Type::Bool,
    StatusPlayingScript    = 13 | Type::String,
    StatusShowIcon         = 14 | Type::Bool,
    LibTreeDoubleClick     = 15 | Type::Int,
    LibTreeMiddleClick     = 16 | Type::Int,
    LibTreePlaylistEnabled = 17 | Type::Bool,
    LibTreeAutoSwitch      = 18 | Type::Bool,
    LibTreeAutoPlaylist    = 19 | Type::String,
    LibTreeHeader          = 20 | Type::Bool,
    LibTreeScrollBar       = 21 | Type::Bool,
    LibTreeAltColours      = 22 | Type::Bool,
    LibTreeAppearance      = 23 | Type::Variant,
    SystemIconTheme        = 24 | Type::String,
};
Q_ENUM_NS(GuiInternalSettings)
} // namespace Fooyin::Settings::Gui::Internal
