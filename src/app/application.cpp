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

#include <core/constants.h>
#include <core/corepaths.h>
#include <core/coreplugin.h>
#include <core/library/librarymanager.h>
#include <core/library/libraryscanner.h>
#include <core/library/musiclibrary.h>
#include <core/player/playercontroller.h>
#include <core/playlist/libraryplaylistmanager.h>
#include <core/playlist/playlisthandler.h>
#include <core/plugins/plugin.h>
#include <core/plugins/pluginmanager.h>

#include <gui/controls/controlwidget.h>
#include <gui/guiconstants.h>
#include <gui/guiplugin.h>
#include <gui/info/infowidget.h>
#include <gui/library/coverwidget.h>
#include <gui/library/statuswidget.h>
#include <gui/mainwindow.h>
#include <gui/playlist/playlistwidget.h>
#include <gui/widgets/spacer.h>
#include <gui/widgets/splitterwidget.h>

#include <utils/actions/actioncontainer.h>
#include <utils/actions/actionmanager.h>
#include <utils/settings/settingsmanager.h>
#include <utils/threadmanager.h>

namespace Fy {
Application::Application(int& argc, char** argv, int flags)
    : QApplication{argc, argv, flags}
    , m_actionManager{new Utils::ActionManager(this)}
    , m_settingsManager{new Utils::SettingsManager(Core::settingsPath(), this)}
    , m_coreSettings{m_settingsManager}
    , m_threadManager{new Utils::ThreadManager(this)}
    , m_database{m_settingsManager}
    , m_playerManager{new Core::Player::PlayerController(m_settingsManager, this)}
    , m_engine{m_playerManager}
    , m_playlistHandler{new Core::Playlist::PlaylistHandler(m_playerManager, this)}
    , m_libraryManager{new Core::Library::LibraryManager(m_threadManager, &m_database, m_settingsManager, this)}
    , m_library{m_libraryManager->currentLibrary()}
    , m_playlistInterface{std::make_unique<Core::Playlist::LibraryPlaylistManager>(m_library, m_playlistHandler)}
    , m_guiSettings{m_settingsManager}
    , m_mainWindow{std::make_unique<Gui::MainWindow>(m_actionManager, m_playerManager, m_libraryManager,
                                                     m_settingsManager, &m_layoutProvider, &m_widgetFactory)}
    , m_libraryGeneralPage{m_libraryManager, m_settingsManager}
    , m_guiGeneralPage{m_settingsManager}
    , m_playlistGuiPage{m_settingsManager}
    , m_pluginManager{new Plugins::PluginManager(this)}
    , m_corePluginContext{m_actionManager, m_playerManager, m_library, m_settingsManager, m_threadManager, &m_database}
    , m_guiPluginContext{&m_widgetFactory}
{
    m_threadManager->moveToNewThread(&m_engine);

    // Required to ensure plugins are unloaded before main event loop quits
    QObject::connect(this, &QCoreApplication::aboutToQuit, this, &Application::shutdown);

    registerWidgets();
    loadPlugins();

    startup();
}

Application::~Application() = default;

void Application::startup()
{
    m_settingsManager->loadSettings();
    m_library->loadLibrary();
    m_layoutProvider.findLayouts();

    m_mainWindow->setupUi();

    if(m_libraryManager->hasLibrary() && m_settingsManager->value<Core::Settings::WaitForTracks>()) {
        connect(m_library, &Core::Library::MusicLibrary::allTracksLoaded, m_mainWindow.get(), &Gui::MainWindow::show);
    }
    else {
        m_mainWindow->show();
    }
}

void Application::shutdown()
{
    m_mainWindow.reset(nullptr);
    m_threadManager->shutdown();
    m_pluginManager->shutdown();
    m_settingsManager->storeSettings();
    m_database.closeDatabase();
}

void Application::registerWidgets()
{
    m_widgetFactory.registerClass<Gui::Widgets::ControlWidget>("Controls", [this]() {
        return new Gui::Widgets::ControlWidget(m_playerManager, m_settingsManager);
    });

    m_widgetFactory.registerClass<Gui::Widgets::InfoWidget>("Info", [this]() {
        return new Gui::Widgets::InfoWidget(m_playerManager, m_settingsManager);
    });

    m_widgetFactory.registerClass<Gui::Widgets::CoverWidget>("Artwork", [this]() {
        return new Gui::Widgets::CoverWidget(m_library, m_playerManager);
    });

    m_widgetFactory.registerClass<Gui::Widgets::PlaylistWidget>("Playlist", [this]() {
        return new Gui::Widgets::PlaylistWidget(
            m_libraryManager, m_playlistHandler, m_playerManager, m_settingsManager);
    });

    m_widgetFactory.registerClass<Gui::Widgets::Spacer>("Spacer", []() {
        return new Gui::Widgets::Spacer();
    });

    m_widgetFactory.registerClass<Gui::Widgets::StatusWidget>("Status", [this]() {
        return new Gui::Widgets::StatusWidget(m_playerManager);
    });
}

void Application::loadPlugins()
{
    const QString pluginsPath = QCoreApplication::applicationDirPath() + "/../lib/fooyin";
    m_pluginManager->findPlugins(pluginsPath);
    m_pluginManager->loadPlugins();

    m_pluginManager->initialisePlugins<Core::CorePlugin>(m_corePluginContext);
    m_pluginManager->initialisePlugins<Gui::GuiPlugin>(m_guiPluginContext);
}
} // namespace Fy
