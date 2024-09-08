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
#include <core/network/networkaccessmanager.h>
#include <utils/logging/messagehandler.h>
#include <utils/settings/settingsmanager.h>

#include <QFileInfo>

constexpr auto LogLevel = "LogLevel";

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
    m_settings->createSetting<OpenFilesPlaylist>(QStringLiteral("Default"),
                                                 QStringLiteral("Playlist/OpenFilesPlaylist"));
    m_settings->createSetting<OpenFilesSendTo>(false, QStringLiteral("Playlist/OpenFilesSendToPlaylist"));
    m_settings->createSetting<SaveRatingToMetadata>(false, QStringLiteral("Library/SaveRatingToFile"));
    m_settings->createSetting<SavePlaycountToMetadata>(false, QStringLiteral("Library/SavePlaycountToFile"));
    m_settings->createSetting<PlayedThreshold>(0.5, QStringLiteral("Playback/PlayedThreshold"));
    m_settings->createSetting<ExternalSortScript>(QStringLiteral("%filepath%"),
                                                  QStringLiteral("Library/ExternalSortScript"));
    m_settings->createTempSetting<Shutdown>(false);
    m_settings->createTempSetting<StopAfterCurrent>(false);
    m_settings->createSetting<ReplayGainEnabled>(false, QStringLiteral("Engine/ReplayGainEnabled"));
    m_settings->createSetting<Settings::Core::ReplayGainType>(static_cast<int>(ReplayGainType::Track),
                                                              QStringLiteral("Engine/ReplayGainType"));
    m_settings->createSetting<ReplayGainPreAmp>(0, QStringLiteral("Engine/ReplayGainPreAmp"));
    m_settings->createSetting<ProxyMode>(static_cast<int>(NetworkAccessManager::Mode::None),
                                         QStringLiteral("Networking/ProxyMode"));
    m_settings->createSetting<ProxyConfig>(QVariant{}, QStringLiteral("Networking/ProxyConfig"));

    m_settings->createSetting<Internal::MonitorLibraries>(true, QStringLiteral("Library/MonitorLibraries"));
    m_settings->createTempSetting<Internal::MuteVolume>(m_settings->value<OutputVolume>());
    m_settings->createSetting<Internal::DisabledPlugins>(QStringList{}, QStringLiteral("Plugins/Disabled"));
    m_settings->createSetting<Internal::EngineFading>(false, QStringLiteral("Engine/Fading"));
    m_settings->createSetting<Internal::FadingIntervals>(QVariant::fromValue(FadingIntervals{}),
                                                         QStringLiteral("Engine/FadingIntervals"));

    m_settings->set<FirstRun>(!QFileInfo::exists(Core::settingsPath()));

    auto logLevel = m_settings->fileValue(LogLevel, QtInfoMsg);
    bool newLogFormat{false};
    int level = logLevel.toInt(&newLogFormat);
    if(!newLogFormat) {
        level = QtMsgType::QtInfoMsg;
    }
    MessageHandler::setLevel(static_cast<QtMsgType>(level));
}

CoreSettings::~CoreSettings()
{
    m_settings->fileSet(LogLevel, MessageHandler::level());
}
} // namespace Fooyin
