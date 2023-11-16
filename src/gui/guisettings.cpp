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
    m_settings->createTempSetting<Gui::Settings::LayoutEditing>(false);
    m_settings->createSetting<Gui::Settings::StartupBehaviour>(2, u"Interface"_s);
    m_settings->createSetting<Gui::Settings::WaitForTracks>(true, u"Interface"_s);
    m_settings->createSetting<Gui::Settings::EditingMenuLevels>(2, u"Interface"_s);
    m_settings->createSetting<Gui::Settings::SplitterHandles>(true, u"Splitters"_s);
    m_settings->createSetting<Gui::Settings::PlaylistAltColours>(true, u"Playlist"_s);
    m_settings->createSetting<Gui::Settings::PlaylistHeader>(true, u"Playlist"_s);
    m_settings->createSetting<Gui::Settings::PlaylistScrollBar>(true, u"Playlist"_s);
    m_settings->createSetting<Gui::Settings::PlaylistPresets>(QByteArray{}, u"Playlist"_s);
    m_settings->createSetting<Gui::Settings::CurrentPreset>("Default", u"Playlist"_s);
    m_settings->createSetting<Gui::Settings::ElapsedTotal>(false, u"Player"_s);
    m_settings->createSetting<Gui::Settings::InfoAltColours>(true, u"Info"_s);
    m_settings->createSetting<Gui::Settings::InfoHeader>(true, u"Info"_s);
    m_settings->createSetting<Gui::Settings::InfoScrollBar>(true, u"Info"_s);
    m_settings->createSetting<Gui::Settings::IconTheme>("light", u"Theme"_s);
    m_settings->createSetting<Gui::Settings::LastPlaylistId>(0, u"Playlist"_s);
    m_settings->createSetting<Gui::Settings::LibraryTreeGrouping>(QByteArray{}, u"LibraryTree"_s);
    m_settings->createSetting<Gui::Settings::LibraryTreeDoubleClick>(1, u"LibraryTree"_s);
    m_settings->createSetting<Gui::Settings::LibraryTreeMiddleClick>(0, u"LibraryTree"_s);
    m_settings->createSetting<Gui::Settings::LibraryTreePlaylistEnabled>(false, u"LibraryTree"_s);
    m_settings->createSetting<Gui::Settings::LibraryTreeAutoSwitch>(true, u"LibraryTree"_s);
    m_settings->createSetting<Gui::Settings::LibraryTreeAutoPlaylist>("Library Selection", u"LibraryTree"_s);
    m_settings->createSetting<Gui::Settings::LibraryTreeHeader>(true, u"LibraryTree"_s);
    m_settings->createSetting<Gui::Settings::LibraryTreeScrollBar>(true, u"LibraryTree"_s);
    m_settings->createSetting<Gui::Settings::LibraryTreeAltColours>(false, u"LibraryTree"_s);
    m_settings->createSetting<Gui::Settings::LibraryTreeAppearance>(QVariant::fromValue(LibraryTreeAppearance{}),
                                                                    u"LibraryTree"_s);
    m_settings->createSetting<Gui::Settings::PlaylistThumbnailSize>(100, u"Playlist"_s);
    m_settings->createSetting<Gui::Settings::CursorFollowsPlayback>(false, u"Playlist"_s);
    m_settings->createSetting<Gui::Settings::PlaybackFollowsCursor>(false, u"Playlist"_s);
    m_settings->createSetting<Gui::Settings::PlaylistTabsSingleHide>(false, u"PlaylistTabs"_s);

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
