/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <luket@pm.me>
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

#include "nowplayingoutput/nowplayingoutputservice.h"
#include "search/searchwidget.h"
#include "widgets/statuswidget.h"

#include <core/ratingsymbols.h>
#include <gui/coverprovider.h>
#include <gui/guiconstants.h>
#include <gui/guisettings.h>
#include <utils/settings/settingsmanager.h>

#include <QApplication>
#include <QIcon>
#include <QImageReader>
#include <QPalette>
#include <QStyle>

using namespace Qt::StringLiterals;

constexpr int PixmapCacheSize = 128;

namespace {
Fooyin::CoverPaths defaultCoverPaths()
{
    Fooyin::CoverPaths paths;

    paths.frontCoverPaths.append(u"%path%/folder.*"_s);
    paths.frontCoverPaths.append(u"%path%/cover.*"_s);
    paths.frontCoverPaths.append(u"%path%/front.*"_s);
    paths.frontCoverPaths.append(u"%path%/../Artwork/folder.*"_s);

    paths.backCoverPaths.append(u"%path%/back.*"_s);

    paths.artistPaths.append(u"%path%/artist.*"_s);
    paths.artistPaths.append(u"%path%/%albumartist%.*"_s);

    return paths;
}

Fooyin::ArtworkSaveMethods defaultArtworkSaveMethods()
{
    Fooyin::ArtworkSaveMethods methods;

    methods[Fooyin::Track::Cover::Front]  = {.method   = Fooyin::ArtworkSaveMethod::Directory,
                                             .dir      = u"%path%"_s,
                                             .filename = u"cover"_s,
                                             .format   = {},
                                             .quality  = 90};
    methods[Fooyin::Track::Cover::Back]   = {.method   = Fooyin::ArtworkSaveMethod::Directory,
                                             .dir      = u"%path%"_s,
                                             .filename = u"back"_s,
                                             .format   = {},
                                             .quality  = 90};
    methods[Fooyin::Track::Cover::Artist] = {.method   = Fooyin::ArtworkSaveMethod::Directory,
                                             .dir      = u"%path%"_s,
                                             .filename = u"artist"_s,
                                             .format   = {},
                                             .quality  = 90};

    return methods;
}
} // namespace

namespace Fooyin {
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
    m_settings->createSetting<RatingFullStarSymbol>(defaultRatingFullStarSymbol(), u"Interface/RatingFullStarSymbol"_s);
    m_settings->createSetting<RatingHalfStarSymbol>(defaultRatingHalfStarSymbol(), u"Interface/RatingHalfStarSymbol"_s);
    m_settings->createSetting<RatingEmptyStarSymbol>(defaultRatingEmptyStarSymbol(),
                                                     u"Interface/RatingEmptyStarSymbol"_s);
    m_settings->createSetting<SeekBarMouseFocus>(false, u"Interface/SeekBarMouseFocus"_s);

    m_settings->createSetting<Internal::EditingMenuLevels>(2, u"Interface/EditingMenuLevels"_s);
    m_settings->createSetting<Internal::PlaylistAltColours>(true, u"PlaylistWidget/AlternatingColours"_s);
    m_settings->createSetting<Internal::PlaylistHeader>(true, u"PlaylistWidget/Header"_s);
    m_settings->createSetting<Internal::PlaylistScrollBar>(true, u"PlaylistWidget/Scrollbar"_s);
    m_settings->createSetting<Internal::StatusShowIcon>(false, u"StatusWidget/ShowIcon"_s);
    m_settings->createSetting<Internal::StatusShowSelection>(false, u"StatusWidget/ShowSelection"_s);
    m_settings->createSetting<Internal::StatusShowPlaylist>(false, u"StatusWidget/ShowPlaylist"_s);
    m_settings->createSetting<Internal::StatusPlayingScript>(StatusWidget::defaultPlayingScript(),
                                                             u"StatusWidget/PlayingScript"_s);
    m_settings->createSetting<Internal::StatusSelectionScript>(StatusWidget::defaultSelectionScript(),
                                                               u"StatusWidget/SelectionScript"_s);
    m_settings->createSetting<Internal::StatusPlaylistScript>(StatusWidget::defaultPlaylistScript(),
                                                              u"StatusWidget/PlaylistScript"_s);

    m_settings->createTempSetting<Internal::SystemIconTheme>(QIcon::themeName());
    m_settings->createSetting<Internal::WindowTitleTrackScript>(u"[%albumartist% - ]%title% \"[fooyin]\""_s,
                                                                u"Interface/WindowTitleTrackScript"_s);
    m_settings->createSetting<Internal::TrackCoverPaths>(QVariant::fromValue(defaultCoverPaths()), u"Artwork/Paths"_s);
    m_settings->createSetting<Internal::TrackCoverDisplayOption>(0, u"Artwork/DisplayOption"_s);
    m_settings->createSetting<Internal::TrackCoverSourcePreference>(
        static_cast<int>(ArtworkSourcePreference::PreferDirectory), u"Artwork/LocalSourcePreference"_s);
    m_settings->createSetting<Internal::TrackCoverThumbnailGroupScript>(
        u"[%date%|][%albumartist%|][%artist%|]$if2(%album%,%path%)"_s, u"Artwork/ThumbnailGroupScript"_s);
    m_settings->createSetting<Internal::PlaylistImagePadding>(5, u"PlaylistWidget/ImagePadding"_s);
    m_settings->createSetting<Internal::PlaylistImagePaddingTop>(0, u"PlaylistWidget/ImagePaddingTop"_s);
    m_settings->createSetting<Internal::PlaylistBackgroundImageMode>(static_cast<int>(PlaylistBgImage::None),
                                                                     u"PlaylistWidget/BackgroundImage"_s);
    m_settings->createSetting<Internal::PlaylistBackgroundCustomImage>(QString{},
                                                                       u"PlaylistWidget/BackgroundCustomImage"_s);
    m_settings->createSetting<Internal::PlaylistBackgroundScaling>(
        static_cast<int>(PlaylistBgScaling::ScaledAndCropped), u"PlaylistWidget/BackgroundScaling"_s);
    m_settings->createSetting<Internal::PlaylistBackgroundPosition>(static_cast<int>(PlaylistBgImagePosition::Middle),
                                                                    u"PlaylistWidget/BackgroundPosition"_s);
    m_settings->createSetting<Internal::PlaylistBackgroundMaxSize>(0, u"PlaylistWidget/BackgroundMaxSize"_s);
    m_settings->createSetting<Internal::PlaylistBackgroundBlur>(0, u"PlaylistWidget/BackgroundBlur"_s);
    m_settings->createSetting<Internal::PlaylistBackgroundOpacity>(40, u"PlaylistWidget/BackgroundOpacity"_s);
    m_settings->createSetting<Internal::PlaylistBackgroundFadeDuration>(0, u"PlaylistWidget/BackgroundFadeDuration"_s);
    m_settings->createSetting<Internal::PlaylistBackgroundCoverType>(static_cast<int>(Track::Cover::Front),
                                                                     u"PlaylistWidget/BackgroundCoverType"_s);
    m_settings->createSetting<Internal::PixmapCacheSize>(PixmapCacheSize, u"Interface/PixmapCacheSize"_s);
    m_settings->createSetting<Internal::EditableLayoutMargin>(-1, u"Interface/EditableLayoutMargin"_s);
    m_settings->createSetting<Internal::PlaylistTabsAddButton>(false, u"PlaylistTabs/ShowAddButton"_s);
    m_settings->createSetting<Internal::ShowTrayIcon>(false, u"Interface/ShowTrayIcon"_s);
    m_settings->createSetting<Internal::TrayOnClose>(true, u"Interface/TrayOnClose"_s);
    m_settings->createSetting<Internal::PlaylistTabsCloseButton>(false, u"PlaylistTabs/ShowCloseButton"_s);
    m_settings->createSetting<Internal::PlaylistTabsMiddleClose>(false, u"PlaylistTabs/CloseOnMiddleClick"_s);
    m_settings->createSetting<Internal::PlaylistTabsExpand>(false, u"PlaylistTabs/ExpandToFill"_s);
    m_settings->createSetting<Internal::PlaylistTabsClearButton>(false, u"PlaylistTabs/ShowClearButton"_s);
    m_settings->createSetting<Internal::PlaylistMiddleClick>(0, u"PlaylistWidget/MiddleClickBehaviour"_s);
    m_settings->createSetting<Internal::InfoDisplayPrefer>(0, u"SelectionInfo/PreferDisplay"_s);
    m_settings->createTempSetting<Internal::SystemStyle>(QApplication::style()->name());
    m_settings->createTempSetting<Internal::SystemFont>(QApplication::font());
    m_settings->createTempSetting<Internal::SystemPalette>(QApplication::palette());
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
    m_settings->createSetting<Internal::PlaylistInlineTagEditing>(false, u"PlaylistWidget/InlineTagEditing"_s);
    m_settings->createSetting<Internal::ContextMenuTrackDisabledSections>(
        QStringList{QString::fromLatin1(Constants::Actions::CopyLocation),
                    QString::fromLatin1(Constants::Actions::CopyDirectoryPath)},
        u"Interface/ContextMenuTrackDisabledSections"_s);
    m_settings->createSetting<Internal::ContextMenuPlaylistDisabledSections>(
        QStringList{QString::fromLatin1(Constants::Actions::AddToPlaylist)},
        u"Interface/ContextMenuPlaylistDisabledSections"_s);
    m_settings->createSetting<Internal::ContextMenuTrackLayout>(QStringList{}, u"Interface/ContextMenuTrackLayout"_s);
    m_settings->createSetting<Internal::ContextMenuPlaylistLayout>(QStringList{},
                                                                   u"Interface/ContextMenuPlaylistLayout"_s);
    m_settings->createSetting<Internal::ContextMenuLibraryTreeDisabledSections>(
        QStringList{QString::fromLatin1(Constants::Actions::AddToPlaylist)},
        u"Interface/ContextMenuLibraryTreeDisabledSections"_s);
    m_settings->createSetting<Internal::ContextMenuLibraryTreeLayout>(QStringList{},
                                                                      u"Interface/ContextMenuLibraryTreeLayout"_s);
    m_settings->createSetting<Internal::ContextMenuDirBrowserDisabledSections>(
        QStringList{QString::fromLatin1(Constants::Actions::AddToPlaylist)},
        u"Interface/ContextMenuDirBrowserDisabledSections"_s);
    m_settings->createSetting<Internal::ContextMenuDirBrowserLayout>(QStringList{},
                                                                     u"Interface/ContextMenuDirBrowserLayout"_s);
    m_settings->createSetting<Internal::ContextMenuLayoutEditingDisabledSections>(
        QStringList{}, u"Interface/ContextMenuLayoutEditingDisabledSections"_s);
    m_settings->createSetting<Internal::ContextMenuLayoutEditingLayout>(QStringList{},
                                                                        u"Interface/ContextMenuLayoutEditingLayout"_s);
    m_settings->createSetting<Internal::PropertiesSidebarTrackScript>(u"[%track%. ]%title%"_s,
                                                                      u"Interface/PropertiesSidebarTrackScript "_s);
    m_settings->createSetting<Internal::NowPlayingOutputEnabled>(false, u"NowPlayingOutput/Enabled"_s);
    m_settings->createSetting<Internal::NowPlayingOutputScript>(u"[%artist% - ]%title%"_s,
                                                                u"NowPlayingOutput/Script"_s);
    m_settings->createSetting<Internal::NowPlayingOutputUpdateEvents>(
        static_cast<int>(NowPlayingOutputService::DefaultEvents), u"NowPlayingOutput/UpdateEvents"_s);
    m_settings->createSetting<Internal::NowPlayingOutputTargets>(static_cast<int>(NowPlayingOutputService::NoOutput),
                                                                 u"NowPlayingOutput/Targets"_s);
    m_settings->createSetting<Internal::NowPlayingOutputFilePath>(u""_s, u"NowPlayingOutput/OutputFilePath"_s);
    m_settings->createSetting<Internal::NowPlayingOutputOptions>(
        static_cast<int>(NowPlayingOutputService::OutputDefaultOptions), u"NowPlayingOutput/Options"_s);
    m_settings->createSetting<Internal::NowPlayingOutputAppendLineLimit>(0, u"NowPlayingOutput/AppendLineLimit"_s);
    m_settings->createSetting<Internal::OutputDeviceRefreshMs>(0, u"Engine/OutputDeviceRefreshMs"_s);
}
} // namespace Fooyin
