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

#include "coresettings.h"

#include "core/player/playermanager.h"
#include "corepaths.h"
#include "version.h"

#include <utils/settings/settingsmanager.h>
#include <utils/utils.h>

namespace Fy::Core::Settings {
CoreSettings::CoreSettings(Utils::SettingsManager* settingsManager)
    : m_settings{settingsManager}
{
    m_settings->createSetting(Settings::Version, VERSION);
    m_settings->createSetting(Settings::DatabaseVersion, DATABASE_VERSION);
    m_settings->createTempSetting(Settings::FirstRun, true);
    m_settings->createSetting(Settings::PlayMode, Player::PlayMode::Default, "Player");
    m_settings->createSetting(Settings::AutoRefresh, true, "Library");

    m_settings->set<Settings::FirstRun>(!Utils::File::exists(settingsPath()));
}
} // namespace Fy::Core::Settings
