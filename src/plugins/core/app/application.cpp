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

#include "database/database.h"
#include "engine/enginehandler.h"
#include "gui/mainwindow.h"
#include "gui/settings/settingsdialog.h"
#include "library/librarymanager.h"
#include "library/musiclibrary.h"
#include "player/playercontroller.h"
#include "playlist/libraryplaylistmanager.h"
#include "playlist/playlisthandler.h"
#include "settings/settings.h"
#include "threadmanager.h"
#include "widgets/widgetprovider.h"

#include <PluginManager>

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
        , mainWindow(new MainWindow(widgetProvider, settingsDialog.get(), library))
    {
        threadManager->moveToNewThread(&engine);

        PluginSystem::addObject(playerManager);
        PluginSystem::addObject(libraryManager);
        PluginSystem::addObject(library);
        PluginSystem::addObject(settingsDialog.get());
        PluginSystem::addObject(widgetProvider);

        setupConnections();
        mainWindow->setAttribute(Qt::WA_DeleteOnClose);
        mainWindow->setupUi();
        mainWindow->show();
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
