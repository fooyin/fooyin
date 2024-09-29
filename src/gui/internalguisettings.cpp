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
#include <QStyle>

constexpr int PixmapCacheSize = 32;

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
    qRegisterMetaType<FyTheme>("FyTheme");

    m_settings->createTempSetting<LayoutEditing>(false);
    m_settings->createSetting<StartupBehaviour>(3, QStringLiteral("Interface/StartupBehaviour"));
    m_settings->createSetting<WaitForTracks>(true, QStringLiteral("Interface/WaitForTracks"));
    m_settings->createSetting<IconTheme>(0, QStringLiteral("Theme/IconTheme"));
    m_settings->createSetting<LastPlaylistId>(0, QStringLiteral("Playlist/LastPlaylistId"));
    m_settings->createSetting<CursorFollowsPlayback>(false, QStringLiteral("Playlist/CursorFollowsPlayback"));
    m_settings->createSetting<PlaybackFollowsCursor>(false, QStringLiteral("Playlist/PlaybackFollowsCursor"));
    m_settings->createSetting<ToolButtonStyle>(0, QStringLiteral("Interface/ToolButtonStyle"));
    m_settings->createSetting<StarRatingSize>(17, QStringLiteral("Interface/StarRatingSize"));
    m_settings->createSetting<Style>(QString{}, QStringLiteral("Interface/Style"));
    m_settings->createTempSetting<MainWindowPixelRatio>(1.0);
    m_settings->createSetting<SeekStep>(4000, QStringLiteral("Interface/SeekIncrement"));
    m_settings->createSetting<ShowStatusTips>(true, QStringLiteral("Interface/ShowStatusTips"));
    m_settings->createTempSetting<Theme>(QVariant{});
    m_settings->createSetting<ShowSplitterHandles>(false, QStringLiteral("Interface/SplitterHandles"));
    m_settings->createSetting<LockSplitterHandles>(false, QStringLiteral("Interface/LockSplitterHandles"));
    m_settings->createSetting<SplitterHandleSize>(-1, QStringLiteral("Interface/SplitterHandleSize"));
    m_settings->createSetting<VolumeStep>(0.05, QStringLiteral("Controls/VolumeStep"));

    m_settings->createSetting<Internal::EditingMenuLevels>(2, QStringLiteral("Interface/EditingMenuLevels"));
    m_settings->createSetting<Internal::PlaylistAltColours>(true, QStringLiteral("PlaylistWidget/AlternatingColours"));
    m_settings->createSetting<Internal::PlaylistHeader>(true, QStringLiteral("PlaylistWidget/Header"));
    m_settings->createSetting<Internal::PlaylistScrollBar>(true, QStringLiteral("PlaylistWidget/Scrollbar"));
    m_settings->createSetting<Internal::InfoAltColours>(true, QStringLiteral("InfoPanel/AlternatingColours"));
    m_settings->createSetting<Internal::InfoHeader>(true, QStringLiteral("InfoPanel/Header"));
    m_settings->createSetting<Internal::InfoScrollBar>(true, QStringLiteral("InfoPanel/Scrollbar"));
    m_settings->createSetting<Internal::StatusShowIcon>(false, QStringLiteral("StatusWidget/ShowIcon"));
    m_settings->createSetting<Internal::StatusShowSelection>(false, QStringLiteral("StatusWidget/ShowSelection"));
    m_settings->createSetting<Internal::StatusPlayingScript>(
        QStringLiteral(
            "[%codec% | ][%bitrate% kbps | ][%samplerate% Hz | ][%channels% | ]%playback_time%[ / %duration%]"),
        QStringLiteral("StatusWidget/PlayingScript"));
    m_settings->createSetting<Internal::StatusSelectionScript>(
        QStringLiteral("%trackcount% $ifequal(%trackcount%,1,Track,Tracks) | %playtime%"),
        QStringLiteral("StatusWidget/SelectionScript"));

    m_settings->createSetting<Internal::LibTreeDoubleClick>(0, QStringLiteral("LibraryTree/DoubleClickBehaviour"));
    m_settings->createSetting<Internal::LibTreeMiddleClick>(0, QStringLiteral("LibraryTree/MiddleClickBehaviour"));
    m_settings->createSetting<Internal::LibTreePlaylistEnabled>(false,
                                                                QStringLiteral("LibraryTree/SelectionPlaylistEnabled"));
    m_settings->createSetting<Internal::LibTreeAutoSwitch>(true,
                                                           QStringLiteral("LibraryTree/SelectionPlaylistAutoSwitch"));
    m_settings->createSetting<Internal::LibTreeAutoPlaylist>(QStringLiteral("Library Selection"),
                                                             QStringLiteral("LibraryTree/SelectionPlaylistName"));
    m_settings->createSetting<Internal::LibTreeScrollBar>(true, QStringLiteral("LibraryTree/Scrollbar"));
    m_settings->createSetting<Internal::LibTreeAltColours>(false, QStringLiteral("LibraryTree/AlternatingColours"));
    m_settings->createSetting<Internal::LibTreeRowHeight>(0, QStringLiteral("LibraryTree/RowHeight"));
    m_settings->createTempSetting<Internal::SystemIconTheme>(QIcon::themeName());
    m_settings->createSetting<Internal::DirBrowserPath>(QString{}, QStringLiteral("DirectoryBrowser/Path"));
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
        QStringLiteral("[%albumartist% - ]%title% \"[fooyin]\""), QStringLiteral("Interface/WindowTitleTrackScript"));
    m_settings->createSetting<Internal::TrackCoverPaths>(QVariant::fromValue(defaultCoverPaths()),
                                                         QStringLiteral("Artwork/Paths"));
    m_settings->createSetting<Internal::TrackCoverDisplayOption>(0, QStringLiteral("Artwork/DisplayOption"));
    m_settings->createSetting<Internal::PlaylistImagePadding>(5, QStringLiteral("PlaylistWidget/ImagePadding"));
    m_settings->createSetting<Internal::PlaylistImagePaddingTop>(0, QStringLiteral("PlaylistWidget/ImagePaddingTop"));
    m_settings->createSetting<Internal::PixmapCacheSize>(
        static_cast<int>(PixmapCacheSize * std::pow(qApp->devicePixelRatio(), 2)),
        QStringLiteral("Interface/PixmapCacheSize"));
    m_settings->createSetting<Internal::LibTreeSendPlayback>(true, QStringLiteral("LibraryTree/StartPlaybackOnSend"));
    m_settings->createSetting<Internal::DirBrowserSendPlayback>(true,
                                                                QStringLiteral("DirectoryBrowser/StartPlaybackOnSend"));
    m_settings->createSetting<Internal::EditableLayoutMargin>(-1, QStringLiteral("Interface/EditableLayoutMargin"));
    m_settings->createSetting<Internal::PlaylistTabsAddButton>(false, QStringLiteral("PlaylistTabs/ShowAddButton"));
    m_settings->createSetting<Internal::LibTreeRestoreState>(true, QStringLiteral("LibraryTree/RestoreState"));
    m_settings->createSetting<Internal::ShowTrayIcon>(false, QStringLiteral("Interface/ShowTrayIcon"));
    m_settings->createSetting<Internal::TrayOnClose>(true, QStringLiteral("Interface/TrayOnClose"));
    m_settings->createSetting<Internal::LibTreeKeepAlive>(false, QStringLiteral("LibraryTree/KeepAlive"));
    m_settings->createSetting<Internal::PlaylistTabsCloseButton>(false, QStringLiteral("PlaylistTabs/ShowCloseButton"));
    m_settings->createSetting<Internal::PlaylistTabsMiddleClose>(false,
                                                                 QStringLiteral("PlaylistTabs/CloseOnMiddleClick"));
    m_settings->createSetting<Internal::PlaylistTabsExpand>(false, QStringLiteral("PlaylistTabs/ExpandToFill"));
    m_settings->createSetting<Internal::LibTreeAnimated>(true, QStringLiteral("LibraryTree/Animated"));
    m_settings->createSetting<Internal::PlaylistTabsClearButton>(false, QStringLiteral("PlaylistTabs/ShowClearButton"));
    m_settings->createSetting<Internal::LibTreeHeader>(true, QStringLiteral("LibraryTree/Header"));
    m_settings->createSetting<Internal::QueueViewerShowIcon>(true, QStringLiteral("PlaybackQueue/ShowIcon"));
    m_settings->createSetting<Internal::QueueViewerIconSize>(QSize{36, 36}, QStringLiteral("PlaybackQueue/IconSize"));
    m_settings->createSetting<Internal::QueueViewerHeader>(true, QStringLiteral("PlaybackQueue/Header"));
    m_settings->createSetting<Internal::QueueViewerScrollBar>(true, QStringLiteral("PlaybackQueue/Scrollbar"));
    m_settings->createSetting<Internal::QueueViewerAltColours>(false,
                                                               QStringLiteral("PlaybackQueue/AlternatingColours"));
    m_settings->createSetting<Internal::QueueViewerLeftScript>(QStringLiteral("%title%$crlf()%album%"),
                                                               QStringLiteral("PlaybackQueue/LeftScript"));
    m_settings->createSetting<Internal::QueueViewerRightScript>(QStringLiteral("%duration%"),
                                                                QStringLiteral("PlaybackQueue/RightScript"));
    m_settings->createSetting<Internal::PlaylistMiddleClick>(0, QStringLiteral("PlaylistWidget/MiddleClickBehaviour"));
    m_settings->createSetting<Internal::InfoDisplayPrefer>(0, QStringLiteral("SelectionInfo/PreferDisplay"));
    m_settings->createTempSetting<Internal::SystemStyle>(QApplication::style()->name());
    m_settings->createTempSetting<Internal::SystemFont>(QApplication::font());
    m_settings->createTempSetting<Internal::SystemPalette>(QApplication::palette());
    m_settings->createSetting<Internal::DirBrowserElide>(true, QStringLiteral("DirectoryBrowser/ElideText"));
}
} // namespace Fooyin
