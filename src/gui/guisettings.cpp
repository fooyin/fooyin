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

#include "librarytree/librarytreeappearance.h"
#include "librarytree/librarytreegroupregistry.h"
#include "playlist/presetregistry.h"

#include <utils/settings/settingsmanager.h>

using namespace Qt::Literals::StringLiterals;

namespace Fooyin {
GuiSettings::GuiSettings(SettingsManager* settingsManager)
    : m_settings{settingsManager}
    , m_libraryTreeGroupRegistry{new LibraryTreeGroupRegistry(m_settings)}
    , m_playlistPresetRegistry{new PresetRegistry(m_settings)}
{
    m_settings->createTempSetting<Settings::Gui::LayoutEditing>(false);
    m_settings->createSetting<Settings::Gui::StartupBehaviour>(2, u"Interface"_s);
    m_settings->createSetting<Settings::Gui::WaitForTracks>(true, u"Interface"_s);
    m_settings->createSetting<Settings::Gui::EditingMenuLevels>(2, u"Interface"_s);
    m_settings->createSetting<Settings::Gui::SplitterHandles>(true, u"Splitters"_s);
    m_settings->createSetting<Settings::Gui::PlaylistAltColours>(true, u"Playlist"_s);
    m_settings->createSetting<Settings::Gui::PlaylistHeader>(true, u"Playlist"_s);
    m_settings->createSetting<Settings::Gui::PlaylistScrollBar>(true, u"Playlist"_s);
    m_settings->createSetting<Settings::Gui::PlaylistPresets>(QByteArray{}, u"Playlist"_s);
    m_settings->createSetting<Settings::Gui::CurrentPreset>("Default", u"Playlist"_s);
    m_settings->createSetting<Settings::Gui::ElapsedTotal>(false, u"Player"_s);
    m_settings->createSetting<Settings::Gui::InfoAltColours>(true, u"Info"_s);
    m_settings->createSetting<Settings::Gui::InfoHeader>(true, u"Info"_s);
    m_settings->createSetting<Settings::Gui::InfoScrollBar>(true, u"Info"_s);
    m_settings->createSetting<Settings::Gui::IconTheme>("light", u"Theme"_s);
    m_settings->createSetting<Settings::Gui::LastPlaylistId>(0, u"Playlist"_s);
    m_settings->createSetting<Settings::Gui::LibraryTreeGrouping>(QByteArray{}, u"LibraryTree"_s);
    m_settings->createSetting<Settings::Gui::LibraryTreeDoubleClick>(1, u"LibraryTree"_s);
    m_settings->createSetting<Settings::Gui::LibraryTreeMiddleClick>(0, u"LibraryTree"_s);
    m_settings->createSetting<Settings::Gui::LibraryTreePlaylistEnabled>(false, u"LibraryTree"_s);
    m_settings->createSetting<Settings::Gui::LibraryTreeAutoSwitch>(true, u"LibraryTree"_s);
    m_settings->createSetting<Settings::Gui::LibraryTreeAutoPlaylist>("Library Selection", u"LibraryTree"_s);
    m_settings->createSetting<Settings::Gui::LibraryTreeHeader>(true, u"LibraryTree"_s);
    m_settings->createSetting<Settings::Gui::LibraryTreeScrollBar>(true, u"LibraryTree"_s);
    m_settings->createSetting<Settings::Gui::LibraryTreeAltColours>(false, u"LibraryTree"_s);
    m_settings->createSetting<Settings::Gui::LibraryTreeAppearance>(QVariant::fromValue(LibraryTreeAppearance{}),
                                                                    u"LibraryTree"_s);
    m_settings->createSetting<Settings::Gui::PlaylistThumbnailSize>(100, u"Playlist"_s);
    m_settings->createSetting<Settings::Gui::CursorFollowsPlayback>(false, u"Playlist"_s);
    m_settings->createSetting<Settings::Gui::PlaybackFollowsCursor>(false, u"Playlist"_s);
    m_settings->createSetting<Settings::Gui::PlaylistTabsSingleHide>(false, u"PlaylistTabs"_s);
    m_settings->createSetting<Settings::Gui::StatusPlayingScript>(
        "[$num(%track%,2). ][%title% ($timems(%duration%))][ \u2022 %albumartist%][ \u2022 %album%]",
        u"StatusWidget"_s);

    m_settings->loadSettings();
    m_libraryTreeGroupRegistry->loadItems();
    m_playlistPresetRegistry->loadItems();
}

GuiSettings::~GuiSettings()
{
    m_libraryTreeGroupRegistry->saveItems();
    m_playlistPresetRegistry->saveItems();
    m_settings->storeSettings();
}

LibraryTreeGroupRegistry* GuiSettings::libraryTreeGroupRegistry() const
{
    return m_libraryTreeGroupRegistry;
}

PresetRegistry* GuiSettings::playlistPresetRegistry() const
{
    return m_playlistPresetRegistry;
}
} // namespace Fooyin
