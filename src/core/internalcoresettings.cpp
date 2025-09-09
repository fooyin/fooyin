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

using namespace Qt::StringLiterals;

constexpr auto LogLevel = "LogLevel";

namespace Fooyin {
FySettings::FySettings(QObject* parent)
    : QSettings{Core::settingsPath(), QSettings::IniFormat, parent}
{ }

FyStateSettings::FyStateSettings(QObject* parent)
    : QSettings{Core::statePath(), QSettings::IniFormat, parent}
{ }

CoreSettings::CoreSettings(SettingsManager* settingsManager)
    : m_settings{settingsManager}
{
    using namespace Settings::Core;

    qRegisterMetaType<FadingIntervals>("FadingIntervals");

    m_settings->createTempSetting<FirstRun>(true);
    m_settings->createTempSetting<Version>(QString::fromLatin1(VERSION));
    m_settings->createSetting<PlayMode>(0, QString::fromLatin1(PlayModeKey));
    m_settings->createSetting<AutoRefresh>(true, u"Library/AutoRefresh"_s);
    m_settings->createSetting<LibrarySortScript>(
        u"%albumartist% - %year% - %album% - $num(%disc%,5) - $num(%track%,5) - %title%"_s, u"Library/SortScript"_s);
    m_settings->createSetting<AudioOutput>(QString{}, u"Engine/AudioOutput"_s);
    m_settings->createSetting<OutputVolume>(1.0, u"Engine/OutputVolume"_s);
    m_settings->createSetting<RewindPreviousTrack>(false, u"Playlist/RewindPreviousTrack"_s);
    m_settings->createSetting<GaplessPlayback>(true, u"Engine/GaplessPlayback"_s);
    m_settings->createSetting<Language>(QString{}, u"Language"_s);
    m_settings->createSetting<BufferLength>(4000, u"Engine/BufferLength"_s);
    m_settings->createSetting<OpenFilesPlaylist>(u"Default"_s, u"Playlist/OpenFilesPlaylist"_s);
    m_settings->createSetting<OpenFilesSendTo>(false, u"Playlist/OpenFilesSendToPlaylist"_s);
    m_settings->createSetting<SaveRatingToMetadata>(false, u"Library/SaveRatingToFile"_s);
    m_settings->createSetting<SavePlaycountToMetadata>(false, u"Library/SavePlaycountToFile"_s);
    m_settings->createSetting<PlayedThreshold>(0.5, u"Playback/PlayedThreshold"_s);
    m_settings->createSetting<ExternalSortScript>(u"%filepath%"_s, u"Library/ExternalSortScript"_s);
    m_settings->createTempSetting<Shutdown>(false);
    m_settings->createTempSetting<StopAfterCurrent>(false);
    m_settings->createSetting<RGMode>(0, u"Engine/ReplayGainMode"_s);
    m_settings->createSetting<RGType>(static_cast<int>(ReplayGainType::Track), u"Engine/ReplayGainType"_s);
    m_settings->createSetting<RGPreAmp>(0.0F, u"Engine/ReplayGainPreAmp"_s);
    m_settings->createSetting<NonRGPreAmp>(0.0F, u"Engine/NonReplayGainPreAmp"_s);
    m_settings->createSetting<ProxyMode>(static_cast<int>(NetworkAccessManager::Mode::None), u"Networking/ProxyMode"_s);
    m_settings->createSetting<ProxyConfig>(QVariant{}, u"Networking/ProxyConfig"_s);
    m_settings->createSetting<UseVariousForCompilations>(false, u"Library/UseVariousArtistsForCompilations"_s);
    m_settings->createSetting<ShuffleAlbumsGroupScript>(u"%albumartist% - %date% - %album%"_s,
                                                        u"Playback/ShuffleAlbumsGroupScript"_s);
    m_settings->createSetting<ShuffleAlbumsSortScript>(u"%disc% - %track% - %title%"_s,
                                                       u"Playback/ShuffleAlbumsSortScript"_s);
    m_settings->createTempSetting<ActiveTrack>(QVariant{});
    m_settings->createTempSetting<ActiveTrackId>(-2);
    m_settings->createSetting<FollowPlaybackQueue>(false, u"Playback/FollowPlaybackQueue"_s);

    m_settings->createSetting<Internal::MonitorLibraries>(true, u"Library/MonitorLibraries"_s);
    m_settings->createTempSetting<Internal::MuteVolume>(m_settings->value<OutputVolume>());
    m_settings->createSetting<Internal::DisabledPlugins>(QStringList{}, u"Plugins/Disabled"_s);
    m_settings->createSetting<Internal::EngineFading>(false, u"Engine/Fading"_s);
    m_settings->createSetting<Internal::FadingIntervals>(QVariant::fromValue(FadingIntervals{}),
                                                         u"Engine/FadingIntervals"_s);
    m_settings->createSetting<Internal::VBRUpdateInterval>(1000, u"Engine/VBRUpdateInterval"_s);

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
