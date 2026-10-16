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
constexpr auto LastPlaylistPath      = "Playlist/LastPlaylistPath";
constexpr auto ConfirmMetadataWipe   = "MetadataLookup/ConfirmWipeWritableTags";
constexpr auto ShowLayoutEditingHint = "Interface/ShowLayoutEditingHint";

enum GuiInternalSettings : uint32_t
{
    EditingMenuLevels                        = 1 | Type::Int,
    StatusPlayingScript                      = 2 | Type::String,
    StatusSelectionScript                    = 3 | Type::String,
    StatusShowIcon                           = 4 | Type::Bool,
    StatusShowSelection                      = 5 | Type::Bool,
    SystemIconTheme                          = 6 | Type::String,
    SystemStyle                              = 7 | Type::String,
    SystemFont                               = 8 | Type::Variant,
    SystemPalette                            = 9 | Type::Variant,
    WindowTitleTrackScript                   = 10 | Type::String,
    TrackCoverPaths                          = 11 | Type::Variant,
    PixmapCacheSize                          = 12 | Type::Int,
    EditableLayoutMargin                     = 13 | Type::Int,
    ShowTrayIcon                             = 14 | Type::Bool,
    TrayOnClose                              = 15 | Type::Bool,
    LibTreeIconSize                          = 16 | Type::Variant,
    ArtworkSaveMethods                       = 17 | Type::Variant,
    ArtworkAutoSearch                        = 18 | Type::Bool,
    ArtworkTitleField                        = 19 | Type::String,
    ArtworkAlbumField                        = 20 | Type::String,
    ArtworkArtistField                       = 21 | Type::String,
    ArtworkMatchThreshold                    = 22 | Type::Int,
    ArtworkDownloadThumbSize                 = 23 | Type::Int,
    ImageAllocationLimit                     = 24 | Type::Int,
    PlaylistTrackPreloadCount                = 25 | Type::Int,
    TrackCoverSourcePreference               = 26 | Type::Int,
    PlaylistInlineTagEditing                 = 27 | Type::Bool,
    ContextMenuTrackDisabledSections         = 28 | Type::StringList,
    ContextMenuPlaylistDisabledSections      = 29 | Type::StringList,
    ContextMenuTrackLayout                   = 30 | Type::StringList,
    ContextMenuPlaylistLayout                = 31 | Type::StringList,
    ContextMenuLibraryTreeDisabledSections   = 32 | Type::StringList,
    ContextMenuLibraryTreeLayout             = 33 | Type::StringList,
    PropertiesSidebarTrackScript             = 34 | Type::String,
    StatusPlaylistScript                     = 35 | Type::String,
    StatusShowPlaylist                       = 36 | Type::Bool,
    TrackCoverThumbnailGroupScript           = 37 | Type::String,
    ContextMenuDirBrowserDisabledSections    = 38 | Type::StringList,
    ContextMenuDirBrowserLayout              = 39 | Type::StringList,
    ContextMenuLayoutEditingDisabledSections = 40 | Type::StringList,
    ContextMenuLayoutEditingLayout           = 41 | Type::StringList,
    NowPlayingOutputEnabled                  = 42 | Type::Bool,
    NowPlayingOutputScript                   = 43 | Type::String,
    NowPlayingOutputUpdateEvents             = 44 | Type::Int,
    NowPlayingOutputTargets                  = 45 | Type::Int,
    NowPlayingOutputFilePath                 = 46 | Type::String,
    NowPlayingOutputOptions                  = 47 | Type::Int,
    NowPlayingOutputAppendLineLimit          = 48 | Type::Int,
    OutputDeviceRefreshMs                    = 49 | Type::Int,
    PlaylistPlayingRowColour                 = 50 | Type::Variant,
    PlaylistPlayingRowFont                   = 51 | Type::Variant,
    ArtistCoverFallbackToFront               = 52 | Type::Bool,
    StatusDoubleClick                        = 53 | Type::Int,
    StatusMiddleClick                        = 54 | Type::Int,
    ContextMenuQueueViewerDisabledSections   = 55 | Type::StringList,
    ContextMenuQueueViewerLayout             = 56 | Type::StringList,
    PlaylistInlineEditDelayMs                = 57 | Type::Int,
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
