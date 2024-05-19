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
#include "engine/enginehandler.h"
#include "internalcoresettings.h"
#include "library/librarymanager.h"
#include "library/unifiedmusiclibrary.h"
#include "plugins/pluginmanager.h"
#include "translations.h"

#include <core/engine/outputplugin.h>
#include <core/player/playercontroller.h>
#include <core/playlist/playlisthandler.h>
#include <core/plugins/coreplugin.h>
#include <utils/settings/settingsmanager.h>

#include <QCoreApplication>
#include <QProcess>

constexpr auto LastPlaybackPosition = "Player/LastPositon";
constexpr auto LastPlaybackState    = "Player/LastState";

namespace Fooyin {
struct Application::Private
{
    SettingsManager* settingsManager;
    CoreSettings coreSettings;
    Translations translations;
    Database* database;
    PlayerController* playerController;
    EngineHandler engine;
    LibraryManager* libraryManager;
    UnifiedMusicLibrary* library;
    PlaylistHandler* playlistHandler;

    PluginManager pluginManager;
    CorePluginContext corePluginContext;

    explicit Private(QObject* parent)
        : settingsManager{new SettingsManager(Core::settingsPath(), parent)}
        , coreSettings{settingsManager}
        , translations{settingsManager}
        , database{new Database(parent)}
        , playerController{new PlayerController(settingsManager, parent)}
        , engine{playerController, settingsManager}
        , libraryManager{new LibraryManager(database->connectionPool(), settingsManager, parent)}
        , library{new UnifiedMusicLibrary(libraryManager, database->connectionPool(), settingsManager, parent)}
        , playlistHandler{new PlaylistHandler(database->connectionPool(), playerController, settingsManager, parent)}
        , pluginManager{settingsManager}
        , corePluginContext{&pluginManager, &engine,         playerController, libraryManager,
                            library,        playlistHandler, settingsManager}
    {
        registerTypes();
        loadPlugins();
    }

    static void registerTypes()
    {
        qRegisterMetaType<Track>("Track");
        qRegisterMetaType<TrackList>("TrackList");
        qRegisterMetaType<TrackIds>("TrackIds");
        qRegisterMetaType<TrackIdMap>("TrackIdMap");
        qRegisterMetaType<TrackFieldMap>("TrackFieldMap");
        qRegisterMetaType<OutputCreator>("OutputCreator");
        qRegisterMetaType<LibraryInfo>("LibraryInfo");
        qRegisterMetaType<LibraryInfoMap>("LibraryInfoMap");
    }

    void loadPlugins()
    {
        const QStringList pluginPaths{Core::pluginPaths()};
        pluginManager.findPlugins(pluginPaths);
        pluginManager.loadPlugins();

        pluginManager.initialisePlugins<CorePlugin>(
            [this](CorePlugin* plugin) { plugin->initialise(corePluginContext); });

        pluginManager.initialisePlugins<OutputPlugin>([this](OutputPlugin* plugin) {
            const AudioOutputBuilder builder = plugin->registerOutput();
            engine.addOutput(builder);
        });
    }

    void savePlaybackState() const
    {
        if(settingsManager->value<Settings::Core::Internal::SavePlaybackState>()) {
            const auto lastPos = static_cast<quint64>(playerController->currentPosition());

            settingsManager->fileSet(QString::fromLatin1(LastPlaybackPosition), lastPos);
            settingsManager->fileSet(QString::fromLatin1(LastPlaybackState),
                                     static_cast<int>(playerController->playState()));
        }
        else {
            settingsManager->fileRemove(QString::fromLatin1(LastPlaybackPosition));
            settingsManager->fileRemove(QString::fromLatin1(LastPlaybackState));
        }
    }

    void loadPlaybackState() const
    {
        if(!settingsManager->value<Settings::Core::Internal::SavePlaybackState>()) {
            return;
        }

        const auto lastPos = settingsManager->fileValue(QString::fromLatin1(LastPlaybackPosition)).value<uint64_t>();
        const auto state   = settingsManager->fileValue(QString::fromLatin1(LastPlaybackState)).value<PlayState>();

        switch(state) {
            case PlayState::Paused:
                playerController->pause();
                break;
            case PlayState::Playing:
                playerController->play();
                break;
            case PlayState::Stopped:
                break;
        }

        playerController->seek(lastPos);
    }
};

Application::Application(QObject* parent)
    : QObject{parent}
    , p{std::make_unique<Private>(this)}
{
    QObject::connect(p->playlistHandler, &PlaylistHandler::playlistsPopulated, this,
                     [this]() { p->loadPlaybackState(); });
    QObject::connect(p->playerController, &PlayerController::trackPlayed, p->library,
                     &UnifiedMusicLibrary::trackWasPlayed);
    QObject::connect(p->library, &MusicLibrary::tracksLoaded, p->playlistHandler, &PlaylistHandler::populatePlaylists);
    QObject::connect(p->libraryManager, &LibraryManager::removingLibraryTracks, p->playlistHandler,
                     &PlaylistHandler::savePlaylists);
    QObject::connect(p->library, &MusicLibrary::tracksUpdated, p->playlistHandler,
                     [this](const TrackList& tracks) { p->playlistHandler->tracksUpdated(tracks); });
    QObject::connect(p->library, &MusicLibrary::tracksPlayed, p->playlistHandler,
                     [this](const TrackList& tracks) { p->playlistHandler->tracksPlayed(tracks); });
    QObject::connect(&p->engine, &EngineHandler::trackAboutToFinish, p->playlistHandler,
                     &PlaylistHandler::trackAboutToFinish);

    p->library->loadAllTracks();
    p->engine.setup();
}

Application::~Application() = default;

CorePluginContext Application::context() const
{
    return p->corePluginContext;
}

void Application::shutdown()
{
    p->savePlaybackState();
    p->playlistHandler->savePlaylists();
    p->coreSettings.shutdown();
    p->pluginManager.shutdown();
    p->settingsManager->storeSettings();
    p->library->cleanupTracks();
}

void Application::quit()
{
    QMetaObject::invokeMethod(
        QCoreApplication::instance(), []() { QCoreApplication::quit(); }, Qt::QueuedConnection);
}

void Application::restart()
{
    QMetaObject::invokeMethod(
        QCoreApplication::instance(),
        []() {
            const QString appPath = QCoreApplication::applicationFilePath();
            QCoreApplication::quit();
            QProcess::startDetached(appPath, {QStringLiteral("-s")});
        },
        Qt::QueuedConnection);
}
} // namespace Fooyin

#include "moc_application.cpp"
