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

namespace Fooyin {
CoreSettings::CoreSettings(SettingsManager* settingsManager)
    : m_settings{settingsManager}
{
    using namespace Settings::Core;

    m_settings->createTempSetting<FirstRun>(true);
    m_settings->createSetting<Version>(VERSION, QStringLiteral("Version"));
    m_settings->createSetting<PlayMode>(0, QStringLiteral("Player/PlayMode"));
    m_settings->createSetting<AutoRefresh>(true, QStringLiteral("Library/AutoRefresh"));
    m_settings->createSetting<Internal::MonitorLibraries>(true, QStringLiteral("Library/MonitorLibraries"));
    m_settings->createSetting<LibrarySortScript>(
        "%albumartist% - %year% - %album% - $num(%disc%,5) - $num(%track%,5) - %title%",
        QStringLiteral("Library/SortScript"));
    m_settings->createSetting<ActivePlaylistId>(0, QStringLiteral("Playlist/ActivePlaylistId"));
    m_settings->createSetting<AudioOutput>("ALSA|default", QStringLiteral("Engine/AudioOutput"));
    m_settings->createSetting<OutputVolume>(1.0, QStringLiteral("Engine/OutputVolume"));
    m_settings->createSetting<RewindPreviousTrack>(false, QStringLiteral("Playlist/RewindPreviousTrack"));
    m_settings->createSetting<GaplessPlayback>(true, QStringLiteral("Engine/GaplessPlayback"));
    m_settings->createSetting<Language>(QStringLiteral(""), QStringLiteral("Language"));

    m_settings->set<FirstRun>(!QFileInfo::exists(Core::settingsPath()));
}

void CoreSettings::shutdown()
{
    m_settings->fileSet(VersionSetting, VERSION);
}
} // namespace Fooyin
