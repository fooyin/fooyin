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

#include "core/engine/enginehandler.h"
#include "core/library/librarymanager.h"
#include "core/library/musiclibrary.h"
#include "core/player/playercontroller.h"
#include "core/playlist/libraryplaylistmanager.h"
#include "core/playlist/playlisthandler.h"
#include "database/database.h"
#include "gui/mainwindow.h"
#include "utils/paths.h"
#include "utils/settings.h"
#include "utils/widgetprovider.h"

struct Application::Private
{
    DB::Database* db;
    Settings* settings;
    PlayerManager* playerManager;
    EngineHandler* engine;
    Playlist::PlaylistHandler* playlistHandler;
    WidgetProvider* widgetProvider;
    MainWindow* mainWindow;
    std::unique_ptr<LibraryPlaylistInterface> playlistInterface;
    Library::LibraryManager* libraryManager;
};

Application::Application(int& argc, char** argv)
    : QApplication(argc, argv)
{
    p = std::make_unique<Private>();

    p->db = DB::Database::instance();
    p->settings = Settings::instance();
    p->playerManager = new PlayerController(this);
    p->engine = new EngineHandler(p->playerManager, this);
    p->playlistHandler = new Playlist::PlaylistHandler(p->playerManager, this);
    p->playlistInterface = std::make_unique<LibraryPlaylistManager>(p->playlistHandler);
    p->libraryManager = new Library::LibraryManager(p->playlistInterface.get(), this);
    p->widgetProvider = new WidgetProvider(p->playerManager, p->libraryManager, this);

    p->mainWindow = new MainWindow(p->libraryManager, p->libraryManager->musicLibrary(), p->widgetProvider);
    p->mainWindow->setupUi();
    p->mainWindow->setAttribute(Qt::WA_DeleteOnClose);

    p->mainWindow->show();
}

Application::~Application()
{
    p->settings->storeSettings();
    if(p->db) {
        p->db->cleanup();
        p->db->closeDatabase();
        p->db = nullptr;
    }
}
