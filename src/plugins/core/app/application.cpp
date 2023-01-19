/*
 * Fooyin
 * Copyright 2022, Luke Taylor <LukeT1@proton.me>
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

#include "core/actions/actionmanager.h"
#include "core/coresettings.h"
#include "core/database/database.h"
#include "core/engine/enginehandler.h"
#include "core/library/librarymanager.h"
#include "core/library/musiclibrary.h"
#include "core/player/playercontroller.h"
#include "core/playlist/libraryplaylistmanager.h"
#include "core/playlist/playlisthandler.h"
#include "threadmanager.h"

#include <pluginsystem/pluginmanager.h>

namespace Core {
struct Application::Private
{
    ActionManager* actionManager;
    SettingsManager* settings;
    std::unique_ptr<Settings::CoreSettings> coreSettings;
    ThreadManager* threadManager;
    DB::Database* db;
    Player::PlayerManager* playerManager;
    Engine::EngineHandler engine;
    Playlist::PlaylistHandler* playlistHandler;
    std::unique_ptr<Playlist::LibraryPlaylistInterface> playlistInterface;
    Library::LibraryManager* libraryManager;
    Library::MusicLibrary* library;

    explicit Private(QObject* parent)
        : actionManager(new ActionManager(parent))
        , settings(new SettingsManager(parent))
        , coreSettings(std::make_unique<Settings::CoreSettings>())
        , threadManager(new ThreadManager(parent))
        , db(DB::Database::instance())
        , playerManager(new Player::PlayerController(parent))
        , engine(playerManager)
        , playlistHandler(new Playlist::PlaylistHandler(playerManager, parent))
        , playlistInterface(std::make_unique<Playlist::LibraryPlaylistManager>(playlistHandler))
        , libraryManager(new Library::LibraryManager(parent))
        , library(new Library::MusicLibrary(playlistInterface.get(), libraryManager, threadManager, parent))
    {
        threadManager->moveToNewThread(&engine);

        addObjects();
        setupConnections();
    }

    void setupConnections() const
    {
        connect(libraryManager, &Library::LibraryManager::libraryAdded, library, &Library::MusicLibrary::reload);
        connect(libraryManager, &Library::LibraryManager::libraryRemoved, library, &Library::MusicLibrary::refresh);
    }

    void addObjects() const
    {
        PluginSystem::addObject(threadManager);
        PluginSystem::addObject(playerManager);
        PluginSystem::addObject(libraryManager);
        PluginSystem::addObject(library);
        PluginSystem::addObject(actionManager);
    }
};

Application::Application(QObject* parent)
    : QObject(parent)
    , p(std::make_unique<Private>(this))
{ }

void Application::startup()
{
    p->settings->loadSettings();
    p->playerManager->restoreState();
}

Application::~Application() = default;

void Application::shutdown()
{
    if(p->settings) {
        p->settings->storeSettings();
    }
    if(p->db) {
        p->db->cleanup();
        p->db->closeDatabase();
        p->db = nullptr;
    }
}
} // namespace Core
