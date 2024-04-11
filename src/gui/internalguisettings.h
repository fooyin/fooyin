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
    PlaylistThumbnailSize   = 7 | Type::Int,
    SeekBarElapsedTotal     = 8 | Type::Bool,
    InfoAltColours          = 9 | Type::Bool,
    InfoHeader              = 10 | Type::Bool,
    InfoScrollBar           = 11 | Type::Bool,
    StatusPlayingScript     = 12 | Type::String,
    StatusSelectionScript   = 13 | Type::String,
    StatusShowIcon          = 14 | Type::Bool,
    StatusShowSelection     = 15 | Type::Bool,
    LibTreeDoubleClick      = 16 | Type::Int,
    LibTreeMiddleClick      = 17 | Type::Int,
    LibTreePlaylistEnabled  = 18 | Type::Bool,
    LibTreeAutoSwitch       = 19 | Type::Bool,
    LibTreeAutoPlaylist     = 20 | Type::String,
    LibTreeScrollBar        = 21 | Type::Bool,
    LibTreeAltColours       = 22 | Type::Bool,
    LibTreeFont             = 23 | Type::String,
    LibTreeColour           = 24 | Type::String,
    LibTreeRowHeight        = 25 | Type::Int,
    LibTreeAppearance       = 26 | Type::Variant,
    SystemIconTheme         = 27 | Type::String,
    SeekBarLabels           = 28 | Type::Bool,
    DirBrowserPath          = 29 | Type::String,
    DirBrowserIcons         = 30 | Type::Bool,
    DirBrowserDoubleClick   = 31 | Type::Int,
    DirBrowserMiddleClick   = 32 | Type::Int,
    DirBrowserMode          = 33 | Type::Int,
    DirBrowserListIndent    = 34 | Type::Bool,
    DirBrowserControls      = 35 | Type::Bool,
    DirBrowserLocation      = 36 | Type::Bool,
    WindowTitleTrackScript  = 37 | Type::String,
    TrackCoverPaths         = 38 | Settings::Variant,
    TrackCoverDisplayOption = 39 | Settings::Int,
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
