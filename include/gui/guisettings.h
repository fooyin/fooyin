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
    CurrentPreset              = 9 | SettingsType::String,
    ElapsedTotal               = 10 | SettingsType::Bool,
    InfoAltColours             = 11 | SettingsType::Bool,
    InfoHeader                 = 12 | SettingsType::Bool,
    InfoScrollBar              = 13 | SettingsType::Bool,
    IconTheme                  = 14 | SettingsType::String,
    LastPlaylistId             = 15 | SettingsType::Int,
    LibraryTreeDoubleClick     = 16 | SettingsType::Int,
    LibraryTreeMiddleClick     = 17 | SettingsType::Int,
    LibraryTreePlaylistEnabled = 18 | SettingsType::Bool,
    LibraryTreeAutoSwitch      = 19 | SettingsType::Bool,
    LibraryTreeAutoPlaylist    = 20 | SettingsType::String,
    LibraryTreeHeader          = 21 | SettingsType::Bool,
    LibraryTreeScrollBar       = 22 | SettingsType::Bool,
    LibraryTreeAltColours      = 23 | SettingsType::Bool,
    LibraryTreeAppearance      = 24 | SettingsType::Variant,
    PlaylistThumbnailSize      = 25 | SettingsType::Int,
    CursorFollowsPlayback      = 26 | SettingsType::Bool,
    PlaybackFollowsCursor      = 27 | SettingsType::Bool,
    PlaylistTabsSingleHide     = 28 | SettingsType::Bool,
    StatusPlayingScript        = 29 | SettingsType::String,
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
    std::unique_ptr<LibraryTreeGroupRegistry> m_libraryTreeGroupRegistry;
    std::unique_ptr<PresetRegistry> m_playlistPresetRegistry;
};
} // namespace Fooyin
