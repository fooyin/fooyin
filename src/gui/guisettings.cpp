/*
 * Fooyin
 * Copyright 2022-2023, Luke Taylor <LukeT1@proton.me>
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

#include <gui/guisettings.h>

#include "internalguisettings.h"
#include "librarytree/librarytreeappearance.h"
#include "librarytree/librarytreegroupregistry.h"
#include "playlist/presetregistry.h"

#include <utils/settings/settingsmanager.h>

using namespace Qt::Literals::StringLiterals;

namespace Fooyin {
GuiSettings::GuiSettings(SettingsManager* settingsManager)
    : m_settings{settingsManager}
    , m_playlistPresetRegistry{std::make_unique<PresetRegistry>(m_settings)}
{
    using namespace Settings::Gui;

    m_settings->createTempSetting<LayoutEditing>(false);
    m_settings->createSetting<StartupBehaviour>(2, u"Interface/StartupBehaviour"_s);
    m_settings->createSetting<WaitForTracks>(true, u"Interface/WaitForTracks"_s);
    m_settings->createSetting<IconTheme>("light", u"Theme/IconTheme"_s);
    m_settings->createSetting<LastPlaylistId>(0, u"Playlist/LastPlaylistId"_s);
    m_settings->createSetting<CursorFollowsPlayback>(false, u"Playlist/CursorFollowsPlayback"_s);
    m_settings->createSetting<PlaybackFollowsCursor>(false, u"Playlist/PlaybackFollowsCursor"_s);

    m_settings->createSetting<Internal::EditingMenuLevels>(2, u"Interface/EditingMenuLevels"_s);
    m_settings->createSetting<Internal::SplitterHandles>(true, u"Interface/SplitterHandles"_s);
    m_settings->createSetting<Internal::ElapsedTotal>(false, u"Player/ElapsedTotal"_s);
    m_settings->createSetting<Internal::PlaylistAltColours>(true, u"PlaylistWidget/AlternatingColours"_s);
    m_settings->createSetting<Internal::PlaylistHeader>(true, u"PlaylistWidget/Header"_s);
    m_settings->createSetting<Internal::PlaylistScrollBar>(true, u"PlaylistWidget/Scrollbar"_s);
    m_settings->createSetting<Internal::PlaylistCurrentPreset>("Default", u"PlaylistWidget/CurrentPreset"_s);
    m_settings->createSetting<Internal::PlaylistThumbnailSize>(100, u"PlaylistWidget/ThumbnailSize"_s);
    m_settings->createSetting<Internal::PlaylistTabsHide>(false, u"PlaylistTabs/HideSingleTab"_s);
    m_settings->createSetting<Internal::InfoAltColours>(true, u"InfoPanel/AlternatingColours"_s);
    m_settings->createSetting<Internal::InfoHeader>(true, u"InfoPanel/Header"_s);
    m_settings->createSetting<Internal::InfoScrollBar>(true, u"InfoPanel/Scrollbar"_s);
    m_settings->createSetting<Internal::StatusPlayingScript>(
        "[$num(%track%,2). ][%title% ($timems(%duration%))][ \u2022 %albumartist%][ \u2022 %album%]",
        u"StatusWidget/PlayingScript"_s);

    qRegisterMetaType<Fooyin::LibraryTreeAppearance>("Fooyin::LibraryTreeAppearance");

    m_libraryTreeGroupRegistry = std::make_unique<LibraryTreeGroupRegistry>(m_settings);

    m_settings->createSetting<Internal::LibTreeDoubleClick>(1, u"LibraryTree/DoubleClickBehaviour"_s);
    m_settings->createSetting<Internal::LibTreeMiddleClick>(0, u"LibraryTree/MiddleClickkBehaviour"_s);
    m_settings->createSetting<Internal::LibTreePlaylistEnabled>(false, u"LibraryTree/SelectionPlaylistEnabled"_s);
    m_settings->createSetting<Internal::LibTreeAutoSwitch>(true, u"LibraryTree/SelectionPlaylistAutoSwitch"_s);
    m_settings->createSetting<Internal::LibTreeAutoPlaylist>("Library Selection",
                                                             u"LibraryTree/SelectionPlaylistName"_s);
    m_settings->createSetting<Internal::LibTreeHeader>(true, u"LibraryTree/Header"_s);
    m_settings->createSetting<Internal::LibTreeScrollBar>(true, u"LibraryTree/Scrollbar"_s);
    m_settings->createSetting<Internal::LibTreeAltColours>(false, u"LibraryTree/AlternatingColours"_s);
    m_settings->createSetting<Internal::LibTreeAppearance>(QVariant::fromValue(LibraryTreeAppearance{}),
                                                           u"LibraryTree/Appearance"_s);

    m_libraryTreeGroupRegistry->loadItems();
    m_playlistPresetRegistry->loadItems();
}

GuiSettings::~GuiSettings() = default;

void GuiSettings::shutdown()
{
    m_libraryTreeGroupRegistry->saveItems();
    m_playlistPresetRegistry->saveItems();
}

LibraryTreeGroupRegistry* GuiSettings::libraryTreeGroupRegistry() const
{
    return m_libraryTreeGroupRegistry.get();
}

PresetRegistry* GuiSettings::playlistPresetRegistry() const
{
    return m_playlistPresetRegistry.get();
}
} // namespace Fooyin
