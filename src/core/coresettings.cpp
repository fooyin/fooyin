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

#include <core/coresettings.h>

#include "version.h"

#include <core/corepaths.h>
#include <utils/settings/settingsmanager.h>
#include <utils/utils.h>

using namespace Qt::Literals::StringLiterals;

namespace Fy::Core::Settings {
CoreSettings::CoreSettings(Utils::SettingsManager* settingsManager)
    : m_settings{settingsManager}
{
    m_settings->createSetting<Settings::Version>(VERSION);
    m_settings->createSetting<Settings::DatabaseVersion>("0.1.0");
    m_settings->createTempSetting<Settings::FirstRun>(true);
    m_settings->createSetting<Settings::PlayMode>(0, u"Player"_s);
    m_settings->createSetting<Settings::AutoRefresh>(false, u"Library"_s);
    m_settings->createSetting<Settings::LibrarySorting>(QByteArray{}, u"Library"_s);
    m_settings->createSetting<Settings::LibrarySortScript>(
        "%albumartist% - %year% - %album% - $num(%disc%,2) - $num(%track%,2) - %title%", u"Library"_s);
    m_settings->createSetting<Settings::ActivePlaylistId>(0, u"Playlist"_s);
    m_settings->createSetting<Settings::AudioOutput>("ALSA|default", u"Engine"_s);
    m_settings->createSetting<Settings::OutputVolume>(1.0, u"Engine"_s);
    m_settings->createSetting<Settings::RewindPreviousTrack>(false, u"Playlist"_s);

    m_settings->set<Settings::FirstRun>(!Utils::File::exists(settingsPath()));

    m_settings->loadSettings();
}
} // namespace Fy::Core::Settings
