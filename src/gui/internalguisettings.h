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
    DirBrowserShowSymLinks    = 26 | Type::Bool,
    DirBrowserShowHidden      = 27 | Type::Bool,
    WindowTitleTrackScript    = 28 | Type::String,
    TrackCoverPaths           = 29 | Type::Variant,
    TrackCoverDisplayOption   = 30 | Type::Int,
    PlaylistImagePadding      = 31 | Type::Int,
    PlaylistImagePaddingTop   = 32 | Type::Int,
    PixmapCacheSize           = 33 | Type::Int,
    LibTreeSendPlayback       = 34 | Type::Bool,
    DirBrowserSendPlayback    = 35 | Type::Bool,
    EditableLayoutMargin      = 36 | Type::Int,
    PlaylistTabsAddButton     = 37 | Type::Bool,
    LibTreeRestoreState       = 38 | Type::Bool,
    ShowTrayIcon              = 39 | Type::Bool,
    TrayOnClose               = 40 | Type::Bool,
    LibTreeKeepAlive          = 41 | Type::Bool,
    PlaylistTabsCloseButton   = 42 | Type::Bool,
    PlaylistTabsMiddleClose   = 43 | Type::Bool,
    PlaylistTabsExpand        = 44 | Type::Bool,
    LibTreeAnimated           = 45 | Type::Bool,
    PlaylistTabsClearButton   = 46 | Type::Bool,
    LibTreeHeader             = 47 | Type::Bool,
    QueueViewerShowIcon       = 48 | Type::Bool,
    QueueViewerIconSize       = 49 | Type::Variant,
    QueueViewerHeader         = 50 | Type::Bool,
    QueueViewerScrollBar      = 51 | Type::Bool,
    QueueViewerAltColours     = 52 | Type::Bool,
    QueueViewerLeftScript     = 53 | Type::String,
    QueueViewerRightScript    = 54 | Type::String,
    QueueViewerShowCurrent    = 55 | Type::Bool,
    PlaylistMiddleClick       = 56 | Type::Int,
    InfoDisplayPrefer         = 57 | Type::Int,
    SystemStyle               = 58 | Type::String,
    SystemFont                = 59 | Type::Variant,
    SystemPalette             = 60 | Type::Variant,
    DirBrowserShowHorizScroll = 61 | Type::Bool,
    LibTreeIconSize           = 62 | Type::Variant,
    ArtworkSaveMethods        = 63 | Type::Variant,
    ArtworkAutoSearch         = 64 | Type::Bool,
    ArtworkTitleField         = 65 | Type::String,
    ArtworkAlbumField         = 66 | Type::String,
    ArtworkArtistField        = 67 | Type::String,
    ArtworkMatchThreshold     = 68 | Type::Int,
    ArtworkDownloadThumbSize  = 69 | Type::Int,
    ImageAllocationLimit      = 70 | Type::Int,
    PlaylistTrackPreloadCount = 71 | Type::Int,
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
