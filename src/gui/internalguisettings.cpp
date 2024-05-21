/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <LukeT1@proton.me>
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

#include "internalguisettings.h"

#include <gui/guisettings.h>
#include <utils/settings/settingsmanager.h>

#include <QApplication>
#include <QIcon>
#include <QPalette>

namespace {
Fooyin::CoverPaths defaultCoverPaths()
{
    Fooyin::CoverPaths paths;

    paths.frontCoverPaths = {QStringLiteral("%path%/folder.*"), QStringLiteral("%path%/cover.*"),
                             QStringLiteral("%path%/front.*"), QStringLiteral("%path%/../Artwork/folder.*")};
    paths.backCoverPaths  = {QStringLiteral("%path%/back.*")};
    paths.artistPaths     = {QStringLiteral("%path%/artist.*"), QStringLiteral("%path%/%albumartist%.*")};

    return paths;
}
} // namespace

namespace Fooyin {
GuiSettings::GuiSettings(SettingsManager* settingsManager)
    : m_settings{settingsManager}
{
    using namespace Settings::Gui;

    qRegisterMetaType<CoverPaths>("CoverPaths");

    m_settings->createTempSetting<LayoutEditing>(false);
    m_settings->createSetting<StartupBehaviour>(2, QStringLiteral("Interface/StartupBehaviour"));
    m_settings->createSetting<WaitForTracks>(true, QStringLiteral("Interface/WaitForTracks"));
    m_settings->createSetting<IconTheme>(0, QStringLiteral("Theme/IconTheme"));
    m_settings->createSetting<LastPlaylistId>(0, QStringLiteral("Playlist/LastPlaylistId"));
    m_settings->createSetting<CursorFollowsPlayback>(false, QStringLiteral("Playlist/CursorFollowsPlayback"));
    m_settings->createSetting<PlaybackFollowsCursor>(false, QStringLiteral("Playlist/PlaybackFollowsCursor"));

    m_settings->createSetting<Internal::EditingMenuLevels>(2, QStringLiteral("Interface/EditingMenuLevels"));
    m_settings->createSetting<Internal::SplitterHandles>(true, QStringLiteral("Interface/SplitterHandles"));
    m_settings->createSetting<Internal::SeekBarElapsedTotal>(false, QStringLiteral("SeekBar/ElapsedTotal"));
    m_settings->createSetting<Internal::PlaylistAltColours>(true, QStringLiteral("PlaylistWidget/AlternatingColours"));
    m_settings->createSetting<Internal::PlaylistHeader>(true, QStringLiteral("PlaylistWidget/Header"));
    m_settings->createSetting<Internal::PlaylistScrollBar>(true, QStringLiteral("PlaylistWidget/Scrollbar"));
    m_settings->createSetting<Internal::PlaylistCurrentPreset>(0, QStringLiteral("PlaylistWidget/CurrentPreset"));
    m_settings->createSetting<Internal::InfoAltColours>(true, QStringLiteral("InfoPanel/AlternatingColours"));
    m_settings->createSetting<Internal::InfoHeader>(true, QStringLiteral("InfoPanel/Header"));
    m_settings->createSetting<Internal::InfoScrollBar>(true, QStringLiteral("InfoPanel/Scrollbar"));
    m_settings->createSetting<Internal::StatusShowIcon>(false, QStringLiteral("StatusWidget/ShowIcon"));
    m_settings->createSetting<Internal::StatusShowSelection>(false, QStringLiteral("StatusWidget/ShowSelection"));
    m_settings->createSetting<Internal::StatusPlayingScript>(
        QStringLiteral("[$num(%track%,2). ][%title% ($timems(%duration%))][ \u2022 %albumartist%][ \u2022 %album%]"),
        QStringLiteral("StatusWidget/PlayingScript"));
    m_settings->createSetting<Internal::StatusSelectionScript>(
        QStringLiteral("%trackcount% $ifequal(%trackcount%,1,Track,Tracks) | %playtime%"),
        QStringLiteral("StatusWidget/SelectionScript"));

    m_settings->createSetting<Internal::LibTreeDoubleClick>(5, QStringLiteral("LibraryTree/DoubleClickBehaviour"));
    m_settings->createSetting<Internal::LibTreeMiddleClick>(0, QStringLiteral("LibraryTree/MiddleClickkBehaviour"));
    m_settings->createSetting<Internal::LibTreePlaylistEnabled>(false,
                                                                QStringLiteral("LibraryTree/SelectionPlaylistEnabled"));
    m_settings->createSetting<Internal::LibTreeAutoSwitch>(true,
                                                           QStringLiteral("LibraryTree/SelectionPlaylistAutoSwitch"));
    m_settings->createSetting<Internal::LibTreeAutoPlaylist>(QStringLiteral("Library Selection"),
                                                             QStringLiteral("LibraryTree/SelectionPlaylistName"));
    m_settings->createSetting<Internal::LibTreeScrollBar>(true, QStringLiteral("LibraryTree/Scrollbar"));
    m_settings->createSetting<Internal::LibTreeAltColours>(false, QStringLiteral("LibraryTree/AlternatingColours"));
    m_settings->createSetting<Internal::LibTreeFont>(QStringLiteral(""), QStringLiteral("LibraryTree/Font"));
    m_settings->createSetting<Internal::LibTreeColour>(QApplication::palette().text().color().name(),
                                                       QStringLiteral("LibraryTree/Colour"));
    m_settings->createSetting<Internal::LibTreeRowHeight>(0, QStringLiteral("LibraryTree/RowHeight"));
    m_settings->createTempSetting<Internal::SystemIconTheme>(QIcon::themeName());
    m_settings->createSetting<Internal::SeekBarLabels>(true, QStringLiteral("SeekBar/Labels"));
    m_settings->createSetting<Internal::DirBrowserPath>(QStringLiteral(""), QStringLiteral("DirectoryBrowser/Path"));
    m_settings->createSetting<Internal::DirBrowserIcons>(true, QStringLiteral("DirectoryBrowser/Icons"));
    m_settings->createSetting<Internal::DirBrowserDoubleClick>(5,
                                                               QStringLiteral("DirectoryBrowser/DoubleClickBehaviour"));
    m_settings->createSetting<Internal::DirBrowserMiddleClick>(0,
                                                               QStringLiteral("DirectoryBrowser/MiddleClickBehaviour"));
    m_settings->createSetting<Internal::DirBrowserMode>(1, QStringLiteral("DirectoryBrowser/Mode"));
    m_settings->createSetting<Internal::DirBrowserListIndent>(true, QStringLiteral("DirectoryBrowser/IndentList"));
    m_settings->createSetting<Internal::DirBrowserControls>(true, QStringLiteral("DirectoryBrowser/Controls"));
    m_settings->createSetting<Internal::DirBrowserLocation>(true, QStringLiteral("DirectoryBrowser/LocationBar"));
    m_settings->createSetting<Internal::WindowTitleTrackScript>(
        QStringLiteral("[%albumartist% - ]$if2(%title%,%filepath%)"),
        QStringLiteral("Interface/WindowTitleTrackScript"));
    m_settings->createSetting<Internal::TrackCoverPaths>(QVariant::fromValue(defaultCoverPaths()),
                                                         QStringLiteral("Artwork/Paths"));
    m_settings->createSetting<Internal::TrackCoverDisplayOption>(0, QStringLiteral("Artwork/DisplayOption"));
    m_settings->createSetting<Internal::PlaylistImagePadding>(5, QStringLiteral("PlaylistWidget/ImagePadding"));
    m_settings->createSetting<Internal::PlaylistImagePaddingTop>(0, QStringLiteral("PlaylistWidget/ImagePaddingTop"));
    m_settings->createSetting<Internal::PixmapCacheSize>(20, QStringLiteral("Interface/PixmapCacheSize"));
    m_settings->createSetting<Internal::ArtworkThumbnailSize>(200, QStringLiteral("Interface/ArtworkThumbnailSize"));
    m_settings->createSetting<Internal::LibTreeSendPlayback>(true, QStringLiteral("LibraryTree/StartPlaybackOnSend"));
    m_settings->createSetting<Internal::DirBrowserSendPlayback>(true,
                                                                QStringLiteral("DirectoryBrowser/StartPlaybackOnSend"));
    m_settings->createSetting<Internal::EditableLayoutMargin>(-1, QStringLiteral("Interface/EditableLayoutMargin"));
    m_settings->createSetting<Internal::PlaylistTabsAddButton>(false, QStringLiteral("PlaylistTabs/ShowAddButton"));
}
} // namespace Fooyin
