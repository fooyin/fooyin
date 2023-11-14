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

namespace Fy::Core {
struct Application::Private
{
    Utils::SettingsManager* settingsManager;
    Core::Settings::CoreSettings coreSettings;
    Core::DB::Database database;
    Core::Player::PlayerManager* playerManager;
    Core::Engine::EngineHandler engine;
    Core::Library::LibraryManager* libraryManager;
    Core::Library::MusicLibrary* library;
    Core::Playlist::PlaylistHandler* playlistHandler;

    Plugins::PluginManager pluginManager;
    Core::CorePluginContext corePluginContext;

    explicit Private(QObject* parent)
        : settingsManager{new Utils::SettingsManager(Core::settingsPath(), parent)}
        , coreSettings{settingsManager}
        , database{settingsManager}
        , playerManager{new Core::Player::PlayerController(settingsManager, parent)}
        , engine{playerManager, settingsManager}
        , libraryManager{new Core::Library::LibraryManager(&database, settingsManager, parent)}
        , library{new Core::Library::UnifiedMusicLibrary(libraryManager, &database, settingsManager, parent)}
        , playlistHandler{new Core::Playlist::PlaylistHandler(&database, playerManager, settingsManager, parent)}
        , corePluginContext{&pluginManager, &engine,         playerManager,   libraryManager,
                            library,        playlistHandler, settingsManager, &coreSettings}
    {
        registerOutputs();
        loadPlugins();
    }

    void registerOutputs()
    {
        engine.addOutput({.name = "ALSA", .creator = []() {
                              return std::make_unique<Core::Engine::AlsaOutput>();
                          }});
    }

    void loadPlugins()
    {
        const QString pluginsPath = Core::pluginsPath();
        pluginManager.findPlugins(pluginsPath);
        pluginManager.loadPlugins();

        pluginManager.initialisePlugins<Core::CorePlugin>(
            [this](Core::CorePlugin* plugin) { plugin->initialise(corePluginContext); });

        pluginManager.initialisePlugins<Engine::OutputPlugin>([this](Engine::OutputPlugin* plugin) {
            const Engine::AudioOutputBuilder builder = plugin->registerOutput();
            engine.addOutput(builder);
        });
    }
};

Application::Application(QObject* parent)
    : QObject{parent}
    , p{std::make_unique<Private>(this)}
{
    QObject::connect(p->library, &Core::Library::MusicLibrary::tracksLoaded, p->playlistHandler,
                     &Core::Playlist::PlaylistHandler::populatePlaylists);
    QObject::connect(p->library, &Core::Library::MusicLibrary::libraryRemoved, p->playlistHandler,
                     &Core::Playlist::PlaylistHandler::libraryRemoved);
    QObject::connect(p->library, &Core::Library::MusicLibrary::tracksUpdated, p->playlistHandler,
                     &Core::Playlist::PlaylistHandler::tracksUpdated);
    QObject::connect(p->library, &Core::Library::MusicLibrary::tracksDeleted, p->playlistHandler,
                     &Core::Playlist::PlaylistHandler::tracksRemoved);

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
} // namespace Fy::Core

#include "moc_application.cpp"
