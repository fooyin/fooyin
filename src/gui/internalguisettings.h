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
    LibTreeFont             = 20 | Type::String,
    LibTreeColour           = 21 | Type::String,
    LibTreeRowHeight        = 22 | Type::Int,
    LibTreeAppearance       = 23 | Type::Variant,
    SystemIconTheme         = 24 | Type::String,
    DirBrowserPath          = 25 | Type::String,
    DirBrowserIcons         = 26 | Type::Bool,
    DirBrowserDoubleClick   = 27 | Type::Int,
    DirBrowserMiddleClick   = 28 | Type::Int,
    DirBrowserMode          = 29 | Type::Int,
    DirBrowserListIndent    = 30 | Type::Bool,
    DirBrowserControls      = 31 | Type::Bool,
    DirBrowserLocation      = 32 | Type::Bool,
    WindowTitleTrackScript  = 33 | Type::String,
    TrackCoverPaths         = 34 | Type::Variant,
    TrackCoverDisplayOption = 35 | Type::Int,
    PlaylistImagePadding    = 36 | Type::Int,
    PlaylistImagePaddingTop = 37 | Type::Int,
    PixmapCacheSize         = 38 | Type::Int,
    LibTreeSendPlayback     = 39 | Type::Bool,
    DirBrowserSendPlayback  = 40 | Type::Bool,
    EditableLayoutMargin    = 41 | Type::Int,
    PlaylistTabsAddButton   = 42 | Type::Bool,
    SplitterHandleSize      = 43 | Type::Int,
    LibTreeRestoreState     = 44 | Type::Bool,
    ShowTrayIcon            = 45 | Type::Bool,
    TrayOnClose             = 46 | Type::Bool,
    LibTreeKeepAlive        = 47 | Type::Bool,
    PlaylistTabsCloseButton = 48 | Type::Bool,
    PlaylistTabsMiddleClose = 49 | Type::Bool,
    PlaylistTabsExpand      = 50 | Type::Bool,
    LibTreeAnimated         = 51 | Type::Bool,
    PlaylistTabsClearButton = 52 | Type::Bool,
    LibTreeHeader           = 53 | Type::Bool,
    QueueViewerShowIcon     = 54 | Type::Bool,
    QueueViewerIconSize     = 55 | Type::Variant,
    QueueViewerHeader       = 56 | Type::Bool,
    QueueViewerScrollBar    = 57 | Type::Bool,
    QueueViewerAltColours   = 58 | Type::Bool,
    QueueViewerLeftScript   = 59 | Type::String,
    QueueViewerRightScript  = 60 | Type::String,
    PlaylistMiddleClick     = 61 | Type::Int,
    InfoDisplayPrefer       = 62 | Type::Int,
    SystemStyle             = 63 | Type::String,
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
