/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <luket@pm.me>
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

#include "application.h"

#include "corepaths.h"
#include "database/database.h"
#include "database/settingsdatabase.h"
#include "engine/dsp/dspchainstore.h"
#include "engine/dsp/dspregistry.h"
#include "engine/enginehandler.h"
#include "engine/enginehelpers.h"
#include "engine/input/archiveinput.h"
#include "engine/input/ffmpeg/ffmpeginput.h"
#include "engine/input/taglibparser.h"
#include "internalcoresettings.h"
#include "library/librarymanager.h"
#include "library/sortingregistry.h"
#include "library/unifiedmusiclibrary.h"
#include "playlist/parsers/cueparser.h"
#include "playlist/parsers/m3uparser.h"
#include "playlist/parsers/plsparser.h"
#include "plugins/pluginmanager.h"
#include "version.h"

#include <core/coresettings.h>
#include <core/engine/audiobuffer.h>
#include <core/engine/audioloader.h>
#include <core/engine/dsp/dspplugin.h>
#include <core/engine/outputplugin.h>
#include <core/network/networkaccessmanager.h>
#include <core/network/remoteioservice.h>
#include <core/player/playercontroller.h>
#include <core/playlist/playlisthandler.h>
#include <core/playlist/playlistloader.h>
#include <core/plugins/coreplugin.h>
#include <utils/settings/settingsmanager.h>

#include <QCoreApplication>
#include <QLoggingCategory>
#include <QProcess>
#include <QTimerEvent>

Q_LOGGING_CATEGORY(APP, "fy.app")

using namespace std::chrono_literals;
using namespace Qt::StringLiterals;

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
constexpr auto PlaylistSaveInterval = 30s;
constexpr auto SettingsSaveInterval = 5min;
#else
constexpr auto PlaylistSaveInterval = 30000;
constexpr auto SettingsSaveInterval = 300000;
#endif

namespace {
void registerTypes()
{
    qRegisterMetaType<Fooyin::Track>("Track");
    qRegisterMetaType<Fooyin::TrackList>("TrackList");
    qRegisterMetaType<Fooyin::TrackIds>("TrackIds");
    qRegisterMetaType<Fooyin::ScanProgress>("ScanProgress");
    qRegisterMetaType<Fooyin::AudioBuffer>("AudioBuffer");
    qRegisterMetaType<Fooyin::OutputCreator>("OutputCreator");
    qRegisterMetaType<Fooyin::LibraryInfo>("LibraryInfo");
    qRegisterMetaType<Fooyin::LibraryInfoMap>("LibraryInfoMap");
    qRegisterMetaType<Fooyin::Engine::PlaybackItem>("Fooyin::Engine::PlaybackItemRef");
    qRegisterMetaType<Fooyin::Engine::TrackCommitContext>("Fooyin::Engine::TrackCommitContext");
    qRegisterMetaType<Fooyin::Player::TrackChangeRequest>("Fooyin::Player::TrackChangeRequest");
    qRegisterMetaType<Fooyin::Player::UpcomingTrack>("Fooyin::Player::UpcomingTrack");
}
} // namespace

namespace Fooyin {
Application::Application(QObject* parent)
    : QObject{parent}
    , m_settings{new SettingsManager(Core::settingsPath(), this)}
    , m_coreSettings{m_settings}
    , m_database{new Database(this)}
    , m_audioLoader{std::make_shared<AudioLoader>()}
    , m_dspRegistry{std::make_unique<DspRegistry>()}
    , m_dspChainStore{std::make_unique<DspChainStore>(m_settings, m_dspRegistry.get())}
    , m_networkManager{std::make_shared<NetworkAccessManager>(m_settings)}
    , m_remoteIo{std::make_shared<RemoteIoService>(m_networkManager, m_settings)}
    , m_libraryManager{new LibraryManager(m_database->connectionPool(), m_settings, this)}
    , m_playlistLoader{std::make_shared<PlaylistLoader>()}
    , m_library{new UnifiedMusicLibrary(m_libraryManager, m_database->connectionPool(), m_playlistLoader, m_audioLoader,
                                        m_remoteIo, m_settings, this)}
    , m_playlistHandler{new PlaylistHandler(m_database->connectionPool(), m_audioLoader, m_library, m_settings, this)}
    , m_playerController{new PlayerController(m_settings, m_playlistHandler, this)}
    , m_playbackQueueStore{m_database->connectionPool(), m_library, m_playlistHandler}
    , m_engine{std::make_unique<EngineHandler>(m_audioLoader, m_playerController, m_settings, m_dspRegistry.get())}
    , m_sortingRegistry{new SortingRegistry(m_settings, this)}
    , m_pluginManager{std::make_unique<PluginManager>(m_settings)}
    , m_corePluginContext{m_engine.get(), m_playerController, m_libraryManager, m_library,         m_playlistHandler,
                          m_settings,     m_audioLoader,      m_playlistLoader, m_sortingRegistry, m_networkManager}
{
    m_audioLoader->setRemoteSourceProvider(m_remoteIo);
    m_translations.initialiseTranslations(m_settings->value<Settings::Core::Language>());
    loadDatabaseSettings();

    QObject::connect(m_playlistHandler, &PlaylistHandler::playlistAdded, this, &Application::startSaveTimer);
    QObject::connect(m_playlistHandler, &PlaylistHandler::playlistRemoved, this, &Application::startSaveTimer);
    QObject::connect(m_playlistHandler, &PlaylistHandler::tracksAdded, this, &Application::startSaveTimer);
    QObject::connect(m_playlistHandler, &PlaylistHandler::tracksChanged, this, &Application::startSaveTimer);
    QObject::connect(m_playlistHandler, &PlaylistHandler::tracksUpdated, this, &Application::startSaveTimer);
    QObject::connect(m_playlistHandler, &PlaylistHandler::tracksRemoved, this, &Application::startSaveTimer);
    QObject::connect(
        m_playlistHandler, &PlaylistHandler::playlistsPopulated, this,
        [this]() { m_playerController->replaceTracks(m_playbackQueueStore.load()); }, Qt::SingleShotConnection);
    QObject::connect(m_playerController, &PlayerController::tracksQueued, this,
                     [this]() { m_playbackQueueStore.save(m_playerController->playbackQueue()); });
    QObject::connect(m_playerController, &PlayerController::tracksDequeued, this,
                     [this]() { m_playbackQueueStore.save(m_playerController->playbackQueue()); });
    QObject::connect(m_playerController, &PlayerController::trackIndexesDequeued, this,
                     [this]() { m_playbackQueueStore.save(m_playerController->playbackQueue()); });
    QObject::connect(m_playerController, &PlayerController::trackQueueChanged, this,
                     [this]() { m_playbackQueueStore.save(m_playerController->playbackQueue()); });

    QObject::connect(m_library, &MusicLibrary::tracksMetadataChanged, this,
                     [this](const TrackList& tracks) { tracksWereUpdated(tracks); });
    QObject::connect(m_library, &MusicLibrary::tracksUpdated, this,
                     [this](const TrackList& tracks) { tracksWereUpdated(tracks); });
    QObject::connect(m_playerController, &PlayerController::trackPlayed, m_library,
                     &UnifiedMusicLibrary::trackWasPlayed);
    QObject::connect(m_playerController, &PlayerController::currentTrackChanged, m_library,
                     &UnifiedMusicLibrary::setActivePlaybackTrack);
    QObject::connect(m_playerController, &PlayerController::playStateChanged, m_library,
                     [this](Player::PlayState state) {
                         if(state == Player::PlayState::Stopped) {
                             m_library->flushPendingWrites();
                         }
                     });
    QObject::connect(m_libraryManager, &LibraryManager::libraryAboutToBeRemoved, m_playlistHandler,
                     &PlaylistHandler::savePlaylists);
    QObject::connect(m_engine.get(), &EngineHandler::trackChanged, this, [this](const Track& track) {
        auto currentTrack = m_playerController->currentPlaylistTrack();
        if(!sameTrackIdentity(track, currentTrack.track)) {
            return;
        }

        m_playerController->updateCurrentTrack(track);
    });
    QObject::connect(m_engine.get(), &EngineController::trackStatusContextChanged, this,
                     [this](const Engine::TrackStatusContext& context) {
                         if(context.status == Engine::TrackStatus::Invalid) {
                             m_playerController->pause();
                         }
                     });
}

Application::~Application() = default;

void Application::startup()
{
    initialise();
}

void Application::shutdown()
{
    saveDatabaseSettings();

    if(m_settings->fileValue(Settings::Core::Internal::AutoExportPlaylists).toBool()) {
        exportAllPlaylists(true);
    }

    if(m_settings->value<Settings::Core::ClearPlaybackQueueOnExit>()) {
        m_playbackQueueStore.save(PlaybackQueue{});
    }
    else {
        m_playbackQueueStore.save(m_playerController->playbackQueue());
    }

    m_playlistHandler->savePlaylists();
    m_pluginManager->unloadPlugins();
    m_audioLoader->saveState();
    m_settings->storeSettings();
    m_library->flushPendingWrites();
    m_library->cleanupTracks();
}

void Application::quit()
{
    QMetaObject::invokeMethod(QCoreApplication::instance(), []() { QCoreApplication::quit(); }, Qt::QueuedConnection);
}

void Application::restart()
{
    QMetaObject::invokeMethod(
        QCoreApplication::instance(),
        []() {
            const QString appPath = QCoreApplication::applicationFilePath();
            QCoreApplication::quit();
            QProcess::startDetached(appPath, {u"--skip-single"_s});
        },
        Qt::QueuedConnection);
}

Database* Application::database() const
{
    return m_database;
}

DbConnectionPoolPtr Application::databasePool() const
{
    return m_database->connectionPool();
}

PluginManager* Application::pluginManager() const
{
    return m_pluginManager.get();
}

PlayerController* Application::playerController() const
{
    return m_playerController;
}

LibraryManager* Application::libraryManager() const
{
    return m_libraryManager;
}

MusicLibrary* Application::library() const
{
    return m_library;
}

PendingTrackCoverProvider* Application::pendingTrackCoverProvider() const
{
    return m_library->pendingTrackCoverProvider();
}

PlaylistHandler* Application::playlistHandler() const
{
    return m_playlistHandler;
}

SettingsManager* Application::settingsManager() const
{
    return m_settings;
}

EngineController* Application::engine() const
{
    return m_engine.get();
}

std::shared_ptr<PlaylistLoader> Application::playlistLoader() const
{
    return m_playlistLoader;
}

std::shared_ptr<AudioLoader> Application::audioLoader() const
{
    return m_audioLoader;
}

std::shared_ptr<NetworkAccessManager> Application::networkManager() const
{
    return m_networkManager;
}

std::shared_ptr<RemoteIoService> Application::remoteIoService() const
{
    return m_remoteIo;
}

SortingRegistry* Application::sortingRegistry() const
{
    return m_sortingRegistry;
}

DspRegistry* Application::dspRegistry() const
{
    return m_dspRegistry.get();
}

DspChainStore* Application::dspChainStore() const
{
    return m_dspChainStore.get();
}

void Application::timerEvent(QTimerEvent* event)
{
    if(event->timerId() == m_playlistSaveTimer.timerId()) {
        m_playlistSaveTimer.stop();
        if(m_settings->fileValue(Settings::Core::Internal::AutoExportPlaylists).toBool()) {
            exportAllPlaylists(false);
        }
        m_playlistHandler->savePlaylists();
    }
    else if(event->timerId() == m_settingsSaveTimer.timerId()) {
        if(m_settings->settingsHaveChanged()) {
            m_settings->storeSettings();
        }
    }

    QObject::timerEvent(event);
}

void Application::initialise()
{
    registerTypes();
    registerInputs();
    registerPlaylistParsers();
    setupConnections();
    loadPlugins();

    m_dspChainStore->setEngine(m_engine.get());
    m_audioLoader->restoreState();
    m_settingsSaveTimer.start(SettingsSaveInterval, this);

    m_library->loadAllTracks();
    m_engine->setup();
}

void Application::registerPlaylistParsers()
{
    m_playlistLoader->addParser(std::make_unique<CueParser>());
    m_playlistLoader->addParser(std::make_unique<M3uParser>());
    m_playlistLoader->addParser(std::make_unique<PlsParser>());
}

void Application::registerInputs()
{
    m_audioLoader->addDecoder(
        u"Archive"_s, [this]() { return std::make_unique<ArchiveDecoder>(m_audioLoader); }, -1, true);
    m_audioLoader->addReader(
        u"Archive"_s, [this]() { return std::make_unique<GeneralArchiveReader>(m_audioLoader); }, -1, true);
    m_audioLoader->addReader(u"TagLib"_s, []() { return std::make_unique<TagLibReader>(); });
    m_audioLoader->addDecoder(u"FFmpeg"_s, []() { return std::make_unique<FFmpegDecoder>(); }, 99);
    m_audioLoader->addReader(u"FFmpeg"_s, []() { return std::make_unique<FFmpegReader>(); }, 99);
}

void Application::setupConnections()
{
    QObject::connect(m_library, &MusicLibrary::tracksDeleted, this,
                     [this](const TrackList& tracks) { m_playlistHandler->handleTracksDeleted(tracks); });

    QObject::connect(m_engine.get(), &EngineController::trackStatusContextChanged, this,
                     [this](const Engine::TrackStatusContext& context) {
                         if(context.status == Engine::TrackStatus::Invalid) {
                             const Track track = m_playerController->currentTrack();
                             markTrack(track);
                         }
                     });

    QObject::connect(m_playerController, &PlayerController::currentTrackChanged, this,
                     [this](const Track& track) { markTrack(track); });
    QObject::connect(m_playerController, &PlayerController::currentTrackUpdated, m_engine.get(),
                     &EngineHandler::updateCurrentTrackMetadata);

    m_settings->subscribe<Settings::Core::Shutdown>(this, [this]() {
        const auto state = m_engine->engineState();
        if(state != Engine::PlaybackState::Stopped && state != Engine::PlaybackState::Error) {
            QObject::connect(m_engine.get(), &EngineController::finished, this, Application::quit);
            m_playerController->stop();
        }
        else {
            Application::quit();
        }
    });
}

void Application::markTrack(const Track& track) const
{
    if(!track.isValid()) {
        return;
    }

    auto updateTrack = [this, &track](bool enable) {
        Track markedTrack{track};
        markedTrack.setIsEnabled(enable);
        m_library->updateTrack(markedTrack);
    };

    if(!track.isEnabled() && track.exists()) {
        updateTrack(true);
    }
    else if(track.isEnabled() && m_settings->fileValue(Settings::Core::Internal::MarkUnavailable).toBool()
            && !track.exists()) {
        updateTrack(false);
    }
}

void Application::tracksWereUpdated(const TrackList& tracks) const
{
    const Track currentTrack = m_playerController->currentTrack();
    if(!currentTrack.isValid()) {
        return;
    }

    const auto trackIt = std::ranges::find_if(
        tracks, [&currentTrack](const Track& track) { return sameTrackIdentity(track, currentTrack); });
    if(trackIt != tracks.cend()) {
        const Track& updatedTrack{*trackIt};
        qCDebug(APP) << "Refreshing current track from library update:" << "id=" << updatedTrack.id()
                     << "path=" << updatedTrack.uniqueFilepath() << "playCount=" << updatedTrack.playCount()
                     << "rating=" << updatedTrack.rating();
        m_playerController->updateCurrentTrack(updatedTrack);
    }
}

void Application::loadPlugins()
{
    const QStringList pluginPaths{Core::pluginPaths()};
    m_pluginManager->findPlugins(pluginPaths);
    m_pluginManager->loadPlugins();

    m_pluginManager->initialisePlugins<CorePlugin>(
        [this](CorePlugin* plugin) { plugin->initialise(m_corePluginContext); });

    m_pluginManager->initialisePlugins<OutputPlugin>(
        [this](OutputPlugin* plugin) { m_engine->addOutput(plugin->name(), plugin->creator()); });

    m_pluginManager->initialisePlugins<InputPlugin>([this](InputPlugin* plugin) {
        const auto creator = plugin->inputCreator();
        if(creator.decoder) {
            m_audioLoader->addDecoder(plugin->inputName(), creator.decoder);
        }
        if(creator.reader) {
            m_audioLoader->addReader(plugin->inputName(), creator.reader);
        }
        if(creator.archiveReader) {
            m_audioLoader->addArchiveReader(plugin->inputName(), creator.archiveReader);
        }
    });

    m_pluginManager->initialisePlugins<DspPlugin>([this](DspPlugin* plugin) {
        const auto dsps = plugin->dspCreators();
        for(const auto& creator : dsps) {
            if(creator.id.isEmpty() || creator.name.isEmpty() || !creator.factory) {
                continue;
            }

            m_dspRegistry->registerDsp(creator);
        }
    });
}

void Application::startSaveTimer()
{
    m_playlistSaveTimer.start(PlaylistSaveInterval, this);
}

void Application::exportAllPlaylists(bool shutdown)
{
    using namespace Settings::Core::Internal;

    const auto ext = m_settings->fileValue(AutoExportPlaylistsType).toString();

    auto* parser = m_playlistLoader->parserForExtension(ext);
    if(!parser) {
        return;
    }

    const QString path = m_settings->fileValue(AutoExportPlaylistsPath, Core::playlistsPath()).toString();
    const QDir playlistPath{path};
    const auto pathType = static_cast<PlaylistParser::PathType>(
        m_settings->fileValue(Settings::Core::Internal::PlaylistSavePathType, 0).toInt());
    const bool writeMetadata = m_settings->fileValue(Settings::Core::Internal::PlaylistSaveMetadata, false).toBool();

    enum ExportType : uint8_t
    {
        Save,
        SaveAndDeleteEmpty,
        ForceDelete
    };

    auto saveOrDeletePlaylist = [&](Playlist* playlist, ExportType exportType) {
        const QString playlistFilepath = playlistPath.absoluteFilePath(playlist->name() + u'.' + ext);
        playlistPath.mkpath(path);

        QFile playlistFile{playlistFilepath};
        const bool emptyPlaylist = playlist->trackCount() == 0;
        if(exportType == ForceDelete || (exportType == SaveAndDeleteEmpty && emptyPlaylist)) {
            if(playlistFile.exists() && !playlistFile.moveToTrash()) {
                qCInfo(APP) << "Could not remove" << (emptyPlaylist ? "empty" : "")
                            << "playlist:" << playlistFile.errorString();
            }
            return;
        }

        if(!playlistFile.open(QIODevice::WriteOnly)) {
            qCWarning(APP) << "Could not open playlist file" << playlistPath
                           << "for writing:" << playlistFile.errorString();
            return;
        }

        const QFileInfo info{playlistFile};
        const QDir playlistDir{info.absolutePath()};
        parser->savePlaylist(&playlistFile, ext, playlist->tracks(), playlistDir, pathType, writeMetadata);
    };

    auto playlists       = m_playlistHandler->playlists();
    const auto canDelete = shutdown && m_settings->fileValue(AutoExportPlaylistsRemove, true).toBool();

    for(const auto& playlist : playlists) {
        if(playlist->tracksModified() || (canDelete && playlist->trackCount() == 0)) {
            saveOrDeletePlaylist(playlist, canDelete ? SaveAndDeleteEmpty : Save);
        }
    }

    const auto saveRemoved = shutdown && m_settings->fileValue(AutoExportPlaylistsSaveRemoved, false).toBool();
    if(canDelete || saveRemoved) {
        for(const auto& playlist : m_playlistHandler->pendingRemovedPlaylists()) {
            saveOrDeletePlaylist(playlist, canDelete ? ForceDelete : Save);
        }
    }
}

void Application::loadDatabaseSettings() const
{
    const DbConnectionProvider dbProvider{m_database->connectionPool()};
    SettingsDatabase settingsDb;
    settingsDb.initialise(dbProvider);

    const QString version = settingsDb.value(u"Version"_s, {});
    if(!version.isEmpty()) {
        m_settings->set<Settings::Core::Version>(version);
    }
}

void Application::saveDatabaseSettings() const
{
    const DbConnectionProvider dbProvider{m_database->connectionPool()};
    SettingsDatabase settingsDb;
    settingsDb.initialise(dbProvider);

    settingsDb.set(u"Version"_s, QString::fromLatin1(VERSION));
}
} // namespace Fooyin

#include "moc_application.cpp"
