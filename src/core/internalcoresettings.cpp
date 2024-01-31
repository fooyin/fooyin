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

#include "internalcoresettings.h"

#include "corepaths.h"
#include "version.h"

#include <core/coresettings.h>
#include <utils/settings/settingsmanager.h>

#include <QFileInfo>

constexpr auto VersionSetting = "Version";

using namespace Qt::Literals::StringLiterals;

namespace Fooyin {
CoreSettings::CoreSettings(SettingsManager* settingsManager)
    : m_settings{settingsManager}
{
    using namespace Settings::Core;

    m_settings->createTempSetting<FirstRun>(true);
    m_settings->createSetting<Version>(VERSION, u"Version"_s);
    m_settings->createSetting<PlayMode>(0, u"Player/PlayMode"_s);
    m_settings->createSetting<AutoRefresh>(true, u"Library/AutoRefresh"_s);
    m_settings->createSetting<Internal::MonitorLibraries>(true, u"Library/MonitorLibraries"_s);
    m_settings->createSetting<LibrarySortScript>(
        "%albumartist% - %year% - %album% - $num(%disc%,2) - $num(%track%,2) - %title%", u"Library/SortScript"_s);
    m_settings->createSetting<ActivePlaylistId>(0, u"Playlist/ActivePlaylistId"_s);
    m_settings->createSetting<AudioOutput>("ALSA|default", u"Engine/AudioOutput"_s);
    m_settings->createSetting<OutputVolume>(1.0, u"Engine/OutputVolume"_s);
    m_settings->createSetting<RewindPreviousTrack>(false, u"Playlist/RewindPreviousTrack"_s);
    m_settings->createSetting<GaplessPlayback>(true, u"Engine/GaplessPlayback"_s);

    m_settings->set<FirstRun>(!QFileInfo::exists(Core::settingsPath()));
}

void CoreSettings::shutdown()
{
    m_settings->fileSet(VersionSetting, VERSION);
}
} // namespace Fooyin
