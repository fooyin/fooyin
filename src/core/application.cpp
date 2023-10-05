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

#include "core/database/database.h"
#include "core/engine/enginehandler.h"
#include "core/library/librarymanager.h"
#include "core/library/sortingregistry.h"
#include "core/library/unifiedmusiclibrary.h"
#include "core/player/playercontroller.h"
#include "core/playlist/playlisthandler.h"
#include "core/plugins/pluginmanager.h"
#include "corepaths.h"
#include "coreplugin.h"
#include "coresettings.h"

#include <utils/actions/actioncontainer.h>
#include <utils/actions/actionmanager.h>
#include <utils/settings/settingsmanager.h>

namespace Fy::Core {
struct Application::Private
{
    Utils::ActionManager* actionManager;
    Utils::SettingsManager* settingsManager;
    Core::Settings::CoreSettings coreSettings;
    Core::DB::Database database;
    Core::Player::PlayerManager* playerManager;
    Core::Engine::EngineHandler engine;
    Core::Library::LibraryManager* libraryManager;
    Core::Library::MusicLibrary* library;
    Core::Library::SortingRegistry sortingRegistry;
    Core::Playlist::PlaylistHandler* playlistHandler;

    Plugins::PluginManager* pluginManager;
    Core::CorePluginContext corePluginContext;

    explicit Private(QObject* parent)
        : actionManager{new Utils::ActionManager(parent)}
        , settingsManager{new Utils::SettingsManager(Core::settingsPath(), parent)}
        , coreSettings{settingsManager}
        , database{settingsManager}
        , playerManager{new Core::Player::PlayerController(settingsManager, parent)}
        , engine{playerManager}
        , libraryManager{new Core::Library::LibraryManager(&database, settingsManager, parent)}
        , library{new Core::Library::UnifiedMusicLibrary(libraryManager, &database, settingsManager, parent)}
        , sortingRegistry{settingsManager}
        , playlistHandler{new Core::Playlist::PlaylistHandler(&database, playerManager, library, settingsManager,
                                                              parent)}
        , pluginManager{new Plugins::PluginManager(parent)}
        , corePluginContext{pluginManager,   actionManager,   playerManager, libraryManager,  library,
                            playlistHandler, settingsManager, &database,     &sortingRegistry}
    {
        loadPlugins();
    }

    void loadPlugins() const
    {
        const QString pluginsPath = QCoreApplication::applicationDirPath() + "/../lib/fooyin";
        pluginManager->findPlugins(pluginsPath);
        pluginManager->loadPlugins();

        pluginManager->initialisePlugins<Core::CorePlugin>(corePluginContext);
    }
};

Application::Application(QObject* parent)
    : QObject{parent}
    , p{std::make_unique<Private>(this)}
{
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
    p->pluginManager->shutdown();
    p->settingsManager->storeSettings();
    p->database.closeDatabase();
}
} // namespace Fy::Core
