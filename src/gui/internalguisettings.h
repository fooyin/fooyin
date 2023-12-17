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
    LibTreeDoubleClick     = 14 | Type::Int,
    LibTreeMiddleClick     = 15 | Type::Int,
    LibTreePlaylistEnabled = 16 | Type::Bool,
    LibTreeAutoSwitch      = 17 | Type::Bool,
    LibTreeAutoPlaylist    = 18 | Type::String,
    LibTreeHeader          = 19 | Type::Bool,
    LibTreeScrollBar       = 20 | Type::Bool,
    LibTreeAltColours      = 21 | Type::Bool,
    LibTreeAppearance      = 22 | Type::Variant,
};
Q_ENUM_NS(GuiInternalSettings)
} // namespace Fooyin::Settings::Gui::Internal
