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

#include <core/track.h>
#include <gui/theme/fytheme.h>
#include <utils/settings/settingsentry.h>

namespace Fooyin {
class SettingsManager;

enum class IconThemeOption : uint8_t
{
    AutoDetect = 0,
    System,
    Light,
    Dark,
};

enum class SelectionDisplay : uint8_t
{
    PreferPlaying = 0,
    PreferSelection
};

struct CoverPaths
{
    QStringList frontCoverPaths;
    QStringList backCoverPaths;
    QStringList artistPaths;

    friend QDataStream& operator<<(QDataStream& stream, const CoverPaths& paths)
    {
        stream << paths.frontCoverPaths;
        stream << paths.backCoverPaths;
        stream << paths.artistPaths;
        return stream;
    }

    friend QDataStream& operator>>(QDataStream& stream, CoverPaths& paths)
    {
        stream >> paths.frontCoverPaths;
        stream >> paths.backCoverPaths;
        stream >> paths.artistPaths;
        return stream;
    }
};

enum class ArtworkSaveMethod : uint8_t
{
    Embedded = 0,
    Directory
};

struct ArtworkSaveOptions
{
    ArtworkSaveMethod method{ArtworkSaveMethod::Embedded};
    QString dir;
    QString filename;
    QString format;
    int quality{90};

    friend QDataStream& operator<<(QDataStream& stream, const ArtworkSaveOptions& options)
    {
        stream << options.method;
        stream << options.dir;
        stream << options.filename;
        stream << options.format;
        stream << options.quality;
        return stream;
    }

    friend QDataStream& operator>>(QDataStream& stream, ArtworkSaveOptions& options)
    {
        stream >> options.method;
        stream >> options.dir;
        stream >> options.filename;

        stream.startTransaction();
        stream >> options.format;
        if(!stream.commitTransaction()) {
            stream.resetStatus();
            options.format.clear();
        }

        stream.startTransaction();
        stream >> options.quality;
        if(!stream.commitTransaction()) {
            stream.resetStatus();
            options.quality = 90;
        }

        return stream;
    }
};
using ArtworkSaveMethods = QMap<Track::Cover, ArtworkSaveOptions>;

namespace Settings::Gui::Internal {
Q_NAMESPACE

constexpr auto PlaylistCurrentPreset = "PlaylistWidget/CurrentPreset";
constexpr auto LastFilePath          = "Interface/LastFilePath";

enum GuiInternalSettings : uint32_t
{
    EditingMenuLevels         = 1 | Type::Int,
    PlaylistAltColours        = 2 | Type::Bool,
    PlaylistHeader            = 3 | Type::Bool,
    PlaylistScrollBar         = 4 | Type::Bool,
    StatusPlayingScript       = 5 | Type::String,
    StatusSelectionScript     = 6 | Type::String,
    StatusShowIcon            = 7 | Type::Bool,
    StatusShowSelection       = 8 | Type::Bool,
    SystemIconTheme           = 9 | Type::String,
    WindowTitleTrackScript    = 10 | Type::String,
    TrackCoverPaths           = 11 | Type::Variant,
    TrackCoverDisplayOption   = 12 | Type::Int,
    PlaylistImagePadding      = 13 | Type::Int,
    PlaylistImagePaddingTop   = 14 | Type::Int,
    PixmapCacheSize           = 15 | Type::Int,
    EditableLayoutMargin      = 16 | Type::Int,
    PlaylistTabsAddButton     = 17 | Type::Bool,
    ShowTrayIcon              = 18 | Type::Bool,
    TrayOnClose               = 19 | Type::Bool,
    PlaylistTabsCloseButton   = 20 | Type::Bool,
    PlaylistTabsMiddleClose   = 21 | Type::Bool,
    PlaylistTabsExpand        = 22 | Type::Bool,
    PlaylistTabsClearButton   = 23 | Type::Bool,
    PlaylistMiddleClick       = 24 | Type::Int,
    InfoDisplayPrefer         = 25 | Type::Int,
    SystemStyle               = 26 | Type::String,
    SystemFont                = 27 | Type::Variant,
    SystemPalette             = 28 | Type::Variant,
    LibTreeIconSize           = 29 | Type::Variant,
    ArtworkSaveMethods        = 30 | Type::Variant,
    ArtworkAutoSearch         = 31 | Type::Bool,
    ArtworkTitleField         = 32 | Type::String,
    ArtworkAlbumField         = 33 | Type::String,
    ArtworkArtistField        = 34 | Type::String,
    ArtworkMatchThreshold     = 35 | Type::Int,
    ArtworkDownloadThumbSize  = 36 | Type::Int,
    ImageAllocationLimit      = 37 | Type::Int,
    PlaylistTrackPreloadCount = 38 | Type::Int,
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

Q_DECLARE_METATYPE(Fooyin::CoverPaths)
Q_DECLARE_METATYPE(Fooyin::FyTheme)
Q_DECLARE_METATYPE(Fooyin::ArtworkSaveMethods)
