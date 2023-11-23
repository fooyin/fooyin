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

#include "application.h"

#include "database/database.h"
#include "engine/output/alsaoutput.h"
#include "library/unifiedmusiclibrary.h"
#include "player/playercontroller.h"
#include "playlist/playlisthandler.h"

#include <core/corepaths.h>
#include <core/coresettings.h>
#include <core/engine/enginehandler.h>
#include <core/engine/outputplugin.h>
#include <core/library/librarymanager.h>
#include <core/library/sortingregistry.h>
#include <core/plugins/coreplugin.h>
#include <core/plugins/pluginmanager.h>
#include <utils/settings/settingsmanager.h>

namespace Fooyin {
struct Application::Private
{
    SettingsManager* settingsManager;
    CoreSettings coreSettings;
    Database database;
    PlayerManager* playerManager;
    EngineHandler engine;
    LibraryManager* libraryManager;
    MusicLibrary* library;
    PlaylistHandler* playlistHandler;

    PluginManager pluginManager;
    CorePluginContext corePluginContext;

    explicit Private(QObject* parent)
        : settingsManager{new SettingsManager(Core::settingsPath(), parent)}
        , coreSettings{settingsManager}
        , database{settingsManager}
        , playerManager{new PlayerController(settingsManager, parent)}
        , engine{playerManager, settingsManager}
        , libraryManager{new LibraryManager(&database, settingsManager, parent)}
        , library{new UnifiedMusicLibrary(libraryManager, &database, settingsManager, parent)}
        , playlistHandler{new PlaylistHandler(&database, playerManager, settingsManager, parent)}
        , corePluginContext{&pluginManager, &engine,         playerManager,   libraryManager,
                            library,        playlistHandler, settingsManager, &coreSettings}
    {
        registerOutputs();
        loadPlugins();
    }

    void registerOutputs()
    {
        engine.addOutput({.name = "ALSA", .creator = []() {
                              return std::make_unique<AlsaOutput>();
                          }});
    }

    void loadPlugins()
    {
        const QStringList pluginPaths{Core::pluginsPath(), Core::userPluginsPath()};
        pluginManager.findPlugins(pluginPaths);
        pluginManager.loadPlugins();

        pluginManager.initialisePlugins<CorePlugin>(
            [this](CorePlugin* plugin) { plugin->initialise(corePluginContext); });

        pluginManager.initialisePlugins<OutputPlugin>([this](OutputPlugin* plugin) {
            const AudioOutputBuilder builder = plugin->registerOutput();
            engine.addOutput(builder);
        });
    }
};

Application::Application(QObject* parent)
    : QObject{parent}
    , p{std::make_unique<Private>(this)}
{
    QObject::connect(p->library, &MusicLibrary::tracksLoaded, p->playlistHandler, &PlaylistHandler::populatePlaylists);
    QObject::connect(p->library, &MusicLibrary::libraryRemoved, p->playlistHandler, &PlaylistHandler::libraryRemoved);
    QObject::connect(p->library, &MusicLibrary::tracksUpdated, p->playlistHandler, &PlaylistHandler::tracksUpdated);
    QObject::connect(p->library, &MusicLibrary::tracksDeleted, p->playlistHandler, &PlaylistHandler::tracksRemoved);

    p->library->loadLibrary();
    p->engine.setup();
}

Application::~Application() = default;

CorePluginContext Application::context() const
{
    return p->corePluginContext;
}

void Application::shutdown()
{
    p->engine.shutdown();
    p->playlistHandler->savePlaylists();
    p->pluginManager.shutdown();
    p->database.closeDatabase();
}
} // namespace Fooyin

#include "moc_application.cpp"
