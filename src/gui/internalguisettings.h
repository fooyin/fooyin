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
    InfoAltColours            = 5 | Type::Bool,
    InfoHeader                = 6 | Type::Bool,
    InfoScrollBar             = 7 | Type::Bool,
    StatusPlayingScript       = 8 | Type::String,
    StatusSelectionScript     = 9 | Type::String,
    StatusShowIcon            = 10 | Type::Bool,
    StatusShowSelection       = 11 | Type::Bool,
    LibTreeDoubleClick        = 12 | Type::Int,
    LibTreeMiddleClick        = 13 | Type::Int,
    LibTreePlaylistEnabled    = 14 | Type::Bool,
    LibTreeAutoSwitch         = 15 | Type::Bool,
    LibTreeAutoPlaylist       = 16 | Type::String,
    LibTreeScrollBar          = 17 | Type::Bool,
    LibTreeAltColours         = 18 | Type::Bool,
    LibTreeRowHeight          = 19 | Type::Int,
    SystemIconTheme           = 20 | Type::String,
    DirBrowserPath            = 21 | Type::String,
    DirBrowserIcons           = 22 | Type::Bool,
    DirBrowserDoubleClick     = 23 | Type::Int,
    DirBrowserMiddleClick     = 24 | Type::Int,
    DirBrowserMode            = 25 | Type::Int,
    DirBrowserListIndent      = 26 | Type::Bool,
    DirBrowserControls        = 27 | Type::Bool,
    DirBrowserLocation        = 28 | Type::Bool,
    WindowTitleTrackScript    = 29 | Type::String,
    TrackCoverPaths           = 30 | Type::Variant,
    TrackCoverDisplayOption   = 31 | Type::Int,
    PlaylistImagePadding      = 32 | Type::Int,
    PlaylistImagePaddingTop   = 33 | Type::Int,
    PixmapCacheSize           = 34 | Type::Int,
    LibTreeSendPlayback       = 35 | Type::Bool,
    DirBrowserSendPlayback    = 36 | Type::Bool,
    EditableLayoutMargin      = 37 | Type::Int,
    PlaylistTabsAddButton     = 38 | Type::Bool,
    LibTreeRestoreState       = 39 | Type::Bool,
    ShowTrayIcon              = 40 | Type::Bool,
    TrayOnClose               = 41 | Type::Bool,
    LibTreeKeepAlive          = 42 | Type::Bool,
    PlaylistTabsCloseButton   = 43 | Type::Bool,
    PlaylistTabsMiddleClose   = 44 | Type::Bool,
    PlaylistTabsExpand        = 45 | Type::Bool,
    LibTreeAnimated           = 46 | Type::Bool,
    PlaylistTabsClearButton   = 47 | Type::Bool,
    LibTreeHeader             = 48 | Type::Bool,
    QueueViewerShowIcon       = 49 | Type::Bool,
    QueueViewerIconSize       = 50 | Type::Variant,
    QueueViewerHeader         = 51 | Type::Bool,
    QueueViewerScrollBar      = 52 | Type::Bool,
    QueueViewerAltColours     = 53 | Type::Bool,
    QueueViewerLeftScript     = 54 | Type::String,
    QueueViewerRightScript    = 55 | Type::String,
    PlaylistMiddleClick       = 56 | Type::Int,
    InfoDisplayPrefer         = 57 | Type::Int,
    SystemStyle               = 58 | Type::String,
    SystemFont                = 59 | Type::Variant,
    SystemPalette             = 60 | Type::Variant,
    DirBrowserShowHorizScroll = 61 | Type::Bool,
    LibTreeIconSize           = 62 | Type::Variant,
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
