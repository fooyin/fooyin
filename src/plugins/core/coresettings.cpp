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

#include "core/settings/settingsmanager.h"
#include "core/typedefs.h"
#include "version.h"

#include <pluginsystem/pluginmanager.h>
#include <utils/paths.h>
#include <utils/utils.h>

namespace Core::Setting {
CoreSettings::CoreSettings()
    : m_settings(PluginSystem::object<SettingsManager>())
{
    m_settings->createSetting(Setting::Version, VERSION);
    m_settings->createSetting(Setting::DatabaseVersion, DATABASE_VERSION);
    m_settings->createTempSetting(Setting::FirstRun, true);
    m_settings->createTempSetting(Setting::LayoutEditing, false);
    m_settings->createSetting(Setting::Geometry, "", "Layout");
    m_settings->createSetting(Setting::Layout, "", "Layout");
    m_settings->createSetting(Setting::SplitterHandles, true, "Splitters");
    m_settings->createSetting(Setting::DiscHeaders, true, "Playlist");
    m_settings->createSetting(Setting::SplitDiscs, false, "Playlist");
    m_settings->createSetting(Setting::SimplePlaylist, false, "Playlist");
    m_settings->createSetting(Setting::PlaylistAltColours, true, "Playlist");
    m_settings->createSetting(Setting::PlaylistHeader, true, "Playlist");
    m_settings->createSetting(Setting::PlaylistScrollBar, true, "Playlist");
    m_settings->createSetting(Setting::PlayMode, Player::PlayMode::Default, "Player");
    m_settings->createSetting(Setting::ElapsedTotal, false, "Player");
    m_settings->createSetting(Setting::FilterAltColours, false, "Filters");
    m_settings->createSetting(Setting::FilterHeader, true, "Filters");
    m_settings->createSetting(Setting::FilterScrollBar, true, "Filters");
    m_settings->createSetting(Setting::InfoAltColours, true, "Info");
    m_settings->createSetting(Setting::InfoHeader, true, "Info");
    m_settings->createSetting(Setting::InfoScrollBar, true, "Info");

    m_settings->set(Setting::FirstRun, !Utils::File::exists(Utils::settingsPath()));
}

CoreSettings::~CoreSettings() = default;
}; // namespace Core::Setting
