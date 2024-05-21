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
    SeekBarElapsedTotal     = 7 | Type::Bool,
    InfoAltColours          = 8 | Type::Bool,
    InfoHeader              = 9 | Type::Bool,
    InfoScrollBar           = 10 | Type::Bool,
    StatusPlayingScript     = 11 | Type::String,
    StatusSelectionScript   = 12 | Type::String,
    StatusShowIcon          = 13 | Type::Bool,
    StatusShowSelection     = 14 | Type::Bool,
    LibTreeDoubleClick      = 15 | Type::Int,
    LibTreeMiddleClick      = 16 | Type::Int,
    LibTreePlaylistEnabled  = 17 | Type::Bool,
    LibTreeAutoSwitch       = 18 | Type::Bool,
    LibTreeAutoPlaylist     = 19 | Type::String,
    LibTreeScrollBar        = 20 | Type::Bool,
    LibTreeAltColours       = 21 | Type::Bool,
    LibTreeFont             = 22 | Type::String,
    LibTreeColour           = 23 | Type::String,
    LibTreeRowHeight        = 24 | Type::Int,
    LibTreeAppearance       = 25 | Type::Variant,
    SystemIconTheme         = 26 | Type::String,
    SeekBarLabels           = 27 | Type::Bool,
    DirBrowserPath          = 28 | Type::String,
    DirBrowserIcons         = 29 | Type::Bool,
    DirBrowserDoubleClick   = 30 | Type::Int,
    DirBrowserMiddleClick   = 31 | Type::Int,
    DirBrowserMode          = 32 | Type::Int,
    DirBrowserListIndent    = 33 | Type::Bool,
    DirBrowserControls      = 34 | Type::Bool,
    DirBrowserLocation      = 35 | Type::Bool,
    WindowTitleTrackScript  = 36 | Type::String,
    TrackCoverPaths         = 37 | Type::Variant,
    TrackCoverDisplayOption = 38 | Type::Int,
    PlaylistImagePadding    = 39 | Type::Int,
    PlaylistImagePaddingTop = 40 | Type::Int,
    PixmapCacheSize         = 41 | Type::Int,
    ArtworkThumbnailSize    = 42 | Type::Int,
    LibTreeSendPlayback     = 43 | Type::Bool,
    DirBrowserSendPlayback  = 44 | Type::Bool,
    EditableLayoutMargin    = 45 | Type::Int,
    PlaylistTabsAddButton   = 46 | Type::Bool,
    SplitterHandleSize      = 47 | Type::Int
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
