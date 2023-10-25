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

#include <utils/settings/settingsmanager.h>

using namespace Qt::Literals::StringLiterals;

namespace Fy::Gui::Settings {
GuiSettings::GuiSettings(Utils::SettingsManager* settingsManager)
    : m_settings{settingsManager}
{
    m_settings->createTempSetting<Settings::LayoutEditing>(false);
    m_settings->createSetting<Settings::StartupBehaviour>(2, u"Interface"_s);
    m_settings->createSetting<Settings::WaitForTracks>(true, u"Interface"_s);
    m_settings->createSetting<Settings::Geometry>(QByteArray{}, u"Interface"_s);
    m_settings->createSetting<Settings::SettingsGeometry>(QByteArray{}, u"Interface"_s);
    m_settings->createSetting<Settings::EditingMenuLevels>(2, u"Interface"_s);
    m_settings->createSetting<Settings::SplitterHandles>(true, u"Splitters"_s);
    m_settings->createSetting<Settings::PlaylistAltColours>(true, u"Playlist"_s);
    m_settings->createSetting<Settings::PlaylistHeader>(true, u"Playlist"_s);
    m_settings->createSetting<Settings::PlaylistScrollBar>(true, u"Playlist"_s);
    m_settings->createSetting<Settings::PlaylistPresets>(QByteArray{}, u"Playlist"_s);
    m_settings->createSetting<Settings::CurrentPreset>("Default", u"Playlist"_s);
    m_settings->createSetting<Settings::ElapsedTotal>(false, u"Player"_s);
    m_settings->createSetting<Settings::InfoAltColours>(true, u"Info"_s);
    m_settings->createSetting<Settings::InfoHeader>(true, u"Info"_s);
    m_settings->createSetting<Settings::InfoScrollBar>(true, u"Info"_s);
    m_settings->createSetting<Settings::IconTheme>("light", u"Theme"_s);
    m_settings->createSetting<Settings::LastPlaylistId>(0, u"Playlist"_s);
    m_settings->createSetting<Settings::LibraryTreeGrouping>(QByteArray{}, u"LibraryTree"_s);
    m_settings->createSetting<Settings::LibraryTreeDoubleClick>(1, u"LibraryTree"_s);
    m_settings->createSetting<Settings::LibraryTreeMiddleClick>(0, u"LibraryTree"_s);
    m_settings->createSetting<Settings::LibraryTreePlaylistEnabled>(false, u"LibraryTree"_s);
    m_settings->createSetting<Settings::LibraryTreeAutoSwitch>(true, u"LibraryTree"_s);
    m_settings->createSetting<Settings::LibraryTreeAutoPlaylist>("Library Selection", u"LibraryTree"_s);
    m_settings->createSetting<Settings::LibraryTreeHeader>(true, u"LibraryTree"_s);
    m_settings->createSetting<Settings::LibraryTreeScrollBar>(true, u"LibraryTree"_s);
    m_settings->createSetting<Settings::LibraryTreeAltColours>(false, u"LibraryTree"_s);
    m_settings->createSetting<Settings::LibraryTreeAppearance>(QVariant::fromValue(Widgets::LibraryTreeAppearance{}),
                                                               u"LibraryTree"_s);
    m_settings->createSetting<Settings::ScriptSandboxState>(QByteArray{}, u"Interface"_s);
    m_settings->createSetting<Settings::PlaylistThumbnailSize>(100, u"Playlist"_s);

    m_settings->loadSettings();
}
} // namespace Fy::Gui::Settings
