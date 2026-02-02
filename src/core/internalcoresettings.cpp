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
#include <core/engine/audioformat.h>
#include <core/engine/enginedefs.h>
#include <core/network/networkaccessmanager.h>
#include <utils/logging/messagehandler.h>
#include <utils/settings/settingsmanager.h>

#include <QFileInfo>

using namespace Qt::StringLiterals;

constexpr auto LogLevel = "LogLevel";

namespace Fooyin {
namespace {
Engine::FadingValues defaultFadingValues()
{
    Engine::FadingValues values;
    values.pause.in    = 120;
    values.pause.out   = 120;
    values.pause.curve = Engine::FadeCurve::Cosine;
    values.stop.in     = 120;
    values.stop.out    = 300;
    values.stop.curve  = Engine::FadeCurve::Cosine;
    return values;
}

Engine::CrossfadingValues defaultCrossfadingValues()
{
    Engine::CrossfadingValues values;
    values.manualChange.in  = 300;
    values.manualChange.out = 300;
    values.autoChange.in    = 700;
    values.autoChange.out   = 700;
    values.seek.in          = 80;
    values.seek.out         = 80;
    return values;
}
} // namespace

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

    qRegisterMetaType<Engine::FadingValues>("FadingValues");
    qRegisterMetaType<Engine::CrossfadingValues>("CrossfadingValues");

    m_settings->createTempSetting<FirstRun>(true);
    m_settings->createTempSetting<Version>(QString::fromLatin1(VERSION));
    m_settings->createSetting<PlayMode>(0, QString::fromLatin1(PlayModeKey));
    m_settings->createSetting<AutoRefresh>(true, u"Library/AutoRefresh"_s);
    m_settings->createSetting<LibrarySortScript>(
        u"%albumartist% - %year% - %album% - $num(%disc%,5) - $num(%track%,5) - %title%"_s, u"Library/SortScript"_s);
    m_settings->createSetting<Settings::Core::AudioOutput>(QString{}, u"Engine/AudioOutput"_s);
    m_settings->createSetting<OutputVolume>(1.0, u"Engine/OutputVolume"_s);
    m_settings->createSetting<OutputBitDepth>(static_cast<int>(SampleFormat::Unknown), u"Engine/OutputBitDepth"_s);
    m_settings->createSetting<OutputDither>(false, u"Engine/OutputDither"_s);
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
    m_settings->createSetting<StopAfterCurrent>(false, u"Playback/StopAfterCurrent"_s);
    m_settings->createSetting<RGMode>(0, u"Engine/ReplayGainMode"_s);
    m_settings->createSetting<RGType>(static_cast<int>(ReplayGainType::Track), u"Engine/ReplayGainType"_s);
    m_settings->createSetting<RGPreAmp>(0.0F, u"Engine/ReplayGainPreAmp"_s);
    m_settings->createSetting<NonRGPreAmp>(0.0F, u"Engine/NonReplayGainPreAmp"_s);
    m_settings->createSetting<UseVariousForCompilations>(false, u"Library/UseVariousArtistsForCompilations"_s);
    m_settings->createSetting<ShuffleAlbumsGroupScript>(u"%albumartist% - %date% - %album%"_s,
                                                        u"Playback/ShuffleAlbumsGroupScript"_s);
    m_settings->createSetting<ShuffleAlbumsSortScript>(u"%disc% - %track% - %title%"_s,
                                                       u"Playback/ShuffleAlbumsSortScript"_s);
    m_settings->createTempSetting<ActiveTrack>(QVariant{});
    m_settings->createTempSetting<ActiveTrackId>(-2);
    m_settings->createSetting<FollowPlaybackQueue>(false, u"Playback/FollowPlaybackQueue"_s);
    m_settings->createSetting<StopIfActivePlaylistDeleted>(false, u"Playback/StopIfActivePlaylistDeleted"_s);
    m_settings->createSetting<ResetStopAfterCurrent>(false, u"Playback/ResetStopAfterCurrent"_s);
    m_settings->createSetting<PreserveTimestamps>(false, u"Tagging/PreserveTimestamps"_s);
    m_settings->createSetting<PlaylistSkipMissing>(true, u"Playlist/SkipMissing"_s);
    m_settings->createSetting<PlaybackQueueStopWhenFinished>(false, u"Playback/PlaybackQueueStopWhenFinished"_s);

    m_settings->createSetting<Internal::MonitorLibraries>(true, u"Library/MonitorLibraries"_s);
    m_settings->createTempSetting<Internal::MuteVolume>(m_settings->value<OutputVolume>());
    m_settings->createSetting<Internal::DisabledPlugins>(QStringList{}, u"Plugins/Disabled"_s);
    m_settings->createSetting<Internal::EngineFading>(false, u"Engine/Fading"_s);
    m_settings->createSetting<Internal::FadingValues>(QVariant::fromValue(defaultFadingValues()),
                                                      u"Engine/FadingValues"_s);
    m_settings->createSetting<Internal::EngineCrossfading>(false, u"Engine/Crossfading"_s);
    m_settings->createSetting<Internal::CrossfadingValues>(QVariant::fromValue(defaultCrossfadingValues()),
                                                           u"Engine/CrossfadingValues"_s);
    m_settings->createSetting<Internal::VBRUpdateInterval>(1000, u"Engine/VBRUpdateInterval"_s);
    m_settings->createSetting<Internal::ProxyMode>(static_cast<int>(NetworkAccessManager::Mode::None),
                                                   u"Networking/ProxyMode"_s);
    m_settings->createSetting<Internal::ProxyType>(static_cast<int>(QNetworkProxy::HttpProxy),
                                                   u"Networking/ProxyType"_s);
    m_settings->createSetting<Internal::ProxyHost>(u""_s, u"Networking/ProxyHost"_s);
    m_settings->createSetting<Internal::ProxyPort>(8080, u"Networking/ProxyPort"_s);
    m_settings->createSetting<Internal::ProxyAuth>(false, u"Networking/ProxyAuth"_s);
    m_settings->createSetting<Internal::ProxyUsername>(u""_s, u"Networking/ProxyUsername"_s);
    m_settings->createSetting<Internal::ProxyPassword>(u""_s, u"Networking/ProxyPassword"_s);

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
