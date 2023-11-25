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

#include "fygui_export.h"

#include <utils/settings/settingtypes.h>

#include <QObject>

namespace Fooyin {
class SettingsManager;
class LibraryTreeGroupRegistry;
class PresetRegistry;

namespace Settings::Gui {
Q_NAMESPACE_EXPORT(FYGUI_EXPORT)
enum Settings : uint32_t
{
    LayoutEditing              = 1 | SettingsType::Bool,
    StartupBehaviour           = 2 | SettingsType::Int,
    WaitForTracks              = 3 | SettingsType::Bool,
    EditingMenuLevels          = 4 | SettingsType::Int,
    SplitterHandles            = 5 | SettingsType::Bool,
    PlaylistAltColours         = 6 | SettingsType::Bool,
    PlaylistHeader             = 7 | SettingsType::Bool,
    PlaylistScrollBar          = 8 | SettingsType::Bool,
    PlaylistPresets            = 9 | SettingsType::ByteArray,
    CurrentPreset              = 10 | SettingsType::String,
    ElapsedTotal               = 11 | SettingsType::Bool,
    InfoAltColours             = 12 | SettingsType::Bool,
    InfoHeader                 = 13 | SettingsType::Bool,
    InfoScrollBar              = 14 | SettingsType::Bool,
    IconTheme                  = 15 | SettingsType::String,
    LastPlaylistId             = 16 | SettingsType::Int,
    LibraryTreeGrouping        = 17 | SettingsType::ByteArray,
    LibraryTreeDoubleClick     = 18 | SettingsType::Int,
    LibraryTreeMiddleClick     = 19 | SettingsType::Int,
    LibraryTreePlaylistEnabled = 20 | SettingsType::Bool,
    LibraryTreeAutoSwitch      = 21 | SettingsType::Bool,
    LibraryTreeAutoPlaylist    = 22 | SettingsType::String,
    LibraryTreeHeader          = 23 | SettingsType::Bool,
    LibraryTreeScrollBar       = 24 | SettingsType::Bool,
    LibraryTreeAltColours      = 25 | SettingsType::Bool,
    LibraryTreeAppearance      = 26 | SettingsType::Variant,
    PlaylistThumbnailSize      = 27 | SettingsType::Int,
    CursorFollowsPlayback      = 28 | SettingsType::Bool,
    PlaybackFollowsCursor      = 29 | SettingsType::Bool,
    PlaylistTabsSingleHide     = 30 | SettingsType::Bool,
    StatusPlayingScript        = 31 | SettingsType::String,
};
Q_ENUM_NS(Settings)
} // namespace Settings::Gui

class FYGUI_EXPORT GuiSettings
{
public:
    explicit GuiSettings(SettingsManager* settingsManager);
    ~GuiSettings();

    [[nodiscard]] LibraryTreeGroupRegistry* libraryTreeGroupRegistry() const;
    [[nodiscard]] PresetRegistry* playlistPresetRegistry() const;

private:
    SettingsManager* m_settings;
    LibraryTreeGroupRegistry* m_libraryTreeGroupRegistry;
    PresetRegistry* m_playlistPresetRegistry;
};
} // namespace Fooyin
