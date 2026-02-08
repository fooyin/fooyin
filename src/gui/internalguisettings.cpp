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

#include "librarytree/librarytreecontroller.h"
#include "search/searchwidget.h"
#include "widgets/statuswidget.h"

#include <gui/guisettings.h>
#include <utils/settings/settingsmanager.h>

#include <QApplication>
#include <QIcon>
#include <QImageReader>
#include <QPalette>
#include <QStyle>

using namespace Qt::StringLiterals;

constexpr int PixmapCacheSize = 64;

namespace
{
    Fooyin::CoverPaths defaultCoverPaths()
    {
        Fooyin::CoverPaths paths;

        paths.frontCoverPaths
            = {u"%path%/folder.*"_s, u"%path%/cover.*"_s, u"%path%/front.*"_s, u"%path%/../Artwork/folder.*"_s};
        paths.backCoverPaths = {u"%path%/back.*"_s};
        paths.artistPaths = {u"%path%/artist.*"_s, u"%path%/%albumartist%.*"_s};

        return paths;
    }

    Fooyin::ArtworkSaveMethods defaultArtworkSaveMethods()
    {
        Fooyin::ArtworkSaveMethods methods;

        methods[Fooyin::Track::Cover::Front] = {Fooyin::ArtworkSaveMethod::Embedded, u"%path%"_s, u"cover"_s};
        methods[Fooyin::Track::Cover::Back] = {Fooyin::ArtworkSaveMethod::Embedded, u"%path%"_s, u"back"_s};
        methods[Fooyin::Track::Cover::Artist] = {Fooyin::ArtworkSaveMethod::Embedded, u"%path%"_s, u"artist"_s};

        return methods;
    }
} // namespace

namespace Fooyin
{
    GuiSettings::GuiSettings(SettingsManager* settingsManager)
        : m_settings{settingsManager}
    {
        using namespace Settings::Gui;

        qRegisterMetaType<CoverPaths>("CoverPaths");
        qRegisterMetaType<FyTheme>("FyTheme");
        qRegisterMetaType<ArtworkSaveMethods>("ArtworkSaveMethods");

        m_settings->createTempSetting<LayoutEditing>(false);
        m_settings->createSetting<StartupBehaviour>(3, u"Interface/StartupBehaviour"_s);
        m_settings->createSetting<WaitForTracks>(true, u"Interface/WaitForTracks"_s);
        m_settings->createSetting<IconTheme>(0, u"Theme/IconTheme"_s);
        m_settings->createSetting<CursorFollowsPlayback>(false, u"Playlist/CursorFollowsPlayback"_s);
        m_settings->createSetting<PlaybackFollowsCursor>(false, u"Playlist/PlaybackFollowsCursor"_s);
        m_settings->createSetting<ToolButtonStyle>(0, u"Interface/ToolButtonStyle"_s);
        m_settings->createSetting<StarRatingSize>(17, u"Interface/StarRatingSize"_s);
        m_settings->createSetting<Style>(QString{}, u"Interface/Style"_s);
        m_settings->createTempSetting<MainWindowPixelRatio>(1.0);
        m_settings->createSetting<SeekStepSmall>(4000, u"Interface/SeekIncrement"_s);
        m_settings->createSetting<SeekStepLarge>(30000, u"Interface/SeekIncrementLarge"_s);
        m_settings->createSetting<ShowStatusTips>(true, u"Interface/ShowStatusTips"_s);
        m_settings->createTempSetting<Theme>(QVariant{});
        m_settings->createSetting<ShowSplitterHandles>(false, u"Interface/SplitterHandles"_s);
        m_settings->createSetting<LockSplitterHandles>(false, u"Interface/LockSplitterHandles"_s);
        m_settings->createSetting<SplitterHandleSize>(-1, u"Interface/SplitterHandleSize"_s);
        m_settings->createSetting<VolumeStep>(0.05, u"Controls/VolumeStep"_s);
        m_settings->createSetting<SearchSuccessClear>(true, u"Searching/ClearOnSuccess"_s);
        m_settings->createSetting<SearchAutoDelay>(1, u"Searching/AutoDelay"_s);
        m_settings->createSetting<SearchPlaylistName>(SearchWidget::defaultPlaylistName(), u"Searching/PlaylistName"_s);
        m_settings->createSetting<SearchPlaylistAppendSearch>(false, u"Searching/AppendSearchToPlaylist"_s);
        m_settings->createSetting<SearchSuccessFocus>(true, u"Searching/FocusOnSuccess"_s);
        m_settings->createSetting<SearchErrorBg>(QVariant{}, u"Searching/ErrorBgColour"_s);
        m_settings->createSetting<SearchErrorFg>(QVariant{}, u"Searching/ErrorFgColour"_s);
        m_settings->createSetting<SearchSuccessClose>(true, u"Searching/CloseOnSuccess"_s);
        m_settings->createSetting<ShowMenuBar>(true, u"Interface/ShowMenuBar"_s);

        m_settings->createSetting<Internal::EditingMenuLevels>(2, u"Interface/EditingMenuLevels"_s);
        m_settings->createSetting<Internal::PlaylistAltColours>(true, u"PlaylistWidget/AlternatingColours"_s);
        m_settings->createSetting<Internal::PlaylistHeader>(true, u"PlaylistWidget/Header"_s);
        m_settings->createSetting<Internal::PlaylistScrollBar>(true, u"PlaylistWidget/Scrollbar"_s);
        m_settings->createSetting<Internal::StatusShowIcon>(false, u"StatusWidget/ShowIcon"_s);
        m_settings->createSetting<Internal::StatusShowSelection>(false, u"StatusWidget/ShowSelection"_s);
        m_settings->createSetting<Internal::StatusPlayingScript>(StatusWidget::defaultPlayingScript(),
                                                                 u"StatusWidget/PlayingScript"_s);
        m_settings->createSetting<Internal::StatusSelectionScript>(StatusWidget::defaultSelectionScript(),
                                                                   u"StatusWidget/SelectionScript"_s);

        m_settings->createSetting<Internal::LibTreeDoubleClick>(0, u"LibraryTree/DoubleClickBehaviour"_s);
        m_settings->createSetting<Internal::LibTreeMiddleClick>(0, u"LibraryTree/MiddleClickBehaviour"_s);
        m_settings->createSetting<Internal::LibTreePlaylistEnabled>(false, u"LibraryTree/SelectionPlaylistEnabled"_s);
        m_settings->createSetting<Internal::LibTreeAutoSwitch>(true, u"LibraryTree/SelectionPlaylistAutoSwitch"_s);
        m_settings->createSetting<Internal::LibTreeAutoPlaylist>(LibraryTreeController::defaultPlaylistName(),
                                                                 u"LibraryTree/SelectionPlaylistName"_s);
        m_settings->createSetting<Internal::LibTreeScrollBar>(true, u"LibraryTree/Scrollbar"_s);
        m_settings->createSetting<Internal::LibTreeAltColours>(false, u"LibraryTree/AlternatingColours"_s);
        m_settings->createSetting<Internal::LibTreeRowHeight>(0, u"LibraryTree/RowHeight"_s);
        m_settings->createTempSetting<Internal::SystemIconTheme>(QIcon::themeName());
        m_settings->createSetting<Internal::DirBrowserPath>(QString{}, u"DirectoryBrowser/Path"_s);
        m_settings->createSetting<Internal::DirBrowserIcons>(true, u"DirectoryBrowser/Icons"_s);
        m_settings->createSetting<Internal::DirBrowserDoubleClick>(5, u"DirectoryBrowser/DoubleClickBehaviour"_s);
        m_settings->createSetting<Internal::DirBrowserMiddleClick>(0, u"DirectoryBrowser/MiddleClickBehaviour"_s);
        m_settings->createSetting<Internal::DirBrowserMode>(1, u"DirectoryBrowser/Mode"_s);
        m_settings->createSetting<Internal::DirBrowserListIndent>(true, u"DirectoryBrowser/IndentList"_s);
        m_settings->createSetting<Internal::DirBrowserControls>(true, u"DirectoryBrowser/Controls"_s);
        m_settings->createSetting<Internal::DirBrowserLocation>(true, u"DirectoryBrowser/LocationBar"_s);
        m_settings->createSetting<Internal::WindowTitleTrackScript>(u"[%albumartist% - ]%title% \"[fooyin]\""_s,
                                                                    u"Interface/WindowTitleTrackScript"_s);
        m_settings->createSetting<Internal::TrackCoverPaths>(QVariant::fromValue(defaultCoverPaths()),
                                                             u"Artwork/Paths"_s);
        m_settings->createSetting<Internal::TrackCoverDisplayOption>(0, u"Artwork/DisplayOption"_s);
        m_settings->createSetting<Internal::PlaylistImagePadding>(5, u"PlaylistWidget/ImagePadding"_s);
        m_settings->createSetting<Internal::PlaylistImagePaddingTop>(0, u"PlaylistWidget/ImagePaddingTop"_s);
        m_settings->createSetting<Internal::PixmapCacheSize>(
            static_cast<int>(PixmapCacheSize * std::pow(qApp->devicePixelRatio(), 2)), u"Interface/PixmapCacheSize"_s);
        m_settings->createSetting<Internal::LibTreeSendPlayback>(true, u"LibraryTree/StartPlaybackOnSend"_s);
        m_settings->createSetting<Internal::DirBrowserSendPlayback>(true, u"DirectoryBrowser/StartPlaybackOnSend"_s);
        m_settings->createSetting<Internal::EditableLayoutMargin>(-1, u"Interface/EditableLayoutMargin"_s);
        m_settings->createSetting<Internal::PlaylistTabsAddButton>(false, u"PlaylistTabs/ShowAddButton"_s);
        m_settings->createSetting<Internal::LibTreeRestoreState>(true, u"LibraryTree/RestoreState"_s);
        m_settings->createSetting<Internal::ShowTrayIcon>(false, u"Interface/ShowTrayIcon"_s);
        m_settings->createSetting<Internal::TrayOnClose>(true, u"Interface/TrayOnClose"_s);
        m_settings->createSetting<Internal::LibTreeKeepAlive>(false, u"LibraryTree/KeepAlive"_s);
        m_settings->createSetting<Internal::PlaylistTabsCloseButton>(false, u"PlaylistTabs/ShowCloseButton"_s);
        m_settings->createSetting<Internal::PlaylistTabsMiddleClose>(false, u"PlaylistTabs/CloseOnMiddleClick"_s);
        m_settings->createSetting<Internal::PlaylistTabsExpand>(false, u"PlaylistTabs/ExpandToFill"_s);
        m_settings->createSetting<Internal::LibTreeAnimated>(true, u"LibraryTree/Animated"_s);
        m_settings->createSetting<Internal::PlaylistTabsClearButton>(false, u"PlaylistTabs/ShowClearButton"_s);
        m_settings->createSetting<Internal::LibTreeHeader>(true, u"LibraryTree/Header"_s);
        m_settings->createSetting<Internal::QueueViewerShowIcon>(true, u"PlaybackQueue/ShowIcon"_s);
        m_settings->createSetting<Internal::QueueViewerIconSize>(QSize{36, 36}, u"PlaybackQueue/IconSize"_s);
        m_settings->createSetting<Internal::QueueViewerHeader>(true, u"PlaybackQueue/Header"_s);
        m_settings->createSetting<Internal::QueueViewerScrollBar>(true, u"PlaybackQueue/Scrollbar"_s);
        m_settings->createSetting<Internal::QueueViewerAltColours>(false, u"PlaybackQueue/AlternatingColours"_s);
        m_settings->createSetting<Internal::QueueViewerLeftScript>(u"%title%$crlf()%album%"_s,
                                                                   u"PlaybackQueue/LeftScript"_s);
        m_settings->createSetting<Internal::QueueViewerRightScript>(u"%duration%"_s, u"PlaybackQueue/RightScript"_s);
        m_settings->createSetting<Internal::QueueViewerShowCurrent>(true, u"PlaybackQueue/ShowCurrent"_s);
        m_settings->createSetting<Internal::PlaylistMiddleClick>(0, u"PlaylistWidget/MiddleClickBehaviour"_s);
        m_settings->createSetting<Internal::InfoDisplayPrefer>(0, u"SelectionInfo/PreferDisplay"_s);
        m_settings->createTempSetting<Internal::SystemStyle>(QApplication::style()->name());
        m_settings->createTempSetting<Internal::SystemFont>(QApplication::font());
        m_settings->createTempSetting<Internal::SystemPalette>(QApplication::palette());
        m_settings->createSetting<Internal::DirBrowserShowHorizScroll>(
            true, u"DirectoryBrowser/ShowHorizontalScrollbar"_s);
        m_settings->createSetting<Internal::LibTreeIconSize>(QSize{36, 36}, u"LibraryTree/IconSize"_s);
        m_settings->createSetting<Internal::ArtworkSaveMethods>(QVariant::fromValue(defaultArtworkSaveMethods()),
                                                                u"Artwork/SaveMethods"_s);
        m_settings->createSetting<Internal::ArtworkAutoSearch>(false, u"Artwork/AutoSearch"_s);
        m_settings->createSetting<Internal::ArtworkTitleField>(u"%title%"_s, u"Artwork/TitleField"_s);
        m_settings->createSetting<Internal::ArtworkAlbumField>(u"%album%"_s, u"Artwork/AlbumField"_s);
        m_settings->createSetting<Internal::ArtworkArtistField>(u"%albumartist%"_s, u"Artwork/ArtistField"_s);
        m_settings->createSetting<Internal::ArtworkMatchThreshold>(40, u"Artwork/MatchThreshold"_s);
        m_settings->createSetting<Internal::ArtworkDownloadThumbSize>(150, u"Artwork/DownloadThumbSize"_s);
        m_settings->createSetting<Internal::ImageAllocationLimit>(QImageReader::allocationLimit(),
                                                                  u"Interface/ImageAllocationLimit"_s);
        m_settings->createSetting<Internal::PlaylistTrackPreloadCount>(2000, u"Playlist/TrackPreloadCount"_s);
    }
} // namespace Fooyin