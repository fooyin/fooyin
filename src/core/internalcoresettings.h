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
};

struct FadingIntervals
{
    int inPauseStop{1000};
    int outPauseStop{1000};
    int inSeek{1000};
    int outSeek{1000};
    int inChange{1000};
    int outChange{1000};

    friend QDataStream& operator<<(QDataStream& stream, const FadingIntervals& fading)
    {
        stream << fading.inPauseStop;
        stream << fading.outPauseStop;
        stream << fading.inSeek;
        stream << fading.outSeek;
        stream << fading.inChange;
        stream << fading.outChange;
        return stream;
    }

    friend QDataStream& operator>>(QDataStream& stream, FadingIntervals& fading)
    {
        stream >> fading.inPauseStop;
        stream >> fading.outPauseStop;
        stream >> fading.inSeek;
        stream >> fading.outSeek;
        stream >> fading.inChange;
        stream >> fading.outChange;
        return stream;
    }
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
constexpr auto SavePlaybackState       = "Player/SavePlaybackState";
constexpr auto LibraryRestrictTypes    = "Library/RestrictTypes";
constexpr auto LibraryExcludeTypes     = "Library/ExcludeTypes";
constexpr auto ExternalRestrictTypes   = "Library/ExternalRestrictTypes";
constexpr auto ExternalExcludeTypes    = "Library/ExternalExcludeTypes";

enum CoreInternalSettings : uint32_t
{
    MonitorLibraries = 0 | Settings::Bool,
    MuteVolume       = 1 | Settings::Double,
    DisabledPlugins  = 2 | Settings::StringList,
    EngineFading     = 3 | Type::Bool,
    FadingIntervals  = 4 | Type::Variant,
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

Q_DECLARE_METATYPE(Fooyin::FadingIntervals)
