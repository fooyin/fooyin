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

#include <core/actions/actioncontainer.h>
#include <core/actions/actionmanager.h>
#include <core/app/threadmanager.h>
#include <core/constants.h>
#include <core/coresettings.h>
#include <core/database/database.h>
#include <core/engine/enginehandler.h>
#include <core/library/librarymanager.h>
#include <core/library/libraryscanner.h>
#include <core/library/musiclibrary.h>
#include <core/player/playercontroller.h>
#include <core/playlist/libraryplaylistmanager.h>
#include <core/playlist/playlisthandler.h>
#include <core/plugins/databaseplugin.h>
#include <core/plugins/pluginmanager.h>
#include <core/plugins/settingsplugin.h>
#include <core/plugins/threadplugin.h>
#include <core/plugins/widgetplugin.h>

#include <gui/controls/controlwidget.h>
#include <gui/editablelayout.h>
#include <gui/guisettings.h>
#include <gui/info/infowidget.h>
#include <gui/layoutprovider.h>
#include <gui/library/coverwidget.h>
#include <gui/library/statuswidget.h>
#include <gui/mainwindow.h>
#include <gui/playlist/playlistwidget.h>
#include <gui/settings/settingsdialog.h>
#include <gui/widgetfactory.h>
#include <gui/widgetprovider.h>
#include <gui/widgets/spacer.h>
#include <gui/widgets/splitterwidget.h>

#include <utils/paths.h>

struct Application::Private
{
    Core::ActionManager actionManager;
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
    Gui::Settings::SettingsDialog* settingsDialog;
    Gui::Widgets::EditableLayout* editableLayout;
    Gui::LayoutProvider layoutProvider;
    Gui::MainWindow* mainWindow;

    QAction* quitAction;
    QAction* openSettings;
    QAction* rescanLibrary;

    Plugins::PluginManager pluginManager;
    WidgetPluginContext widgetContext;
    ThreadPluginContext threadContext;
    DatabasePluginContext databaseContext;
    SettingsPluginContext settingsContext;

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
        , settingsDialog{new Gui::Settings::SettingsDialog(&libraryManager, &settingsManager)}
        , editableLayout{new Gui::Widgets::EditableLayout(&settingsManager, &actionManager, &widgetFactory,
                                                          &widgetProvider)}
        , mainWindow{new Gui::MainWindow(&actionManager, &settingsManager, settingsDialog, &layoutProvider,
                                         editableLayout)}
        , widgetContext{&actionManager, playerManager.get(), &library, &widgetFactory}
        , threadContext{&threadManager}
        , databaseContext{&database}
        , settingsContext{&settingsManager, settingsDialog}
    {
        actionManager.setMainWindow(mainWindow);
        mainWindow->setAttribute(Qt::WA_DeleteOnClose);
        threadManager.moveToNewThread(&engine);

        setupConnections();
        registerLayouts();
        registerWidgets();
        registerActions();

        const QString pluginsPath = QCoreApplication::applicationDirPath() + "/../lib/fooyin/plugins";
        pluginManager.findPlugins(pluginsPath);
        pluginManager.loadPlugins();
        initialisePlugins();
    }

    void setupConnections() const
    {
        connect(&libraryManager, &Core::Library::LibraryManager::libraryAdded, &library,
                &Core::Library::MusicLibrary::reload);
        connect(&libraryManager, &Core::Library::LibraryManager::libraryRemoved, &library,
                &Core::Library::MusicLibrary::refresh);
    }

    void registerLayouts()
    {
        layoutProvider.registerLayout("Empty", "{\"Layout\":[{\"SplitterVertical\":{\"Children\":[],\"State\":\"AAAA/"
                                               "wAAAAEAAAABAAACLwD/////AQAAAAIA\"}}]}");

        layoutProvider.registerLayout(
            "Simple", "{\"Layout\":[{\"SplitterVertical\":{\"Children\":[\"Status\",\"Playlist\",\"Controls\"],"
                      "\"State\":\"AAAA/wAAAAEAAAAEAAAAGQAAA94AAAAUAAAAAAD/////AQAAAAIA\"}}]}");

        layoutProvider.registerLayout(
            "Stone", "{\"Layout\":[{\"SplitterVertical\":{\"Children\":[\"Status\",\"Search\",{\"SplitterHorizontal\":{"
                     "\"Children\":[\"FilterAlbumArtist\",\"Playlist\"],\"State\":\"AAAA/wAAAAEAAAADAAAA/wAABlEAAAAAAP/"
                     "///8BAAAAAQA=\"}},\"Controls\"],\"State\":\"AAAA/wAAAAEAAAAFAAAAGQAAAB4AAAO8AAAAFAAAAAAA/////"
                     "wEAAAACAA==\"}}]}");

        layoutProvider.registerLayout(
            "Vision", "{\"Layout\":[{\"SplitterVertical\":{\"Children\":[\"Status\",{\"SplitterHorizontal\":{"
                      "\"Children\":[\"Controls\",\"Search\"],\"State\":\"AAAA/wAAAAEAAAADAAAD1wAAA3kAAAAAAP////"
                      "8BAAAAAQA=\"}},{\"SplitterHorizontal\":{\"Children\":[\"Artwork\",\"Playlist\"],\"State\":"
                      "\"AAAA/wAAAAEAAAADAAAD2AAAA3gAAAAAAP////8BAAAAAQA=\"}}],\"State\":\"AAAA/"
                      "wAAAAEAAAAEAAAAGQAAAB4AAAPUAAAAFAD/////AQAAAAIA\"}}]}");

        layoutProvider.registerLayout(
            "Ember",
            "{\"Layout\":[{\"SplitterVertical\":{\"Children\":[{\"SplitterHorizontal\":{\"Children\":[\"FilterGenre\","
            "\"FilterAlbumArtist\",\"FilterArtist\",\"FilterAlbum\"],\"State\":\"AAAA/"
            "wAAAAEAAAAFAAABAAAAAQAAAAEAAAABAAAAALUA/////"
            "wEAAAABAA==\"}},{\"SplitterHorizontal\":{\"Children\":[\"Controls\",\"Search\"],\"State\":\"AAAA/"
            "wAAAAEAAAADAAAFfgAAAdIAAAAAAP////"
            "8BAAAAAQA=\"}},{\"SplitterHorizontal\":{\"Children\":[{\"SplitterVertical\":{\"Children\":[\"Artwork\","
            "\"Info\"],\"State\":\"AAAA/wAAAAEAAAADAAABzAAAAbcAAAAAAP////8BAAAAAgA=\"}},\"Playlist\"],\"State\":\"AAAA/"
            "wAAAAEAAAADAAABdQAABdsAAAAAAP////8BAAAAAQA=\"}},\"Status\"],\"State\":\"AAAA/"
            "wAAAAEAAAAFAAAA+gAAAB4AAALWAAAAGQAAAAAA/////wEAAAACAA==\"}}]}");
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
            return new Gui::Widgets::PlaylistWidget(&libraryManager, &library, playerManager.get(), settingsDialog,
                                                    &settingsManager);
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
        const auto quitIcon = QIcon(Core::Constants::Icons::Quit);
        quitAction          = new QAction(quitIcon, tr("E&xit"), mainWindow);
        actionManager.registerAction(quitAction, Core::Constants::Actions::Exit);
        auto* fileMenu = actionManager.actionContainer(Core::Constants::Menus::File);
        fileMenu->addAction(quitAction, Core::Constants::Groups::Three);
        connect(quitAction, &QAction::triggered, mainWindow, &QMainWindow::close);

        const QIcon settingsIcon = QIcon(Core::Constants::Icons::Settings);
        openSettings             = new QAction(settingsIcon, tr("&Settings"), mainWindow);
        actionManager.registerAction(openSettings, Core::Constants::Actions::Settings);
        auto* libraryMenu = actionManager.actionContainer(Core::Constants::Menus::Library);
        libraryMenu->addAction(openSettings, Core::Constants::Groups::Three);
        connect(openSettings, &QAction::triggered, settingsDialog, &Gui::Settings::SettingsDialog::exec);

        const QIcon rescanIcon = QIcon(Core::Constants::Icons::RescanLibrary);
        rescanLibrary          = new QAction(rescanIcon, tr("&Rescan Library"), mainWindow);
        actionManager.registerAction(rescanLibrary, Core::Constants::Actions::Rescan);
        libraryMenu->addAction(rescanLibrary, Core::Constants::Groups::Two);
        connect(rescanLibrary, &QAction::triggered, &library, &Core::Library::MusicLibrary::reloadAll);
    }

    void initialisePlugins()
    {
        pluginManager.initialisePlugins<Plugins::WidgetPlugin>(widgetContext);
        pluginManager.initialisePlugins<Plugins::SettingsPlugin>(settingsContext);
        pluginManager.initialisePlugins<Plugins::ThreadPlugin>(threadContext);
        pluginManager.initialisePlugins<Plugins::DatabasePlugin>(databaseContext);
        pluginManager.initialisePlugins();
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

void Application::startup()
{
    p->settingsManager.loadSettings();
    p->playerManager->restoreState();
    p->layoutProvider.findLayouts();
    p->library.load();

    p->settingsDialog->setupUi();
    p->mainWindow->setupUi();
    p->mainWindow->show();
}

Application::~Application() = default;

void Application::shutdown()
{
    p->threadManager.close();
    p->pluginManager.shutdown();
    p->settingsManager.storeSettings();
    p->database.cleanup();
    p->database.closeDatabase();
}
