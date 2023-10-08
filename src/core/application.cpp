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

#include "config.h"

#include "database/database.h"
#include "engine/enginehandler.h"
#include "library/unifiedmusiclibrary.h"
#include "player/playercontroller.h"
#include "playlist/playlisthandler.h"

#include <core/corepaths.h>
#include <core/coresettings.h>
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
    Core::Library::SortingRegistry sortingRegistry;
    Core::Playlist::PlaylistHandler* playlistHandler;

    Plugins::PluginManager pluginManager;
    Core::CorePluginContext corePluginContext;

    explicit Private(QObject* parent)
        : settingsManager{new Utils::SettingsManager(Core::settingsPath(), parent)}
        , coreSettings{settingsManager}
        , database{settingsManager}
        , playerManager{new Core::Player::PlayerController(settingsManager, parent)}
        , engine{playerManager}
        , libraryManager{new Core::Library::LibraryManager(&database, settingsManager, parent)}
        , library{new Core::Library::UnifiedMusicLibrary(libraryManager, &database, settingsManager, parent)}
        , sortingRegistry{settingsManager}
        , playlistHandler{new Core::Playlist::PlaylistHandler(&database, playerManager, settingsManager, parent)}
        , corePluginContext{&pluginManager,  playerManager,   libraryManager,  library,
                            playlistHandler, settingsManager, &sortingRegistry}
    {
        loadPlugins();
    }

    void loadPlugins()
    {
        const QString pluginsPath = PLUGIN_DIR;
        pluginManager.findPlugins(pluginsPath);
        pluginManager.loadPlugins();

        pluginManager.initialisePlugins<Core::CorePlugin>(corePluginContext);
    }
};

Application::Application(QObject* parent)
    : QObject{parent}
    , p{std::make_unique<Private>(this)}
{
    connect(p->library, &Core::Library::MusicLibrary::tracksLoaded, p->playlistHandler,
            &Core::Playlist::PlaylistHandler::populatePlaylists);
    connect(p->library, &Core::Library::MusicLibrary::libraryRemoved, p->playlistHandler,
            &Core::Playlist::PlaylistHandler::libraryRemoved);
    connect(p->library, &Core::Library::MusicLibrary::tracksUpdated, p->playlistHandler,
            &Core::Playlist::PlaylistHandler::tracksUpdated);
    connect(p->library, &Core::Library::MusicLibrary::tracksDeleted, p->playlistHandler,
            &Core::Playlist::PlaylistHandler::tracksRemoved);

    p->settingsManager->loadSettings();
    p->library->loadLibrary();
    p->sortingRegistry.loadItems();
}

Application::~Application() = default;

CorePluginContext Application::context() const
{
    return p->corePluginContext;
}

void Application::shutdown()
{
    p->playlistHandler->savePlaylists();
    p->sortingRegistry.saveItems();
    p->pluginManager.shutdown();
    p->settingsManager->storeSettings();
    p->database.closeDatabase();
}
} // namespace Fy::Core
