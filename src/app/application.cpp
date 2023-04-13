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
#include <core/coreplugincontext.h>
#include <core/coresettings.h>
#include <core/database/database.h>
#include <core/engine/enginehandler.h>
#include <core/library/librarymanager.h>
#include <core/library/libraryscanner.h>
#include <core/library/unifiedmusiclibrary.h>
#include <core/player/playercontroller.h>
#include <core/playlist/libraryplaylistinterface.h>
#include <core/playlist/libraryplaylistmanager.h>
#include <core/playlist/playlisthandler.h>
#include <core/plugins/plugin.h>
#include <core/plugins/pluginmanager.h>

#include <gui/controls/controlwidget.h>
#include <gui/guiconstants.h>
#include <gui/guiplugin.h>
#include <gui/guiplugincontext.h>
#include <gui/guisettings.h>
#include <gui/info/infowidget.h>
#include <gui/layoutprovider.h>
#include <gui/library/coverwidget.h>
#include <gui/library/statuswidget.h>
#include <gui/mainwindow.h>
#include <gui/playlist/playlistwidget.h>
#include <gui/settings/generalpage.h>
#include <gui/settings/guigeneralpage.h>
#include <gui/settings/librarygeneralpage.h>
#include <gui/settings/playlistguipage.h>
#include <gui/settings/pluginspage.h>
#include <gui/widgetfactory.h>
#include <gui/widgets/spacer.h>
#include <gui/widgets/splitterwidget.h>

#include <utils/actions/actioncontainer.h>
#include <utils/actions/actionmanager.h>
#include <utils/settings/settingsmanager.h>

namespace Fy {
struct Application::Private
{
    Utils::ActionManager* actionManager;
    Utils::SettingsManager* settingsManager;
    Core::Settings::CoreSettings coreSettings;
    Core::DB::Database database;
    Core::Player::PlayerManager* playerManager;
    Core::Engine::EngineHandler engine;
    Core::Playlist::PlaylistManager* playlistHandler;
    Core::Library::LibraryManager* libraryManager;
    Core::Library::MusicLibrary* library;
    std::unique_ptr<Core::Playlist::LibraryPlaylistInterface> playlistInterface;

    Gui::Widgets::WidgetFactory widgetFactory;
    Gui::Settings::GuiSettings guiSettings;
    Gui::LayoutProvider layoutProvider;
    std::unique_ptr<Gui::MainWindow> mainWindow;

    Gui::Settings::GeneralPage generalPage;
    Gui::Settings::LibraryGeneralPage libraryGeneralPage;
    Gui::Settings::GuiGeneralPage guiGeneralPage;
    Gui::Settings::PlaylistGuiPage playlistGuiPage;

    Plugins::PluginManager* pluginManager;
    Gui::Settings::PluginPage pluginPage;
    Core::CorePluginContext corePluginContext;
    Gui::GuiPluginContext guiPluginContext;

    explicit Private(QObject* parent)
        : actionManager{new Utils::ActionManager(parent)}
        , settingsManager{new Utils::SettingsManager(Core::settingsPath(), parent)}
        , coreSettings{settingsManager}
        , database{settingsManager}
        , playerManager{new Core::Player::PlayerController(settingsManager, parent)}
        , engine{playerManager}
        , playlistHandler{new Core::Playlist::PlaylistHandler(playerManager, parent)}
        , libraryManager{new Core::Library::LibraryManager(&database, settingsManager, parent)}
        , library{new Core::Library::UnifiedMusicLibrary(libraryManager, &database, settingsManager, parent)}
        , playlistInterface{std::make_unique<Core::Playlist::LibraryPlaylistManager>(library, playlistHandler)}
        , guiSettings{settingsManager}
        , mainWindow{std::make_unique<Gui::MainWindow>(actionManager, playerManager, library, settingsManager,
                                                       &layoutProvider, &widgetFactory)}
        , generalPage{settingsManager}
        , libraryGeneralPage{libraryManager, settingsManager}
        , guiGeneralPage{settingsManager}
        , playlistGuiPage{settingsManager}
        , pluginManager{new Plugins::PluginManager(parent)}
        , pluginPage{settingsManager, pluginManager}
        , corePluginContext{actionManager, playerManager, library, settingsManager, &database}
        , guiPluginContext{&layoutProvider, &widgetFactory}
    {
        registerWidgets();
        loadPlugins();
    }

    void registerWidgets()
    {
        widgetFactory.registerClass<Gui::Widgets::ControlWidget>("Controls", [this]() {
            return new Gui::Widgets::ControlWidget(playerManager, settingsManager);
        });

        widgetFactory.registerClass<Gui::Widgets::InfoWidget>("Info", [this]() {
            return new Gui::Widgets::InfoWidget(playerManager, settingsManager);
        });

        widgetFactory.registerClass<Gui::Widgets::CoverWidget>("Artwork", [this]() {
            return new Gui::Widgets::CoverWidget(library, playerManager);
        });

        widgetFactory.registerClass<Gui::Widgets::PlaylistWidget>("Playlist", [this]() {
            return new Gui::Widgets::PlaylistWidget(library, playlistHandler, playerManager, settingsManager);
        });

        widgetFactory.registerClass<Gui::Widgets::Spacer>("Spacer", []() {
            return new Gui::Widgets::Spacer();
        });

        widgetFactory.registerClass<Gui::Widgets::StatusWidget>("Status", [this]() {
            return new Gui::Widgets::StatusWidget(library, playerManager);
        });
    }

    void loadPlugins()
    {
        const QString pluginsPath = QCoreApplication::applicationDirPath() + "/../lib/fooyin";
        pluginManager->findPlugins(pluginsPath);
        pluginManager->loadPlugins();

        pluginManager->initialisePlugins<Core::CorePlugin>(corePluginContext);
        pluginManager->initialisePlugins<Gui::GuiPlugin>(guiPluginContext);
    }
};

Application::Application(int& argc, char** argv, int flags)
    : QApplication{argc, argv, flags}
    , p{std::make_unique<Private>(this)}
{
    // Required to ensure plugins are unloaded before main event loop quits
    QObject::connect(this, &QCoreApplication::aboutToQuit, this, &Application::shutdown);

    startup();
}

Application::~Application() = default;

void Application::startup()
{
    p->settingsManager->loadSettings();
    p->library->loadLibrary();
    p->layoutProvider.findLayouts();

    p->mainWindow->setupUi();

    if(p->libraryManager->hasLibrary() && p->settingsManager->value<Core::Settings::WaitForTracks>()) {
        connect(p->library, &Core::Library::MusicLibrary::allTracksLoaded, p->mainWindow.get(), &Gui::MainWindow::show);
    }
    else {
        p->mainWindow->show();
    }
}

void Application::shutdown()
{
    p->mainWindow.reset(nullptr);
    p->pluginManager->shutdown();
    p->settingsManager->storeSettings();
    p->database.closeDatabase();
}
} // namespace Fy
