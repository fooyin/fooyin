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

#include <core/app/threadmanager.h>
#include <core/constants.h>
#include <core/coreplugin.h>
#include <core/coresettings.h>
#include <core/database/database.h>
#include <core/engine/enginehandler.h>
#include <core/library/librarymanager.h>
#include <core/library/libraryscanner.h>
#include <core/library/musiclibrary.h>
#include <core/player/playercontroller.h>
#include <core/playlist/libraryplaylistmanager.h>
#include <core/playlist/playlisthandler.h>
#include <core/plugins/plugin.h>
#include <core/plugins/pluginmanager.h>

#include <gui/controls/controlwidget.h>
#include <gui/editablelayout.h>
#include <gui/guiconstants.h>
#include <gui/guiplugin.h>
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
#include <gui/widgetfactory.h>
#include <gui/widgetprovider.h>
#include <gui/widgets/spacer.h>
#include <gui/widgets/splitterwidget.h>

#include <utils/actions/actioncontainer.h>
#include <utils/actions/actionmanager.h>
#include <utils/paths.h>
#include <utils/settings/settingsdialogcontroller.h>

struct Application::Private
{
    Utils::ActionManager actionManager;
    Core::SettingsManager settingsManager;
    Core::Settings::CoreSettings coreSettings;
    Core::ThreadManager threadManager;
    Core::DB::Database database;
    std::unique_ptr<Core::Player::PlayerManager> playerManager;
    Core::Engine::EngineHandler engine;
    Core::Playlist::PlaylistHandler playlistHandler;
    std::unique_ptr<Core::Playlist::LibraryPlaylistInterface> playlistInterface;
    Core::Library::LibraryManager libraryManager;
    Core::Library::MusicLibrary library;

    Gui::Widgets::WidgetFactory widgetFactory;
    Gui::Widgets::WidgetProvider widgetProvider;
    Gui::Settings::GuiSettings guiSettings;
    Gui::Widgets::EditableLayout* editableLayout;
    Gui::LayoutProvider layoutProvider;
    Gui::MainWindow* mainWindow;

    Utils::SettingsDialogController settingsDialogController;

    //    Gui::Settings::GeneralPage generalPage;
    Gui::Settings::LibraryGeneralPage libraryGeneralPage;
    Gui::Settings::GuiGeneralPage guiGeneralPage;
    Gui::Settings::PlaylistGuiPage playlistGuiPage;

    QAction* quitAction;
    QAction* openSettings;
    QAction* rescanLibrary;

    Plugins::PluginManager pluginManager;
    CorePluginContext corePluginContext;
    GuiPluginContext guiPluginContext;

    explicit Private()
        : coreSettings{&settingsManager}
        , database{&settingsManager}
        , playerManager{std::make_unique<Core::Player::PlayerController>(&settingsManager)}
        , engine{playerManager.get()}
        , playlistHandler{playerManager.get()}
        , playlistInterface{std::make_unique<Core::Playlist::LibraryPlaylistManager>(&playlistHandler)}
        , libraryManager{&database}
        , library{playlistInterface.get(), &libraryManager, &threadManager, &database, &settingsManager}
        , widgetProvider{&widgetFactory}
        , guiSettings{&settingsManager}
        , editableLayout{new Gui::Widgets::EditableLayout(&settingsManager, &actionManager, &widgetFactory,
                                                          &widgetProvider, &layoutProvider)}
        , mainWindow{new Gui::MainWindow(&actionManager, &settingsManager, &layoutProvider, editableLayout)}
        , libraryGeneralPage{&settingsDialogController, &libraryManager, &settingsManager}
        , guiGeneralPage{&settingsDialogController, &settingsManager}
        , playlistGuiPage{&settingsDialogController, &settingsManager}
        , corePluginContext{&actionManager,   playerManager.get(),       &library,
                            &settingsManager, &settingsDialogController, &threadManager,
                            &database}
        , guiPluginContext{&widgetFactory}
    {
        actionManager.setMainWindow(mainWindow);
        mainWindow->setAttribute(Qt::WA_DeleteOnClose);
        threadManager.moveToNewThread(&engine);

        mainWindow->setupMenu();

        setupConnections();
        registerWidgets();
        registerActions();
        loadPlugins();
    }

    void setupConnections() const
    {
        connect(&libraryManager, &Core::Library::LibraryManager::libraryAdded, &library,
                &Core::Library::MusicLibrary::reload);
        connect(&libraryManager, &Core::Library::LibraryManager::libraryRemoved, &library,
                &Core::Library::MusicLibrary::refresh);
    }

    void registerWidgets()
    {
        widgetFactory.registerClass<Gui::Widgets::ControlWidget>("Controls", [this]() {
            return new Gui::Widgets::ControlWidget(playerManager.get(), &settingsManager);
        });

        widgetFactory.registerClass<Gui::Widgets::InfoWidget>("Info", [this]() {
            return new Gui::Widgets::InfoWidget(playerManager.get(), &settingsManager);
        });

        widgetFactory.registerClass<Gui::Widgets::CoverWidget>("Artwork", [this]() {
            return new Gui::Widgets::CoverWidget(&library, playerManager.get());
        });

        widgetFactory.registerClass<Gui::Widgets::PlaylistWidget>("Playlist", [this]() {
            return new Gui::Widgets::PlaylistWidget(&libraryManager, &library, playerManager.get(),
                                                    &settingsDialogController, &settingsManager);
        });

        widgetFactory.registerClass<Gui::Widgets::Spacer>("Spacer", []() {
            return new Gui::Widgets::Spacer();
        });

        widgetFactory.registerClass<Gui::Widgets::VerticalSplitterWidget>(
            "SplitterVertical",
            [this]() {
                return new Gui::Widgets::VerticalSplitterWidget(&actionManager, &widgetProvider, &settingsManager);
            },
            "Vertical Splitter", {"Splitter"});

        widgetFactory.registerClass<Gui::Widgets::HorizontalSplitterWidget>(
            "SplitterHorizontal",
            [this]() {
                return new Gui::Widgets::HorizontalSplitterWidget(&actionManager, &widgetProvider, &settingsManager);
            },
            "Horizontal Splitter", {"Splitter"});

        widgetFactory.registerClass<Gui::Widgets::StatusWidget>("Status", [this]() {
            return new Gui::Widgets::StatusWidget(playerManager.get());
        });
    }

    void registerActions()
    {
        auto* fileMenu = actionManager.actionContainer(Gui::Constants::Menus::File);

        if(fileMenu) {
            const auto quitIcon = QIcon(Gui::Constants::Icons::Quit);
            quitAction          = new QAction(quitIcon, tr("E&xit"), mainWindow);
            actionManager.registerAction(quitAction, Gui::Constants::Actions::Exit);
            fileMenu->addAction(quitAction, Gui::Constants::Groups::Three);
            connect(quitAction, &QAction::triggered, mainWindow, &QMainWindow::close);
        }

        auto* libraryMenu = actionManager.actionContainer(Gui::Constants::Menus::Library);

        if(libraryMenu) {
            const QIcon settingsIcon = QIcon(Gui::Constants::Icons::Settings);
            openSettings             = new QAction(settingsIcon, tr("&Settings"), mainWindow);
            actionManager.registerAction(openSettings, Gui::Constants::Actions::Settings);
            libraryMenu->addAction(openSettings, Gui::Constants::Groups::Three);
            connect(openSettings, &QAction::triggered, &settingsDialogController,
                    &Utils::SettingsDialogController::open);
            const QIcon rescanIcon = QIcon(Gui::Constants::Icons::RescanLibrary);
            rescanLibrary          = new QAction(rescanIcon, tr("&Rescan Library"), mainWindow);
            actionManager.registerAction(rescanLibrary, Gui::Constants::Actions::Rescan);
            libraryMenu->addAction(rescanLibrary, Gui::Constants::Groups::Two);
            connect(rescanLibrary, &QAction::triggered, &library, &Core::Library::MusicLibrary::reloadAll);
        }
    }

    void loadPlugins()
    {
        const QString pluginsPath = QCoreApplication::applicationDirPath() + "/../lib/fooyin/plugins";
        pluginManager.findPlugins(pluginsPath);
        pluginManager.loadPlugins();

        pluginManager.initialisePlugins<Core::CorePlugin>(corePluginContext);
        pluginManager.initialisePlugins<Gui::GuiPlugin>(guiPluginContext);
    }
};

Application::Application(int& argc, char** argv, int flags)
    : QApplication{argc, argv, flags}
    , p{std::make_unique<Private>()}
{
    // Required to ensure plugins are unloaded before main event loop quits
    QObject::connect(this, &QCoreApplication::aboutToQuit, this, &Application::shutdown);

    startup();
}

Application::~Application() = default;

void Application::startup()
{
    p->settingsManager.loadSettings();
    p->playerManager->restoreState();
    p->layoutProvider.findLayouts();
    p->library.load();

    p->mainWindow->setupUi();
    p->mainWindow->show();
}

void Application::shutdown()
{
    p->threadManager.close();
    p->pluginManager.shutdown();
    p->settingsManager.storeSettings();
    p->database.cleanup();
    p->database.closeDatabase();
}
