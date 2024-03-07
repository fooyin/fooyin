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

#include "librarytree/librarytreeappearance.h"

#include <gui/guisettings.h>
#include <utils/settings/settingsmanager.h>

#include <QIcon>

namespace Fooyin {
GuiSettings::GuiSettings(SettingsManager* settingsManager)
    : m_settings{settingsManager}
{
    using namespace Settings::Gui;

    m_settings->createTempSetting<LayoutEditing>(false);
    m_settings->createSetting<StartupBehaviour>(2, QStringLiteral("Interface/StartupBehaviour"));
    m_settings->createSetting<WaitForTracks>(true, QStringLiteral("Interface/WaitForTracks"));
    m_settings->createSetting<IconTheme>(0, QStringLiteral("Theme/IconTheme"));
    m_settings->createSetting<LastPlaylistId>(0, QStringLiteral("Playlist/LastPlaylistId"));
    m_settings->createSetting<CursorFollowsPlayback>(false, QStringLiteral("Playlist/CursorFollowsPlayback"));
    m_settings->createSetting<PlaybackFollowsCursor>(false, QStringLiteral("Playlist/PlaybackFollowsCursor"));
    m_settings->createSetting<RememberPlaylistState>(true, QStringLiteral("Playlist/RememberPlaylistState"));

    m_settings->createSetting<Internal::EditingMenuLevels>(2, QStringLiteral("Interface/EditingMenuLevels"));
    m_settings->createSetting<Internal::SplitterHandles>(true, QStringLiteral("Interface/SplitterHandles"));
    m_settings->createSetting<Internal::SeekBarElapsedTotal>(false, QStringLiteral("SeekBar/ElapsedTotal"));
    m_settings->createSetting<Internal::PlaylistAltColours>(true, QStringLiteral("PlaylistWidget/AlternatingColours"));
    m_settings->createSetting<Internal::PlaylistHeader>(true, QStringLiteral("PlaylistWidget/Header"));
    m_settings->createSetting<Internal::PlaylistScrollBar>(true, QStringLiteral("PlaylistWidget/Scrollbar"));
    m_settings->createSetting<Internal::PlaylistCurrentPreset>(0, QStringLiteral("PlaylistWidget/CurrentPreset"));
    m_settings->createSetting<Internal::PlaylistThumbnailSize>(60, QStringLiteral("PlaylistWidget/ThumbnailSize"));
    m_settings->createSetting<Internal::PlaylistTabsHide>(false, QStringLiteral("PlaylistTabs/HideSingleTab"));
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

    qRegisterMetaType<Fooyin::LibraryTreeAppearance>("Fooyin::LibraryTreeAppearance");

    m_settings->createSetting<Internal::LibTreeDoubleClick>(1, QStringLiteral("LibraryTree/DoubleClickBehaviour"));
    m_settings->createSetting<Internal::LibTreeMiddleClick>(0, QStringLiteral("LibraryTree/MiddleClickkBehaviour"));
    m_settings->createSetting<Internal::LibTreePlaylistEnabled>(false,
                                                                QStringLiteral("LibraryTree/SelectionPlaylistEnabled"));
    m_settings->createSetting<Internal::LibTreeAutoSwitch>(true,
                                                           QStringLiteral("LibraryTree/SelectionPlaylistAutoSwitch"));
    m_settings->createSetting<Internal::LibTreeAutoPlaylist>(QStringLiteral("Library Selection"),
                                                             QStringLiteral("LibraryTree/SelectionPlaylistName"));
    m_settings->createSetting<Internal::LibTreeHeader>(true, QStringLiteral("LibraryTree/Header"));
    m_settings->createSetting<Internal::LibTreeScrollBar>(true, QStringLiteral("LibraryTree/Scrollbar"));
    m_settings->createSetting<Internal::LibTreeAltColours>(false, QStringLiteral("LibraryTree/AlternatingColours"));
    m_settings->createSetting<Internal::LibTreeAppearance>(QVariant::fromValue(LibraryTreeAppearance{}),
                                                           QStringLiteral("LibraryTree/Appearance"));
    m_settings->createTempSetting<Internal::SystemIconTheme>(QIcon::themeName());
    m_settings->createSetting<Internal::SeekBarLabels>(true, QStringLiteral("SeekBar/Labels"));
    m_settings->createSetting<Internal::DirBrowserPath>(QStringLiteral(""), QStringLiteral("DirectoryBrowser/Path"));
    m_settings->createSetting<Internal::DirBrowserIcons>(true, QStringLiteral("DirectoryBrowser/Icons"));
    m_settings->createSetting<Internal::DirBrowserDoubleClick>(1,
                                                               QStringLiteral("DirectoryBrowser/DoubleClickBehaviour"));
    m_settings->createSetting<Internal::DirBrowserMiddleClick>(
        0, QStringLiteral("DirectoryBrowser/MiddleClickkBehaviour"));
}
} // namespace Fooyin
