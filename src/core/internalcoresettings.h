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

#include "fycore_export.h"

#include <utils/settings/settingsentry.h>

namespace Fooyin {
class SettingsManager;

enum class ReplayGainType : uint8_t
{
    Track = 0,
    Album,
    // Track gain if shuffle, else album gain
    PlaybackOrder
};

namespace Settings::Core::Internal {
Q_NAMESPACE_EXPORT(FYCORE_EXPORT)

constexpr auto PlaylistSkipUnavailable = "Playlist/SkipUnavailable";
constexpr auto PlaylistSaveMetadata    = "Playlist/SaveMetadata";
constexpr auto PlaylistSavePathType    = "Playlist/SavePathType";
constexpr auto AutoExportPlaylists     = "Playlist/AutoExport";
constexpr auto AutoExportPlaylistsType = "Playlist/AutoExportType";
constexpr auto AutoExportPlaylistsPath = "Playlist/AutoExportPath";
constexpr auto MarkUnavailable         = "Library/MarkUnavailable";
constexpr auto MarkUnavailableStartup  = "Library/MarkUnavailableOnStartup";
constexpr auto SaveActivePlaylistState = "Playlist/SaveActivePlaylistState";
constexpr auto SavePlaybackState       = "Player/SavePlaybackState";
constexpr auto LibraryRestrictTypes    = "Library/RestrictTypes";
constexpr auto LibraryExcludeTypes     = "Library/ExcludeTypes";
constexpr auto ExternalRestrictTypes   = "Library/ExternalRestrictTypes";
constexpr auto ExternalExcludeTypes    = "Library/ExternalExcludeTypes";
constexpr auto FFmpegAllExtensions     = "Engine/FFmpegAllExtensions";

enum CoreInternalSettings : uint32_t
{
    MonitorLibraries  = 0 | Type::Bool,
    MuteVolume        = 1 | Type::Double,
    DisabledPlugins   = 2 | Type::StringList,
    EngineFading      = 3 | Type::Bool,
    FadingValues      = 4 | Type::Variant,
    EngineCrossfading = 5 | Type::Bool,
    CrossfadingValues = 6 | Type::Variant,
    VBRUpdateInterval = 7 | Type::Int,
    ProxyMode         = 8 | Type::Int,
    ProxyType         = 9 | Type::Int,
    ProxyHost         = 10 | Type::String,
    ProxyPort         = 11 | Type::Int,
    ProxyAuth         = 12 | Type::Bool,
    ProxyUsername     = 13 | Type::String,
    ProxyPassword     = 14 | Type::String,
};
Q_ENUM_NS(CoreInternalSettings)
} // namespace Settings::Core::Internal

class CoreSettings
{
public:
    explicit CoreSettings(SettingsManager* settingsManager);
    ~CoreSettings();

private:
    SettingsManager* m_settings;
};
} // namespace Fooyin
