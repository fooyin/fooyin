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
#include "gui/settings/settingsdialog.h"
#include "threadmanager.h"
#include "utils/paths.h"
#include "utils/settings.h"
#include "utils/widgetprovider.h"

struct Application::Private
{
    ThreadManager* threadManager;
    DB::Database* db;
    Settings* settings;
    PlayerManager* playerManager;
    EngineHandler engine;
    Playlist::PlaylistHandler* playlistHandler;
    std::unique_ptr<LibraryPlaylistInterface> playlistInterface;
    Library::LibraryManager* libraryManager;
    Library::MusicLibrary* library;
    std::unique_ptr<SettingsDialog> settingsDialog;
    WidgetProvider* widgetProvider;
    MainWindow* mainWindow;

    Private(QObject* parent)
        : threadManager(new ThreadManager(parent))
        , db(DB::Database::instance())
        , settings(Settings::instance())
        , playerManager(new PlayerController(parent))
        , engine(playerManager)
        , playlistHandler(new Playlist::PlaylistHandler(playerManager, parent))
        , playlistInterface(std::make_unique<LibraryPlaylistManager>(playlistHandler))
        , libraryManager(new Library::LibraryManager(parent))
        , library(new Library::MusicLibrary(playlistInterface.get(), libraryManager, threadManager, parent))
        , settingsDialog(std::make_unique<SettingsDialog>(libraryManager))
        , widgetProvider(new WidgetProvider(playerManager, libraryManager, library, settingsDialog.get(), parent))
        , mainWindow(new MainWindow(widgetProvider, settingsDialog.get()))
    {
        threadManager->moveToNewThread(&engine);

        setupConnections();
        mainWindow->setAttribute(Qt::WA_DeleteOnClose);
        mainWindow->show();
    }

    void setupConnections() const
    {
        connect(libraryManager, &Library::LibraryManager::libraryAdded, library, &Library::MusicLibrary::reload);
        connect(libraryManager, &Library::LibraryManager::libraryRemoved, library, &Library::MusicLibrary::refresh);
    }
};

Application::Application(int& argc, char** argv)
    : QApplication(argc, argv)
    , p(std::make_unique<Private>(this))
{ }

Application::~Application()
{
    p->threadManager->close();
    p->settings->storeSettings();
    if(p->db) {
        p->db->cleanup();
        p->db->closeDatabase();
        p->db = nullptr;
    }
}
