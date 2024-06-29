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

enum class CoverDisplay : uint8_t
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

enum GuiInternalSettings : uint32_t
{
    EditingMenuLevels       = 1 | Type::Int,
    SplitterHandles         = 2 | Type::Bool,
    PlaylistAltColours      = 3 | Type::Bool,
    PlaylistHeader          = 4 | Type::Bool,
    PlaylistScrollBar       = 5 | Type::Bool,
    PlaylistCurrentPreset   = 6 | Type::Int,
    InfoAltColours          = 7 | Type::Bool,
    InfoHeader              = 8 | Type::Bool,
    InfoScrollBar           = 9 | Type::Bool,
    StatusPlayingScript     = 10 | Type::String,
    StatusSelectionScript   = 11 | Type::String,
    StatusShowIcon          = 12 | Type::Bool,
    StatusShowSelection     = 13 | Type::Bool,
    LibTreeDoubleClick      = 14 | Type::Int,
    LibTreeMiddleClick      = 15 | Type::Int,
    LibTreePlaylistEnabled  = 16 | Type::Bool,
    LibTreeAutoSwitch       = 17 | Type::Bool,
    LibTreeAutoPlaylist     = 18 | Type::String,
    LibTreeScrollBar        = 19 | Type::Bool,
    LibTreeAltColours       = 20 | Type::Bool,
    LibTreeFont             = 21 | Type::String,
    LibTreeColour           = 22 | Type::String,
    LibTreeRowHeight        = 23 | Type::Int,
    LibTreeAppearance       = 24 | Type::Variant,
    SystemIconTheme         = 25 | Type::String,
    DirBrowserPath          = 26 | Type::String,
    DirBrowserIcons         = 27 | Type::Bool,
    DirBrowserDoubleClick   = 28 | Type::Int,
    DirBrowserMiddleClick   = 29 | Type::Int,
    DirBrowserMode          = 30 | Type::Int,
    DirBrowserListIndent    = 31 | Type::Bool,
    DirBrowserControls      = 32 | Type::Bool,
    DirBrowserLocation      = 33 | Type::Bool,
    WindowTitleTrackScript  = 34 | Type::String,
    TrackCoverPaths         = 35 | Type::Variant,
    TrackCoverDisplayOption = 36 | Type::Int,
    PlaylistImagePadding    = 37 | Type::Int,
    PlaylistImagePaddingTop = 38 | Type::Int,
    PixmapCacheSize         = 39 | Type::Int,
    ArtworkThumbnailSize    = 40 | Type::Int,
    LibTreeSendPlayback     = 41 | Type::Bool,
    DirBrowserSendPlayback  = 42 | Type::Bool,
    EditableLayoutMargin    = 43 | Type::Int,
    PlaylistTabsAddButton   = 44 | Type::Bool,
    SplitterHandleSize      = 45 | Type::Int,
    LibTreeRestoreState     = 46 | Type::Bool,
    ShowTrayIcon            = 47 | Type::Bool,
    TrayOnClose             = 48 | Type::Bool,
    LibTreeKeepAlive        = 49 | Type::Bool,
    PlaylistTabsCloseButton = 50 | Type::Bool,
    PlaylistTabsMiddleClose = 51 | Type::Bool,
    PlaylistTabsExpand      = 52 | Type::Bool,
    LibTreeAnimated         = 53 | Type::Bool
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
