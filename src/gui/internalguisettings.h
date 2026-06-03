/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <luket@pm.me>
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
    static constexpr qint32 Magic   = -0x434F5650;
    static constexpr qint32 Version = 1;

    QStringList frontCoverPaths;
    QStringList backCoverPaths;
    QStringList artistPaths;
    QString frontPlaceholder;
    QString backPlaceholder;
    QString artistPlaceholder;

    friend QDataStream& operator<<(QDataStream& stream, const CoverPaths& paths)
    {
        stream << Magic;
        stream << Version;
        stream << paths.frontCoverPaths;
        stream << paths.backCoverPaths;
        stream << paths.artistPaths;
        stream << paths.frontPlaceholder;
        stream << paths.backPlaceholder;
        stream << paths.artistPlaceholder;
        return stream;
    }

    friend QDataStream& operator>>(QDataStream& stream, CoverPaths& paths)
    {
        qint32 magic{0};
        qint32 version{0};

        stream.startTransaction();
        stream >> magic;
        stream >> version;

        if(magic == Magic && version == Version) {
            stream >> paths.frontCoverPaths;
            stream >> paths.backCoverPaths;
            stream >> paths.artistPaths;
            stream >> paths.frontPlaceholder;
            stream >> paths.backPlaceholder;
            stream >> paths.artistPlaceholder;

            if(stream.commitTransaction()) {
                return stream;
            }
        }

        stream.rollbackTransaction();

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

enum class PlaylistBgImage : uint8_t
{
    None = 0,
    AlbumCover,
    Custom
};

enum class PlaylistBgImagePosition : uint8_t
{
    TopLeft = 0,
    Top,
    TopRight,
    Left,
    Middle,
    Right,
    BottomLeft,
    Bottom,
    BottomRight
};

enum class PlaylistBgScaling : uint8_t
{
    ScaledAndCropped = 0,
    Scaled,
    ScaledKeepProportions,
    OriginalSize
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
Q_NAMESPACE_EXPORT(FYGUI_EXPORT)

constexpr auto PlaylistCurrentPreset = "PlaylistWidget/CurrentPreset";
constexpr auto LastFilePath          = "Interface/LastFilePath";

enum GuiInternalSettings : uint32_t
{
    EditingMenuLevels                        = 1 | Type::Int,
    PlaylistAltColours                       = 2 | Type::Bool,
    PlaylistHeader                           = 3 | Type::Bool,
    PlaylistScrollBar                        = 4 | Type::Bool,
    StatusPlayingScript                      = 5 | Type::String,
    StatusSelectionScript                    = 6 | Type::String,
    StatusShowIcon                           = 7 | Type::Bool,
    StatusShowSelection                      = 8 | Type::Bool,
    SystemIconTheme                          = 9 | Type::String,
    WindowTitleTrackScript                   = 10 | Type::String,
    TrackCoverPaths                          = 11 | Type::Variant,
    TrackCoverDisplayOption                  = 12 | Type::Int,
    PlaylistImagePadding                     = 13 | Type::Int,
    PlaylistImagePaddingTop                  = 14 | Type::Int,
    PixmapCacheSize                          = 15 | Type::Int,
    EditableLayoutMargin                     = 16 | Type::Int,
    PlaylistTabsAddButton                    = 17 | Type::Bool,
    ShowTrayIcon                             = 18 | Type::Bool,
    TrayOnClose                              = 19 | Type::Bool,
    PlaylistTabsCloseButton                  = 20 | Type::Bool,
    PlaylistTabsMiddleClose                  = 21 | Type::Bool,
    PlaylistTabsExpand                       = 22 | Type::Bool,
    PlaylistTabsClearButton                  = 23 | Type::Bool,
    PlaylistMiddleClick                      = 24 | Type::Int,
    InfoDisplayPrefer                        = 25 | Type::Int,
    SystemStyle                              = 26 | Type::String,
    SystemFont                               = 27 | Type::Variant,
    SystemPalette                            = 28 | Type::Variant,
    LibTreeIconSize                          = 29 | Type::Variant,
    ArtworkSaveMethods                       = 30 | Type::Variant,
    ArtworkAutoSearch                        = 31 | Type::Bool,
    ArtworkTitleField                        = 32 | Type::String,
    ArtworkAlbumField                        = 33 | Type::String,
    ArtworkArtistField                       = 34 | Type::String,
    ArtworkMatchThreshold                    = 35 | Type::Int,
    ArtworkDownloadThumbSize                 = 36 | Type::Int,
    ImageAllocationLimit                     = 37 | Type::Int,
    PlaylistTrackPreloadCount                = 38 | Type::Int,
    TrackCoverSourcePreference               = 39 | Type::Int,
    PlaylistInlineTagEditing                 = 40 | Type::Bool,
    ContextMenuTrackDisabledSections         = 41 | Type::StringList,
    ContextMenuPlaylistDisabledSections      = 42 | Type::StringList,
    ContextMenuTrackLayout                   = 43 | Type::StringList,
    ContextMenuPlaylistLayout                = 44 | Type::StringList,
    ContextMenuLibraryTreeDisabledSections   = 45 | Type::StringList,
    ContextMenuLibraryTreeLayout             = 46 | Type::StringList,
    PropertiesSidebarTrackScript             = 47 | Type::String,
    StatusPlaylistScript                     = 48 | Type::String,
    StatusShowPlaylist                       = 49 | Type::Bool,
    TrackCoverThumbnailGroupScript           = 50 | Type::String,
    ContextMenuDirBrowserDisabledSections    = 51 | Type::StringList,
    ContextMenuDirBrowserLayout              = 52 | Type::StringList,
    ContextMenuLayoutEditingDisabledSections = 53 | Type::StringList,
    ContextMenuLayoutEditingLayout           = 54 | Type::StringList,
    PlaylistBackgroundImageMode              = 55 | Type::Int,
    PlaylistBackgroundCustomImage            = 56 | Type::String,
    PlaylistBackgroundScaling                = 57 | Type::Int,
    PlaylistBackgroundPosition               = 58 | Type::Int,
    PlaylistBackgroundMaxSize                = 59 | Type::Int,
    PlaylistBackgroundBlur                   = 60 | Type::Int,
    PlaylistBackgroundOpacity                = 61 | Type::Int,
    PlaylistBackgroundFadeDuration           = 62 | Type::Int,
    PlaylistBackgroundCoverType              = 63 | Type::Int,
    NowPlayingOutputEnabled                  = 64 | Type::Bool,
    NowPlayingOutputScript                   = 65 | Type::String,
    NowPlayingOutputUpdateEvents             = 66 | Type::Int,
    NowPlayingOutputTargets                  = 67 | Type::Int,
    NowPlayingOutputFilePath                 = 68 | Type::String,
    NowPlayingOutputOptions                  = 69 | Type::Int,
    NowPlayingOutputAppendLineLimit          = 70 | Type::Int,
    OutputDeviceRefreshMs                    = 71 | Type::Int,
};
Q_ENUM_NS(GuiInternalSettings)
} // namespace Settings::Gui::Internal

class FYGUI_EXPORT GuiSettings
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
