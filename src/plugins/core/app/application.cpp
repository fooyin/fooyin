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

#include "actions/actionmanager.h"
#include "core/database/database.h"
#include "core/engine/enginehandler.h"
#include "core/gui/mainwindow.h"
#include "core/gui/settings/settingsdialog.h"
#include "core/library/librarymanager.h"
#include "core/library/musiclibrary.h"
#include "core/player/playercontroller.h"
#include "core/playlist/libraryplaylistmanager.h"
#include "core/playlist/playlisthandler.h"
#include "core/settings/settings.h"
#include "core/widgets/widgetprovider.h"
#include "threadmanager.h"

#include <pluginsystem/pluginmanager.h>

struct Application::Private
{
    Settings* settings;
    ThreadManager* threadManager;
    DB::Database* db;
    PlayerManager* playerManager;
    EngineHandler engine;
    Playlist::PlaylistHandler* playlistHandler;
    std::unique_ptr<LibraryPlaylistInterface> playlistInterface;
    Library::LibraryManager* libraryManager;
    Library::MusicLibrary* library;
    std::unique_ptr<SettingsDialog> settingsDialog;
    WidgetProvider* widgetProvider;
    ActionManager* actionManager;
    MainWindow* mainWindow;

    explicit Private(QObject* parent)
        : settings(new Settings(parent))
        , threadManager(new ThreadManager(parent))
        , db(DB::Database::instance())
        , playerManager(new PlayerController(parent))
        , engine(playerManager)
        , playlistHandler(new Playlist::PlaylistHandler(playerManager, parent))
        , playlistInterface(std::make_unique<LibraryPlaylistManager>(playlistHandler))
        , libraryManager(new Library::LibraryManager(parent))
        , library(new Library::MusicLibrary(playlistInterface.get(), libraryManager, threadManager, parent))
        , settingsDialog(std::make_unique<SettingsDialog>(libraryManager))
        , widgetProvider(new WidgetProvider(playerManager, libraryManager, library, settingsDialog.get(), parent))
        , actionManager(new ActionManager(parent))
    {
        threadManager->moveToNewThread(&engine);

        PluginSystem::addObject(playerManager);
        PluginSystem::addObject(libraryManager);
        PluginSystem::addObject(library);
        PluginSystem::addObject(settingsDialog.get());
        PluginSystem::addObject(widgetProvider);
        PluginSystem::addObject(actionManager);

        setupConnections();
    }

    void setupConnections() const
    {
        connect(libraryManager, &Library::LibraryManager::libraryAdded, library, &Library::MusicLibrary::reload);
        connect(libraryManager, &Library::LibraryManager::libraryRemoved, library, &Library::MusicLibrary::refresh);
    }
};

Application::Application(QObject* parent)
    : QObject(parent)
    , p(std::make_unique<Private>(this))
{ }

void Application::startup()
{
    p->mainWindow = new MainWindow(p->widgetProvider, p->settingsDialog.get(), p->library);
    PluginSystem::addObject(p->mainWindow);

    p->setupConnections();
    p->mainWindow->setAttribute(Qt::WA_DeleteOnClose);
    p->mainWindow->setupUi();
    p->mainWindow->show();
}

Application::~Application() = default;

void Application::shutdown()
{
    if(p->threadManager) {
        p->threadManager->close();
    }
    if(p->settings) {
        p->settings->storeSettings();
    }
    if(p->db) {
        p->db->cleanup();
        p->db->closeDatabase();
        p->db = nullptr;
    }
}
