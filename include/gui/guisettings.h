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

#include "fygui_export.h"

#include <utils/settings/settingsentry.h>

#include <QObject>

namespace Fooyin::Settings::Gui {
Q_NAMESPACE_EXPORT(FYGUI_EXPORT)

enum ToolButtonOption : uint8_t
{
    None    = 0,
    Raise   = 1 << 0,
    Stretch = 1 << 1,
};
Q_DECLARE_FLAGS(ToolButtonOptions, ToolButtonOption)

enum GuiSettings : uint32_t
{
    LayoutEditing              = 1 | Type::Bool,
    StartupBehaviour           = 2 | Type::Int,
    WaitForTracks              = 3 | Type::Bool,
    IconTheme                  = 4 | Type::Int,
    CursorFollowsPlayback      = 5 | Type::Bool,
    PlaybackFollowsCursor      = 6 | Type::Bool,
    ToolButtonStyle            = 7 | Type::Int,
    MainWindowPixelRatio       = 8 | Type::Double,
    StarRatingSize             = 19 | Type::Int,
    Style                      = 10 | Type::String,
    SeekStep                   = 11 | Type::Int,
    ShowStatusTips             = 12 | Type::Bool,
    Theme                      = 13 | Type::Variant,
    ShowSplitterHandles        = 14 | Type::Bool,
    LockSplitterHandles        = 15 | Type::Bool,
    SplitterHandleSize         = 16 | Type::Int,
    VolumeStep                 = 17 | Type::Double,
    SearchSuccessClear         = 18 | Type::Bool,
    SearchAutoDelay            = 29 | Type::Int,
    SearchPlaylistName         = 20 | Type::String,
    SearchPlaylistAppendSearch = 21 | Type::Bool,
    SearchSuccessFocus         = 22 | Type::Bool,
    SearchErrorBg              = 23 | Type::Variant,
    SearchErrorFg              = 24 | Type::Variant,
    SearchSuccessClose         = 25 | Type::Bool,
};
Q_ENUM_NS(GuiSettings)
} // namespace Fooyin::Settings::Gui
