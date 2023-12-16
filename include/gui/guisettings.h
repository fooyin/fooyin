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

#include <utils/settings/settingsentry.h>

#include <QObject>

namespace Fooyin {
class SettingsManager;
class LibraryTreeGroupRegistry;
class PresetRegistry;

namespace Settings::Gui {
Q_NAMESPACE_EXPORT(FYGUI_EXPORT)
enum Settings : uint32_t
{
    LayoutEditing          = 1 | Type::Bool,
    StartupBehaviour       = 2 | Type::Int,
    WaitForTracks          = 3 | Type::Bool,
    EditingMenuLevels      = 4 | Type::Int,
    SplitterHandles        = 5 | Type::Bool,
    PlaylistAltColours     = 6 | Type::Bool,
    PlaylistHeader         = 7 | Type::Bool,
    PlaylistScrollBar      = 8 | Type::Bool,
    CurrentPreset          = 9 | Type::String,
    ElapsedTotal           = 10 | Type::Bool,
    InfoAltColours         = 11 | Type::Bool,
    InfoHeader             = 12 | Type::Bool,
    InfoScrollBar          = 13 | Type::Bool,
    IconTheme              = 14 | Type::String,
    LastPlaylistId         = 15 | Type::Int,
    PlaylistThumbnailSize  = 16 | Type::Int,
    CursorFollowsPlayback  = 17 | Type::Bool,
    PlaybackFollowsCursor  = 18 | Type::Bool,
    PlaylistTabsSingleHide = 19 | Type::Bool,
    StatusPlayingScript    = 20 | Type::String,
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
