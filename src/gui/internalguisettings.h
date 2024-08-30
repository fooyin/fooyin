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
    EditingMenuLevels       = 1 | Type::Int,
    SplitterHandles         = 2 | Type::Bool,
    PlaylistAltColours      = 3 | Type::Bool,
    PlaylistHeader          = 4 | Type::Bool,
    PlaylistScrollBar       = 5 | Type::Bool,
    InfoAltColours          = 6 | Type::Bool,
    InfoHeader              = 7 | Type::Bool,
    InfoScrollBar           = 8 | Type::Bool,
    StatusPlayingScript     = 9 | Type::String,
    StatusSelectionScript   = 10 | Type::String,
    StatusShowIcon          = 11 | Type::Bool,
    StatusShowSelection     = 12 | Type::Bool,
    LibTreeDoubleClick      = 13 | Type::Int,
    LibTreeMiddleClick      = 14 | Type::Int,
    LibTreePlaylistEnabled  = 15 | Type::Bool,
    LibTreeAutoSwitch       = 16 | Type::Bool,
    LibTreeAutoPlaylist     = 17 | Type::String,
    LibTreeScrollBar        = 18 | Type::Bool,
    LibTreeAltColours       = 19 | Type::Bool,
    LibTreeRowHeight        = 20 | Type::Int,
    SystemIconTheme         = 21 | Type::String,
    DirBrowserPath          = 22 | Type::String,
    DirBrowserIcons         = 23 | Type::Bool,
    DirBrowserDoubleClick   = 24 | Type::Int,
    DirBrowserMiddleClick   = 25 | Type::Int,
    DirBrowserMode          = 26 | Type::Int,
    DirBrowserListIndent    = 27 | Type::Bool,
    DirBrowserControls      = 28 | Type::Bool,
    DirBrowserLocation      = 29 | Type::Bool,
    WindowTitleTrackScript  = 30 | Type::String,
    TrackCoverPaths         = 31 | Type::Variant,
    TrackCoverDisplayOption = 32 | Type::Int,
    PlaylistImagePadding    = 33 | Type::Int,
    PlaylistImagePaddingTop = 34 | Type::Int,
    PixmapCacheSize         = 35 | Type::Int,
    LibTreeSendPlayback     = 36 | Type::Bool,
    DirBrowserSendPlayback  = 37 | Type::Bool,
    EditableLayoutMargin    = 38 | Type::Int,
    PlaylistTabsAddButton   = 39 | Type::Bool,
    SplitterHandleSize      = 40 | Type::Int,
    LibTreeRestoreState     = 41 | Type::Bool,
    ShowTrayIcon            = 42 | Type::Bool,
    TrayOnClose             = 43 | Type::Bool,
    LibTreeKeepAlive        = 44 | Type::Bool,
    PlaylistTabsCloseButton = 45 | Type::Bool,
    PlaylistTabsMiddleClose = 46 | Type::Bool,
    PlaylistTabsExpand      = 47 | Type::Bool,
    LibTreeAnimated         = 48 | Type::Bool,
    PlaylistTabsClearButton = 49 | Type::Bool,
    LibTreeHeader           = 50 | Type::Bool,
    QueueViewerShowIcon     = 51 | Type::Bool,
    QueueViewerIconSize     = 52 | Type::Variant,
    QueueViewerHeader       = 53 | Type::Bool,
    QueueViewerScrollBar    = 54 | Type::Bool,
    QueueViewerAltColours   = 55 | Type::Bool,
    QueueViewerLeftScript   = 56 | Type::String,
    QueueViewerRightScript  = 57 | Type::String,
    PlaylistMiddleClick     = 58 | Type::Int,
    InfoDisplayPrefer       = 59 | Type::Int,
    SystemStyle             = 60 | Type::String,
    SystemFont              = 61 | Type::Variant,
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
