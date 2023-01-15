/*
 * Fooyin
 * Copyright 2022, Luke Taylor <LukeT1@proton.me>
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

#include "coresettings.h"

#include "core/typedefs.h"
#include "version.h"

#include <pluginsystem/pluginmanager.h>
#include <utils/paths.h>
#include <utils/utils.h>

namespace Core::Settings {
CoreSettings::CoreSettings()
    : m_settings(PluginSystem::object<SettingsManager>())
{
    m_settings->createSetting(Settings::Version, VERSION);
    m_settings->createSetting(Settings::DatabaseVersion, DATABASE_VERSION);
    m_settings->createTempSetting(Settings::FirstRun, true);
    m_settings->createTempSetting(Settings::LayoutEditing, false);
    m_settings->createSetting(Settings::Geometry, "", "Layout");
    m_settings->createSetting(Settings::Layout, "", "Layout");
    m_settings->createSetting(Settings::SplitterHandles, true, "Splitters");
    m_settings->createSetting(Settings::DiscHeaders, true, "Playlist");
    m_settings->createSetting(Settings::SplitDiscs, false, "Playlist");
    m_settings->createSetting(Settings::SimplePlaylist, false, "Playlist");
    m_settings->createSetting(Settings::PlaylistAltColours, true, "Playlist");
    m_settings->createSetting(Settings::PlaylistHeader, true, "Playlist");
    m_settings->createSetting(Settings::PlaylistScrollBar, true, "Playlist");
    m_settings->createSetting(Settings::PlayMode, Player::PlayMode::Default, "Player");
    m_settings->createSetting(Settings::ElapsedTotal, false, "Player");
    m_settings->createSetting(Settings::InfoAltColours, true, "Info");
    m_settings->createSetting(Settings::InfoHeader, true, "Info");
    m_settings->createSetting(Settings::InfoScrollBar, true, "Info");

    m_settings->set<Settings::FirstRun>(!Utils::File::exists(Utils::settingsPath()));
}

CoreSettings::~CoreSettings() = default;
} // namespace Core::Settings
