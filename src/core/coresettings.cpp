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
#include <core/library/sortingregistry.h>
#include <utils/settings/settingsmanager.h>
#include <utils/utils.h>

using namespace Qt::Literals::StringLiterals;

namespace Fooyin {
CoreSettings::CoreSettings(SettingsManager* settingsManager)
    : m_settings{settingsManager}
    , m_sortingRegistry{std::make_unique<SortingRegistry>(settingsManager)}
{
    m_settings->createSetting<Settings::Core::Version>(VERSION);
    m_settings->createTempSetting<Settings::Core::FirstRun>(true);
    m_settings->createSetting<Settings::Core::PlayMode>(0, u"Player"_s);
    m_settings->createSetting<Settings::Core::AutoRefresh>(false, u"Library"_s);
    m_settings->createSetting<Settings::Core::LibrarySorting>(QByteArray{}, u"Library"_s);
    m_settings->createSetting<Settings::Core::LibrarySortScript>(
        "%albumartist% - %year% - %album% - $num(%disc%,2) - $num(%track%,2) - %title%", u"Library"_s);
    m_settings->createSetting<Settings::Core::ActivePlaylistId>(0, u"Playlist"_s);
    m_settings->createSetting<Settings::Core::AudioOutput>("ALSA|default", u"Engine"_s);
    m_settings->createSetting<Settings::Core::OutputVolume>(1.0, u"Engine"_s);
    m_settings->createSetting<Settings::Core::RewindPreviousTrack>(false, u"Playlist"_s);

    m_settings->set<Settings::Core::FirstRun>(!Utils::File::exists(Core::settingsPath()));

    m_settings->loadSettings();
    m_sortingRegistry->loadItems();
}

CoreSettings::~CoreSettings()
{
    m_sortingRegistry->saveItems();
    m_settings->storeSettings();
}

SortingRegistry* CoreSettings::sortingRegistry() const
{
    return m_sortingRegistry.get();
}
} // namespace Fooyin
