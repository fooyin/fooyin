/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
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
#include "engine/archiveinput.h"
#include "engine/enginehandler.h"
#include "engine/ffmpeg/ffmpeginput.h"
#include "engine/taglibparser.h"
#include "internalcoresettings.h"
#include "library/librarymanager.h"
#include "library/sortingregistry.h"
#include "library/unifiedmusiclibrary.h"
#include "playlist/parsers/cueparser.h"
#include "playlist/parsers/m3uparser.h"
#include "playlist/playlistloader.h"
#include "plugins/pluginmanager.h"
#include "translationloader.h"
#include "version.h"

#include <core/coresettings.h>
#include <core/engine/audioloader.h>
#include <core/engine/outputplugin.h>
#include <core/network/networkaccessmanager.h>
#include <core/player/playercontroller.h>
#include <core/playlist/playlisthandler.h>
#include <core/plugins/coreplugin.h>
#include <utils/database/dbconnectionprovider.h>
#include <utils/enum.h>
#include <utils/settings/settingsmanager.h>

#include <QBasicTimer>
#include <QCoreApplication>
#include <QLoggingCategory>
#include <QProcess>
#include <QTimerEvent>

Q_LOGGING_CATEGORY(APP, "fy.app")

using namespace std::chrono_literals;
using namespace Qt::StringLiterals;

constexpr auto LastPlaybackPosition = "Player/LastPositon";
constexpr auto LastPlaybackState    = "Player/LastState";

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
    qRegisterMetaType<Fooyin::OutputCreator>("OutputCreator");
    qRegisterMetaType<Fooyin::LibraryInfo>("LibraryInfo");
    qRegisterMetaType<Fooyin::LibraryInfoMap>("LibraryInfoMap");
}
} // namespace

namespace Fooyin {
class ApplicationPrivate
{
public:
    explicit ApplicationPrivate(Application* self_);

    void initialise();
    void registerPlaylistParsers();
    void registerInputs();

    void setupConnections();
    void markTrack(const Track& track) const;
    void loadPlugins();

    void startSaveTimer();
    void exportAllPlaylists();

    void savePlaybackState() const;
    void loadPlaybackState() const;

    void loadDatabaseSettings() const;
    void saveDatabaseSettings() const;

    Application* m_self;

    SettingsManager* m_settings;
    CoreSettings m_coreSettings;
    TranslationLoader m_translations;
    Database* m_database;
    std::shared_ptr<AudioLoader> m_audioLoader;
    PlayerController* m_playerController;
    EngineHandler m_engine;
    LibraryManager* m_libraryManager;
    std::shared_ptr<PlaylistLoader> m_playlistLoader;
    UnifiedMusicLibrary* m_library;
    PlaylistHandler* m_playlistHandler;
    SortingRegistry* m_sortingRegistry;
    std::shared_ptr<NetworkAccessManager> m_networkManager;

    PluginManager m_pluginManager;
    CorePluginContext m_corePluginContext;

    QBasicTimer m_playlistSaveTimer;
    QBasicTimer m_settingsSaveTimer;
    QMetaObject::Connection m_trackLoadedConnection;
};

ApplicationPrivate::ApplicationPrivate(Application* self_)
    : m_self{self_}
    , m_settings{new SettingsManager(Core::settingsPath(), m_self)}
    , m_coreSettings{m_settings}
    , m_database{new Database(m_self)}
    , m_audioLoader{std::make_shared<AudioLoader>()}
    , m_playerController{new PlayerController(m_settings, m_self)}
    , m_engine{m_audioLoader, m_playerController, m_settings}
    , m_libraryManager{new LibraryManager(m_database->connectionPool(), m_settings, m_self)}
    , m_playlistLoader{std::make_shared<PlaylistLoader>()}
    , m_library{new UnifiedMusicLibrary(m_libraryManager, m_database->connectionPool(), m_playlistLoader, m_audioLoader,
                                        m_settings, m_self)}
    , m_playlistHandler{new PlaylistHandler(m_database->connectionPool(), m_audioLoader, m_playerController, m_library,
                                            m_settings, m_self)}
    , m_sortingRegistry{new SortingRegistry(m_settings, m_self)}
    , m_networkManager{new NetworkAccessManager(m_settings, m_self)}
    , m_pluginManager{m_settings}
    , m_corePluginContext{&m_engine,  m_playerController, m_libraryManager,  m_library,       m_playlistHandler,
                          m_settings, m_audioLoader,      m_sortingRegistry, m_networkManager}
{
    m_translations.initialiseTranslations(m_settings->value<Settings::Core::Language>());
    loadDatabaseSettings();
}

void ApplicationPrivate::initialise()
{
    m_playerController->setPlaylistHandler(m_playlistHandler);

    registerTypes();
    registerInputs();
    registerPlaylistParsers();
    setupConnections();
    loadPlugins();

    m_audioLoader->restoreState();
    m_settingsSaveTimer.start(SettingsSaveInterval, m_self);

    m_library->loadAllTracks();
    m_engine.setup();
}

void ApplicationPrivate::registerPlaylistParsers()
{
    m_playlistLoader->addParser(std::make_unique<CueParser>(m_audioLoader));
    m_playlistLoader->addParser(std::make_unique<M3uParser>(m_audioLoader));
}

void ApplicationPrivate::registerInputs()
{
    m_audioLoader->addDecoder(u"Archive"_s, [this]() { return std::make_unique<ArchiveDecoder>(m_audioLoader); });
    m_audioLoader->addReader(u"Archive"_s, [this]() { return std::make_unique<GeneralArchiveReader>(m_audioLoader); });
    m_audioLoader->addReader(u"TagLib"_s, {[]() {
                                 return std::make_unique<TagLibReader>();
                             }});
    m_audioLoader->addDecoder(u"FFmpeg"_s, []() { return std::make_unique<FFmpegDecoder>(); }, 99);
    m_audioLoader->addReader(u"FFmpeg"_s, {[]() {
                                 return std::make_unique<FFmpegReader>();
                             }},
                             99);
}

void ApplicationPrivate::setupConnections()
{
    QObject::connect(&m_engine, &EngineController::trackStatusChanged, m_self, [this](AudioEngine::TrackStatus status) {
        if(status == AudioEngine::TrackStatus::Invalid) {
            const Track track = m_playerController->currentTrack();
            markTrack(track);
        }
    });

    QObject::connect(m_playerController, &PlayerController::currentTrackChanged, m_self,
                     [this](const Track& track) { markTrack(track); });

    m_settings->subscribe<Settings::Core::Shutdown>(m_self, [this]() {
        savePlaybackState();

        const auto state = m_engine.engineState();
        if(state != AudioEngine::PlaybackState::Stopped && state != AudioEngine::PlaybackState::Error) {
            QObject::connect(&m_engine, &EngineController::finished, m_self, Application::quit);
            m_playerController->stop();
        }
        else {
            Application::quit();
        }
    });
}

void ApplicationPrivate::markTrack(const Track& track) const
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

void ApplicationPrivate::loadPlugins()
{
    const QStringList pluginPaths{Core::pluginPaths()};
    m_pluginManager.findPlugins(pluginPaths);
    m_pluginManager.loadPlugins();

    m_pluginManager.initialisePlugins<CorePlugin>(
        [this](CorePlugin* plugin) { plugin->initialise(m_corePluginContext); });

    m_pluginManager.initialisePlugins<OutputPlugin>(
        [this](OutputPlugin* plugin) { m_engine.addOutput(plugin->name(), plugin->creator()); });

    m_pluginManager.initialisePlugins<InputPlugin>([this](InputPlugin* plugin) {
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
}

void ApplicationPrivate::startSaveTimer()
{
    m_playlistSaveTimer.start(PlaylistSaveInterval, m_self);
}

void ApplicationPrivate::exportAllPlaylists()
{
    using namespace Settings::Core::Internal;

    const auto ext = m_settings->fileValue(AutoExportPlaylistsType).toString();

    auto* parser = m_playlistLoader->parserForExtension(ext);
    if(!parser) {
        return;
    }

    const QString path = m_settings->fileValue(AutoExportPlaylistsPath, Core::playlistsPath()).toString();
    const QDir playlistPath{path};
    const auto type          = PlaylistParser::PathType::Auto;
    const bool writeMetadata = m_settings->fileValue(Settings::Core::Internal::PlaylistSaveMetadata, false).toBool();

    auto saveOrDeletePlaylist = [&](Playlist* playlist, bool forceRemove = false) {
        const QString playlistFilepath = playlistPath.absoluteFilePath(playlist->name() + u'.' + ext);
        playlistPath.mkpath(path);

        QFile playlistFile{playlistFilepath};
        if(forceRemove || playlist->trackCount() == 0) {
            if(playlistFile.exists() && !playlistFile.remove()) {
                qCInfo(APP) << "Could not remove empty playlist:" << playlistFile.errorString();
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
        parser->savePlaylist(&playlistFile, ext, playlist->tracks(), playlistDir, type, writeMetadata);
    };

    auto playlists        = m_playlistHandler->playlists();
    auto removedPlaylists = m_playlistHandler->removedPlaylists();

    for(const auto& playlist : playlists) {
        if(playlist->tracksModified()) {
            saveOrDeletePlaylist(playlist);
        }
    }
    for(const auto& playlist : removedPlaylists) {
        saveOrDeletePlaylist(playlist, true);
    }
}

void ApplicationPrivate::savePlaybackState() const
{
    FyStateSettings stateSettings;

    if(m_settings->fileValue(Settings::Core::Internal::SavePlaybackState, false).toBool()) {
        const auto lastPos = static_cast<quint64>(m_playerController->currentPosition());

        stateSettings.setValue(LastPlaybackPosition, lastPos);
        stateSettings.setValue(LastPlaybackState, Utils::Enum::toString(m_playerController->playState()));
    }
    else {
        stateSettings.remove(LastPlaybackPosition);
        stateSettings.remove(LastPlaybackState);
    }
}

void ApplicationPrivate::loadPlaybackState() const
{
    const FyStateSettings stateSettings;

    if(!m_playerController->currentTrack().isValid()) {
        return;
    }

    if(!m_settings->fileValue(Settings::Core::Internal::SavePlaybackState, false).toBool()) {
        return;
    }

    const auto lastPos = stateSettings.value(LastPlaybackPosition).value<uint64_t>();

    auto seek = [this, lastPos]() {
        if(lastPos > 0) {
            QMetaObject::invokeMethod(
                m_playerController, [this, lastPos]() { m_playerController->seek(lastPos); }, Qt::QueuedConnection);
        }
    };

    const QString savedState = stateSettings.value(LastPlaybackState).toString();
    const auto state         = Utils::Enum::fromString<Player::PlayState>(savedState);
    if(!state) {
        return;
    }

    switch(state.value()) {
        case(Player::PlayState::Paused):
            qCDebug(APP) << "Restoring paused state…";
            QMetaObject::invokeMethod(m_playerController, &PlayerController::pause, Qt::QueuedConnection);
            seek();
            break;
        case(Player::PlayState::Playing):
            qCDebug(APP) << "Restoring playing state…";
            QMetaObject::invokeMethod(m_playerController, &PlayerController::play, Qt::QueuedConnection);
            seek();
            break;
        case(Player::PlayState::Stopped):
            qCDebug(APP) << "Restoring stopped state…";
            QMetaObject::invokeMethod(m_playerController, &PlayerController::stop, Qt::QueuedConnection);
            break;
    }
}

void ApplicationPrivate::loadDatabaseSettings() const
{
    const DbConnectionProvider dbProvider{m_database->connectionPool()};
    SettingsDatabase settingsDb;
    settingsDb.initialise(dbProvider);

    const QString version = settingsDb.value(u"Version"_s, {});
    if(!version.isEmpty()) {
        m_settings->set<Settings::Core::Version>(version);
    }
}

void ApplicationPrivate::saveDatabaseSettings() const
{
    const DbConnectionProvider dbProvider{m_database->connectionPool()};
    SettingsDatabase settingsDb;
    settingsDb.initialise(dbProvider);

    settingsDb.set(u"Version"_s, QString::fromLatin1(VERSION));
}

Application::Application(QObject* parent)
    : QObject{parent}
    , p{std::make_unique<ApplicationPrivate>(this)}
{
    auto startPlaylistTimer = [this]() {
        p->startSaveTimer();
    };

    QObject::connect(p->m_playlistHandler, &PlaylistHandler::playlistAdded, this, startPlaylistTimer);
    QObject::connect(p->m_playlistHandler, &PlaylistHandler::playlistRemoved, this, startPlaylistTimer);
    QObject::connect(p->m_playlistHandler, &PlaylistHandler::tracksAdded, this, startPlaylistTimer);
    QObject::connect(p->m_playlistHandler, &PlaylistHandler::tracksChanged, this, startPlaylistTimer);
    QObject::connect(p->m_playlistHandler, &PlaylistHandler::tracksUpdated, this, startPlaylistTimer);
    QObject::connect(p->m_playlistHandler, &PlaylistHandler::tracksRemoved, this, startPlaylistTimer);
    p->m_trackLoadedConnection = QObject::connect(&p->m_engine, &EngineHandler::trackStatusChanged, this,
                                                  [this](const AudioEngine::TrackStatus status) {
                                                      if(status == AudioEngine::TrackStatus::Loaded) {
                                                          QObject::disconnect(p->m_trackLoadedConnection);
                                                          p->loadPlaybackState();
                                                      }
                                                  });

    QObject::connect(p->m_playerController, &PlayerController::trackPlayed, p->m_library,
                     &UnifiedMusicLibrary::trackWasPlayed);
    QObject::connect(p->m_libraryManager, &LibraryManager::libraryAboutToBeRemoved, p->m_playlistHandler,
                     &PlaylistHandler::savePlaylists);
    QObject::connect(&p->m_engine, &EngineHandler::trackAboutToFinish, this, [this]() {
        p->m_playlistHandler->trackAboutToFinish();
        p->m_engine.prepareNextTrack(p->m_playerController->upcomingTrack());
    });
    QObject::connect(&p->m_engine, &EngineHandler::trackChanged, p->m_library, [this](const Track& track) {
        p->m_library->updateTrackMetadata({track});
        auto currentTrack  = p->m_playerController->currentPlaylistTrack();
        currentTrack.track = track;
        p->m_playerController->changeCurrentTrack(currentTrack);
    });
    QObject::connect(&p->m_engine, &EngineController::trackStatusChanged, this,
                     [this](AudioEngine::TrackStatus status) {
                         if(status == AudioEngine::TrackStatus::Invalid) {
                             p->m_playerController->pause();
                         }
                     });
}

void Application::startup()
{
    p->initialise();
}

Application::~Application() = default;

void Application::timerEvent(QTimerEvent* event)
{
    if(event->timerId() == p->m_playlistSaveTimer.timerId()) {
        p->m_playlistSaveTimer.stop();
        if(p->m_settings->fileValue(Settings::Core::Internal::AutoExportPlaylists).toBool()) {
            p->exportAllPlaylists();
        }
        p->m_playlistHandler->savePlaylists();
    }
    else if(event->timerId() == p->m_settingsSaveTimer.timerId()) {
        if(p->m_settings->settingsHaveChanged()) {
            p->m_settings->storeSettings();
        }
    }

    QObject::timerEvent(event);
}

void Application::shutdown()
{
    p->saveDatabaseSettings();

    if(p->m_settings->fileValue(Settings::Core::Internal::AutoExportPlaylists).toBool()) {
        p->exportAllPlaylists();
    }

    p->m_playlistHandler->savePlaylists();
    p->m_pluginManager.unloadPlugins();
    p->m_audioLoader->saveState();
    p->m_settings->storeSettings();
    p->m_library->cleanupTracks();
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
    return p->m_database;
}

DbConnectionPoolPtr Application::databasePool() const
{
    return p->m_database->connectionPool();
}

PluginManager* Application::pluginManager() const
{
    return &p->m_pluginManager;
}

PlayerController* Application::playerController() const
{
    return p->m_playerController;
}

LibraryManager* Application::libraryManager() const
{
    return p->m_libraryManager;
}

MusicLibrary* Application::library() const
{
    return p->m_library;
}

PlaylistHandler* Application::playlistHandler() const
{
    return p->m_playlistHandler;
}

SettingsManager* Application::settingsManager() const
{
    return p->m_settings;
}

EngineController* Application::engine() const
{
    return &p->m_engine;
}

std::shared_ptr<PlaylistLoader> Application::playlistLoader() const
{
    return p->m_playlistLoader;
}

std::shared_ptr<AudioLoader> Application::audioLoader() const
{
    return p->m_audioLoader;
}

std::shared_ptr<NetworkAccessManager> Application::networkManager() const
{
    return p->m_networkManager;
}

SortingRegistry* Application::sortingRegistry() const
{
    return p->m_sortingRegistry;
}
} // namespace Fooyin

#include "moc_application.cpp"
