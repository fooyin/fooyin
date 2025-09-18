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

    friend QDataStream& operator<<(QDataStream& stream, const ArtworkSaveOptions& options)
    {
        stream << options.method;
        stream << options.dir;
        stream << options.filename;
        return stream;
    }

    friend QDataStream& operator>>(QDataStream& stream, ArtworkSaveOptions& options)
    {
        stream >> options.method;
        stream >> options.dir;
        stream >> options.filename;
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
    LibTreeDoubleClick        = 9 | Type::Int,
    LibTreeMiddleClick        = 10 | Type::Int,
    LibTreePlaylistEnabled    = 11 | Type::Bool,
    LibTreeAutoSwitch         = 12 | Type::Bool,
    LibTreeAutoPlaylist       = 13 | Type::String,
    LibTreeScrollBar          = 14 | Type::Bool,
    LibTreeAltColours         = 15 | Type::Bool,
    LibTreeRowHeight          = 16 | Type::Int,
    SystemIconTheme           = 17 | Type::String,
    DirBrowserPath            = 18 | Type::String,
    DirBrowserIcons           = 19 | Type::Bool,
    DirBrowserDoubleClick     = 20 | Type::Int,
    DirBrowserMiddleClick     = 21 | Type::Int,
    DirBrowserMode            = 22 | Type::Int,
    DirBrowserListIndent      = 23 | Type::Bool,
    DirBrowserControls        = 24 | Type::Bool,
    DirBrowserLocation        = 25 | Type::Bool,
    WindowTitleTrackScript    = 26 | Type::String,
    TrackCoverPaths           = 27 | Type::Variant,
    TrackCoverDisplayOption   = 28 | Type::Int,
    PlaylistImagePadding      = 29 | Type::Int,
    PlaylistImagePaddingTop   = 30 | Type::Int,
    PixmapCacheSize           = 31 | Type::Int,
    LibTreeSendPlayback       = 32 | Type::Bool,
    DirBrowserSendPlayback    = 33 | Type::Bool,
    EditableLayoutMargin      = 34 | Type::Int,
    PlaylistTabsAddButton     = 35 | Type::Bool,
    LibTreeRestoreState       = 36 | Type::Bool,
    ShowTrayIcon              = 37 | Type::Bool,
    TrayOnClose               = 38 | Type::Bool,
    LibTreeKeepAlive          = 39 | Type::Bool,
    PlaylistTabsCloseButton   = 40 | Type::Bool,
    PlaylistTabsMiddleClose   = 41 | Type::Bool,
    PlaylistTabsExpand        = 42 | Type::Bool,
    LibTreeAnimated           = 43 | Type::Bool,
    PlaylistTabsClearButton   = 44 | Type::Bool,
    LibTreeHeader             = 45 | Type::Bool,
    QueueViewerShowIcon       = 46 | Type::Bool,
    QueueViewerIconSize       = 47 | Type::Variant,
    QueueViewerHeader         = 48 | Type::Bool,
    QueueViewerScrollBar      = 49 | Type::Bool,
    QueueViewerAltColours     = 50 | Type::Bool,
    QueueViewerLeftScript     = 51 | Type::String,
    QueueViewerRightScript    = 52 | Type::String,
    QueueViewerShowCurrent    = 53 | Type::Bool,
    PlaylistMiddleClick       = 54 | Type::Int,
    InfoDisplayPrefer         = 55 | Type::Int,
    SystemStyle               = 56 | Type::String,
    SystemFont                = 57 | Type::Variant,
    SystemPalette             = 58 | Type::Variant,
    DirBrowserShowHorizScroll = 59 | Type::Bool,
    LibTreeIconSize           = 60 | Type::Variant,
    ArtworkSaveMethods        = 61 | Type::Variant,
    ArtworkAutoSearch         = 62 | Type::Bool,
    ArtworkTitleField         = 63 | Type::String,
    ArtworkAlbumField         = 64 | Type::String,
    ArtworkArtistField        = 65 | Type::String,
    ArtworkMatchThreshold     = 66 | Type::Int,
    ArtworkDownloadThumbSize  = 67 | Type::Int,
    ImageAllocationLimit      = 68 | Type::Int,
    PlaylistTrackPreloadCount = 69 | Type::Int,
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
