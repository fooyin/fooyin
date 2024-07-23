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

namespace Fooyin {
FySettings::FySettings(QObject* parent)
    : QSettings{Core::settingsPath(), QSettings::IniFormat, parent}
{ }

CoreSettings::CoreSettings(SettingsManager* settingsManager)
    : m_settings{settingsManager}
{
    using namespace Settings::Core;

    qRegisterMetaType<FadingIntervals>("FadingIntervals");

    m_settings->createTempSetting<FirstRun>(true);
    m_settings->createSetting<Version>(QString::fromLatin1(VERSION), QStringLiteral("Version"));
    m_settings->createSetting<PlayMode>(0, QStringLiteral("Player/PlayMode"));
    m_settings->createSetting<AutoRefresh>(true, QStringLiteral("Library/AutoRefresh"));
    m_settings->createSetting<LibrarySortScript>(
        QStringLiteral("%albumartist% - %year% - %album% - $num(%disc%,5) - $num(%track%,5) - %title%"),
        QStringLiteral("Library/SortScript"));
    m_settings->createSetting<ActivePlaylistId>(-1, QStringLiteral("Playlist/ActivePlaylistId"));
    m_settings->createSetting<AudioOutput>(QString{}, QStringLiteral("Engine/AudioOutput"));
    m_settings->createSetting<OutputVolume>(1.0, QStringLiteral("Engine/OutputVolume"));
    m_settings->createSetting<RewindPreviousTrack>(false, QStringLiteral("Playlist/RewindPreviousTrack"));
    m_settings->createSetting<GaplessPlayback>(true, QStringLiteral("Engine/GaplessPlayback"));
    m_settings->createSetting<Language>(QString{}, QStringLiteral("Language"));
    m_settings->createSetting<BufferLength>(4000, QStringLiteral("Engine/BufferLength"));
    m_settings->createSetting<PlaylistSavePathType>(0, QStringLiteral("Playlist/SavePathType"));
    m_settings->createSetting<PlaylistSaveMetadata>(false, QStringLiteral("Playlist/SaveMetadata"));

    m_settings->createSetting<Internal::MonitorLibraries>(true, QStringLiteral("Library/MonitorLibraries"));
    m_settings->createTempSetting<Internal::MuteVolume>(m_settings->value<OutputVolume>());
    m_settings->createSetting<Internal::DisabledPlugins>(QStringList{}, QStringLiteral("Plugins/Disabled"));
    m_settings->createSetting<Internal::SavePlaybackState>(false, QStringLiteral("Player/SavePlaybackState"));
    m_settings->createSetting<Internal::EngineFading>(false, QStringLiteral("Engine/Fading"));
    m_settings->createSetting<Internal::FadingIntervals>(QVariant::fromValue(FadingIntervals{}),
                                                         QStringLiteral("Engine/FadingIntervals"));
    m_settings->createSetting<SkipUnavailable>(false, QStringLiteral("Playlist/SkipUnavailable"));
    m_settings->createSetting<OpenFilesPlaylist>(QStringLiteral("Default"),
                                                 QStringLiteral("Playlist/OpenFilesPlaylist"));
    m_settings->createSetting<OpenFilesSendTo>(false, QStringLiteral("Playlist/OpenFilesSendToPlaylist"));
    m_settings->createSetting<SaveRatingToMetadata>(false, QStringLiteral("Library/SaveRatingToFile"));
    m_settings->createSetting<SavePlaycountToMetadata>(false, QStringLiteral("Library/SavePlaycountToFile"));
    m_settings->createSetting<PlayedThreshold>(0.5, QStringLiteral("Playback/PlayedThreshold"));

    m_settings->set<FirstRun>(!QFileInfo::exists(Core::settingsPath()));
}

void CoreSettings::shutdown()
{
    m_settings->fileSet(QStringLiteral("Version"), QString::fromLatin1(VERSION));
}
} // namespace Fooyin
