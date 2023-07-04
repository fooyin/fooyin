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
#include <core/playlist/playlisthandler.h>
#include <core/plugins/plugin.h>
#include <core/plugins/pluginmanager.h>

#include <gui/controls/controlwidget.h>
#include <gui/dialog/propertiesdialog.h>
#include <gui/editablelayout.h>
#include <gui/guiconstants.h>
#include <gui/guiplugin.h>
#include <gui/guiplugincontext.h>
#include <gui/guisettings.h>
#include <gui/info/infowidget.h>
#include <gui/layoutprovider.h>
#include <gui/library/coverwidget.h>
#include <gui/library/statuswidget.h>
#include <gui/librarytree/librarytreegroupregistry.h>
#include <gui/librarytree/librarytreewidget.h>
#include <gui/mainwindow.h>
#include <gui/menu/editmenu.h>
#include <gui/menu/filemenu.h>
#include <gui/menu/helpmenu.h>
#include <gui/menu/librarymenu.h>
#include <gui/menu/playbackmenu.h>
#include <gui/menu/viewmenu.h>
#include <gui/playlist/playlistcontroller.h>
#include <gui/playlist/playlisttabs.h>
#include <gui/playlist/playlistwidget.h>
#include <gui/playlist/presetregistry.h>
#include <gui/settings/generalpage.h>
#include <gui/settings/guigeneralpage.h>
#include <gui/settings/library/librarygeneralpage.h>
#include <gui/settings/librarytree/librarytreepage.h>
#include <gui/settings/playlist/playlistguipage.h>
#include <gui/settings/playlist/playlistpresetspage.h>
#include <gui/settings/plugins/pluginspage.h>
#include <gui/trackselectioncontroller.h>
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
    Core::Library::LibraryManager* libraryManager;
    Core::Library::MusicLibrary* library;
    Core::Playlist::PlaylistHandler* playlistHandler;

    Gui::Widgets::WidgetFactory widgetFactory;
    Gui::Settings::GuiSettings guiSettings;
    Gui::LayoutProvider layoutProvider;
    std::unique_ptr<Gui::Widgets::Playlist::PlaylistController> playlistController;
    Gui::TrackSelectionController selectionController;
    Gui::Widgets::Playlist::PresetRegistry presetRegistry;
    Gui::Widgets::LibraryTreeGroupRegistry treeGroupRegistry;
    std::unique_ptr<Gui::Widgets::EditableLayout> editableLayout;
    std::unique_ptr<Gui::MainWindow> mainWindow;

    Gui::FileMenu* fileMenu;
    Gui::EditMenu* editMenu;
    Gui::ViewMenu* viewMenu;
    Gui::PlaybackMenu* playbackMenu;
    Gui::LibraryMenu* libraryMenu;
    Gui::HelpMenu* helpMenu;

    Gui::PropertiesDialog* propertiesDialog;

    Gui::Settings::GeneralPage generalPage;
    Gui::Settings::LibraryGeneralPage libraryGeneralPage;
    Gui::Settings::GuiGeneralPage guiGeneralPage;
    Gui::Settings::PlaylistGuiPage playlistGuiPage;
    Gui::Settings::PlaylistPresetsPage playlistPresetsPage;
    Gui::Settings::LibraryTreePage libraryTreePage;

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
        , libraryManager{new Core::Library::LibraryManager(&database, settingsManager, parent)}
        , library{new Core::Library::UnifiedMusicLibrary(libraryManager, &database, settingsManager, parent)}
        , playlistHandler{new Core::Playlist::PlaylistHandler(&database, playerManager, library, settingsManager,
                                                              parent)}
        , guiSettings{settingsManager}
        , playlistController{std::make_unique<Gui::Widgets::Playlist::PlaylistController>(playlistHandler,
                                                                                          settingsManager)}
        , selectionController{actionManager, playlistController.get()}
        , presetRegistry{settingsManager}
        , treeGroupRegistry{settingsManager}
        , editableLayout{std::make_unique<Gui::Widgets::EditableLayout>(settingsManager, actionManager, &widgetFactory,
                                                                        &layoutProvider)}
        , mainWindow{std::make_unique<Gui::MainWindow>(actionManager, settingsManager, editableLayout.get())}
        , fileMenu{new Gui::FileMenu(actionManager, settingsManager, parent)}
        , editMenu{new Gui::EditMenu(actionManager, parent)}
        , viewMenu{new Gui::ViewMenu(actionManager, editableLayout.get(), settingsManager, parent)}
        , playbackMenu{new Gui::PlaybackMenu(actionManager, playerManager, parent)}
        , libraryMenu{new Gui::LibraryMenu(actionManager, library, settingsManager, parent)}
        , helpMenu{new Gui::HelpMenu(actionManager, parent)}
        , propertiesDialog{new Gui::PropertiesDialog(parent)}
        , generalPage{settingsManager}
        , libraryGeneralPage{libraryManager, settingsManager}
        , guiGeneralPage{&layoutProvider, editableLayout.get(), settingsManager}
        , playlistGuiPage{settingsManager}
        , playlistPresetsPage{&presetRegistry, settingsManager}
        , libraryTreePage{&treeGroupRegistry, settingsManager}
        , pluginManager{new Plugins::PluginManager(parent)}
        , pluginPage{settingsManager, pluginManager}
        , corePluginContext{actionManager, playerManager, library, playlistHandler, settingsManager, &database}
        , guiPluginContext{&layoutProvider, &widgetFactory}
    {
        registerLayouts();
        registerWidgets();
        createPropertiesTabs();
        loadPlugins();
    }

    void registerLayouts()
    {
        layoutProvider.registerLayout("Empty",
                                      R"({"Layout":[{"SplitterVertical":{"Children":[],
                                     "State":"AAAA/wAAAAEAAAABAAACLwD/////AQAAAAIA"}}]})");

        layoutProvider.registerLayout("Simple",
                                      R"({"Layout":[{"SplitterVertical":{"Children":["Status","Playlist","Controls"],
                                     "State":"AAAA/wAAAAEAAAAEAAAAGQAAA94AAAAUAAAAAAD/////AQAAAAIA"}}]})");

        layoutProvider.registerLayout("Vision",
                                      R"({"Layout":[{"SplitterVertical":{"Children":["Status",{"SplitterHorizontal":{
                                     "Children":["Controls","Search"],"State":"AAAA/wAAAAEAAAADAAAD1wAAA3kAAAAAAP////
                                     8BAAAAAQA="}},{"SplitterHorizontal":{"Children":["Artwork","Playlist"],"State":
                                     "AAAA/wAAAAEAAAADAAAD2AAAA3gAAAAAAP////8BAAAAAQA="}}],"State":"AAAA/
                                     wAAAAEAAAAEAAAAGQAAAB4AAAPUAAAAFAD/////AQAAAAIA"}}]})");
    }

    void registerWidgets()
    {
        widgetFactory.registerClass<Gui::Widgets::Playlist::PlaylistTabs>(
            "PlaylistTabs",
            [this]() {
                return new Gui::Widgets::Playlist::PlaylistTabs(playlistHandler, playlistController.get());
            },
            "Playlist Tabs");

        widgetFactory.registerClass<Gui::Widgets::LibraryTreeWidget>(
            "LibraryTree",
            [this]() {
                return new Gui::Widgets::LibraryTreeWidget(library, &treeGroupRegistry, &selectionController,
                                                           settingsManager);
            },
            "Library Tree");

        widgetFactory.registerClass<Gui::Widgets::ControlWidget>("Controls", [this]() {
            return new Gui::Widgets::ControlWidget(playerManager, settingsManager);
        });

        widgetFactory.registerClass<Gui::Widgets::Info::InfoWidget>("Info", [this]() {
            return new Gui::Widgets::Info::InfoWidget(playerManager, &selectionController, settingsManager);
        });

        widgetFactory.registerClass<Gui::Widgets::CoverWidget>("Artwork", [this]() {
            return new Gui::Widgets::CoverWidget(library, playerManager);
        });

        widgetFactory.registerClass<Gui::Widgets::Playlist::PlaylistWidget>("Playlist", [this]() {
            auto* playlist = new Gui::Widgets::Playlist::PlaylistWidget(
                library, playerManager, playlistController.get(), &presetRegistry, settingsManager);
            QObject::connect(playlist, &Gui::Widgets::Playlist::PlaylistWidget::selectionWasChanged,
                             &selectionController, [this](const Core::TrackList& tracks) {
                                 selectionController.changeSelectedTracks(tracks);
                             });
            return playlist;
        });

        widgetFactory.registerClass<Gui::Widgets::Spacer>("Spacer", []() {
            return new Gui::Widgets::Spacer();
        });

        widgetFactory.registerClass<Gui::Widgets::StatusWidget>("Status", [this]() {
            return new Gui::Widgets::StatusWidget(library, playerManager);
        });
    }

    void createPropertiesTabs()
    {
        propertiesDialog->addTab("Details", [this]() {
            return new Gui::Widgets::Info::InfoWidget(playerManager, &selectionController, settingsManager);
        });
    }

    void loadPlugins() const
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

    QObject::connect(&p->selectionController, &Gui::TrackSelectionController::requestPropertiesDialog,
                     p->propertiesDialog, &Gui::PropertiesDialog::show);

    startup();
}

Application::~Application() = default;

void Application::startup()
{
    p->settingsManager->loadSettings();
    p->library->loadLibrary();
    p->layoutProvider.findLayouts();
    p->presetRegistry.loadPresets();
    p->treeGroupRegistry.loadGroupings();

    QIcon::setThemeName(p->settingsManager->value<Gui::Settings::IconTheme>());

    p->mainWindow->setupUi();
    p->editableLayout->initialise();

    if(p->libraryManager->hasLibrary() && p->settingsManager->value<Core::Settings::WaitForTracks>()) {
        connect(p->library, &Core::Library::MusicLibrary::tracksLoaded, p->mainWindow.get(), &Gui::MainWindow::show);
    }
    else {
        p->mainWindow->show();
    }
}

void Application::shutdown()
{
    p->playlistHandler->savePlaylists();
    p->editableLayout->saveLayout();
    p->presetRegistry.savePresets();
    p->treeGroupRegistry.saveGroupings();
    p->playlistController.reset(nullptr);
    p->editableLayout.reset(nullptr);
    p->mainWindow.reset(nullptr);
    p->pluginManager->shutdown();
    p->settingsManager->storeSettings();
    p->database.closeDatabase();
}
} // namespace Fy
