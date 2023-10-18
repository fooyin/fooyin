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

#include "guiapplication.h"

#include "controls/controlwidget.h"
#include "editablelayout.h"
#include "info/infowidget.h"
#include "library/coverwidget.h"
#include "library/statuswidget.h"
#include "librarytree/librarytreegroupregistry.h"
#include "librarytree/librarytreewidget.h"
#include "mainwindow.h"
#include "menu/editmenu.h"
#include "menu/filemenu.h"
#include "menu/helpmenu.h"
#include "menu/librarymenu.h"
#include "menu/playbackmenu.h"
#include "menu/viewmenu.h"
#include "playlist/playlistcontroller.h"
#include "playlist/playlisttabs.h"
#include "playlist/playlistwidget.h"
#include "playlist/presetregistry.h"
#include "search/searchwidget.h"
#include "settings/enginepage.h"
#include "settings/generalpage.h"
#include "settings/guigeneralpage.h"
#include "settings/library/librarygeneralpage.h"
#include "settings/library/librarysortingpage.h"
#include "settings/librarytree/librarytreepage.h"
#include "settings/playlist/playlistguipage.h"
#include "settings/playlist/playlistpresetspage.h"
#include "settings/plugins/pluginspage.h"
#include "widgets/spacer.h"

#include <core/library/librarymanager.h>
#include <core/library/musiclibrary.h>
#include <core/plugins/coreplugincontext.h>
#include <core/plugins/pluginmanager.h>
#include <gui/layoutprovider.h>
#include <gui/plugins/guiplugin.h>
#include <gui/plugins/guiplugincontext.h>
#include <gui/propertiesdialog.h>
#include <gui/searchcontroller.h>
#include <gui/trackselectioncontroller.h>
#include <gui/widgetfactory.h>
#include <utils/actions/actionmanager.h>
#include <utils/settings/settingsmanager.h>

namespace Fy::Gui {
struct GuiApplication::Private
{
    GuiApplication* self;

    Utils::ActionManager* actionManager;
    Utils::SettingsManager* settingsManager;

    Plugins::PluginManager* pluginManager;
    Core::Engine::EngineHandler* engineHandler;
    Core::Player::PlayerManager* playerManager;
    Core::Library::LibraryManager* libraryManager;
    Core::Library::MusicLibrary* library;
    Core::Library::SortingRegistry* sortingRegistry;
    Core::Playlist::PlaylistManager* playlistHandler;

    Widgets::WidgetFactory widgetFactory;
    Settings::GuiSettings guiSettings;
    LayoutProvider layoutProvider;
    std::unique_ptr<Widgets::Playlist::PlaylistController> playlistController;
    TrackSelectionController selectionController;
    Widgets::SearchController searchController;
    Widgets::Playlist::PresetRegistry presetRegistry;
    Widgets::LibraryTreeGroupRegistry treeGroupRegistry;
    std::unique_ptr<Widgets::EditableLayout> editableLayout;
    std::unique_ptr<MainWindow> mainWindow;

    FileMenu* fileMenu;
    EditMenu* editMenu;
    ViewMenu* viewMenu;
    PlaybackMenu* playbackMenu;
    LibraryMenu* libraryMenu;
    HelpMenu* helpMenu;

    PropertiesDialog* propertiesDialog;

    Settings::GeneralPage generalPage;
    Settings::EnginePage enginePage;
    Settings::LibraryGeneralPage libraryGeneralPage;
    Settings::LibrarySortingPage librarySortingPage;
    Settings::GuiGeneralPage guiGeneralPage;
    Settings::PlaylistGuiPage playlistGuiPage;
    Settings::PlaylistPresetsPage playlistPresetsPage;
    Settings::LibraryTreePage libraryTreePage;
    Settings::PluginPage pluginPage;

    GuiPluginContext guiPluginContext;

    explicit Private(GuiApplication* self, const Core::CorePluginContext& core)
        : self{self}
        , actionManager{new Utils::ActionManager(self)}
        , settingsManager{core.settingsManager}
        , pluginManager{core.pluginManager}
        , engineHandler{core.engineHandler}
        , playerManager{core.playerManager}
        , libraryManager{core.libraryManager}
        , library{core.library}
        , sortingRegistry{core.sortingRegistry}
        , playlistHandler{core.playlistHandler}
        , guiSettings{settingsManager}
        , playlistController{std::make_unique<Widgets::Playlist::PlaylistController>(playlistHandler, &presetRegistry,
                                                                                     sortingRegistry, settingsManager)}
        , selectionController{actionManager, settingsManager, playlistController.get()}
        , presetRegistry{settingsManager}
        , treeGroupRegistry{settingsManager}
        , editableLayout{std::make_unique<Widgets::EditableLayout>(settingsManager, actionManager, &widgetFactory,
                                                                   &layoutProvider)}
        , mainWindow{std::make_unique<MainWindow>(actionManager, settingsManager, editableLayout.get())}
        , fileMenu{new FileMenu(actionManager, settingsManager, self)}
        , editMenu{new EditMenu(actionManager, self)}
        , viewMenu{new ViewMenu(actionManager, &selectionController, settingsManager, self)}
        , playbackMenu{new PlaybackMenu(actionManager, playerManager, self)}
        , libraryMenu{new LibraryMenu(actionManager, library, settingsManager, self)}
        , helpMenu{new HelpMenu(actionManager, self)}
        , propertiesDialog{new PropertiesDialog(self)}
        , generalPage{settingsManager}
        , enginePage{settingsManager, engineHandler}
        , libraryGeneralPage{libraryManager, settingsManager}
        , librarySortingPage{sortingRegistry, settingsManager}
        , guiGeneralPage{&layoutProvider, editableLayout.get(), settingsManager}
        , playlistGuiPage{settingsManager}
        , playlistPresetsPage{&presetRegistry, settingsManager}
        , libraryTreePage{&treeGroupRegistry, settingsManager}
        , pluginPage{settingsManager, pluginManager}
        , guiPluginContext{actionManager,     &layoutProvider,  &selectionController,
                           &searchController, propertiesDialog, &widgetFactory}
    {
        registerLayouts();
        registerWidgets();
        createPropertiesTabs();

        pluginManager->initialisePlugins<GuiPlugin>([this](GuiPlugin* plugin) {
            plugin->initialise(guiPluginContext);
        });
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
        widgetFactory.registerClass<Widgets::VerticalSplitterWidget>(
            "SplitterVertical",
            [this]() {
                auto* splitter = new Widgets::VerticalSplitterWidget(actionManager, &widgetFactory, settingsManager,
                                                                     mainWindow.get());
                splitter->showPlaceholder(true);
                return splitter;
            },
            "Vertical Splitter", {"Splitters"});

        widgetFactory.registerClass<Widgets::HorizontalSplitterWidget>(
            "SplitterHorizontal",
            [this]() {
                auto* splitter = new Widgets::HorizontalSplitterWidget(actionManager, &widgetFactory, settingsManager,
                                                                       mainWindow.get());
                splitter->showPlaceholder(true);
                return splitter;
            },
            "Horizontal Splitter", {"Splitters"});

        widgetFactory.registerClass<Widgets::Playlist::PlaylistTabs>(
            "PlaylistTabs",
            [this]() {
                return new Widgets::Playlist::PlaylistTabs(actionManager, &widgetFactory, playlistController.get(),
                                                           settingsManager, mainWindow.get());
            },
            "Playlist Tabs", {"Splitters"});

        widgetFactory.registerClass<Widgets::LibraryTreeWidget>(
            "LibraryTree",
            [this]() {
                return new Widgets::LibraryTreeWidget(library, &treeGroupRegistry, &selectionController,
                                                      settingsManager, mainWindow.get());
            },
            "Library Tree");

        widgetFactory.registerClass<Widgets::ControlWidget>("Controls", [this]() {
            return new Widgets::ControlWidget(playerManager, settingsManager, mainWindow.get());
        });

        widgetFactory.registerClass<Widgets::Info::InfoWidget>("Info", [this]() {
            return new Widgets::Info::InfoWidget(playerManager, &selectionController, settingsManager,
                                                 mainWindow.get());
        });

        widgetFactory.registerClass<Widgets::CoverWidget>("Artwork", [this]() {
            return new Widgets::CoverWidget(library, playerManager, mainWindow.get());
        });

        widgetFactory.registerClass<Widgets::Playlist::PlaylistWidget>("Playlist", [this]() {
            return new Widgets::Playlist::PlaylistWidget(playerManager, playlistController.get(), &selectionController,
                                                         settingsManager, mainWindow.get());
        });

        widgetFactory.registerClass<Widgets::Spacer>("Spacer", [this]() {
            return new Widgets::Spacer(mainWindow.get());
        });

        widgetFactory.registerClass<Widgets::StatusWidget>("Status", [this]() {
            return new Widgets::StatusWidget(library, playerManager, settingsManager, mainWindow.get());
        });

        widgetFactory.registerClass<Widgets::SearchWidget>("Search", [this]() {
            return new Widgets::SearchWidget(&searchController, settingsManager, mainWindow.get());
        });
    }

    void createPropertiesTabs()
    {
        propertiesDialog->addTab("Details", [this]() {
            return new Widgets::Info::InfoWidget(playerManager, &selectionController, settingsManager);
        });
    }
};

GuiApplication::GuiApplication(const Core::CorePluginContext& core)
    : p{std::make_unique<Private>(this, core)}
{
    QObject::connect(&p->selectionController, &TrackSelectionController::requestPropertiesDialog, p->propertiesDialog,
                     &PropertiesDialog::show);
    QObject::connect(p->viewMenu, &ViewMenu::openQuickSetup, p->editableLayout.get(),
                     &Widgets::EditableLayout::showQuickSetup);

    p->layoutProvider.findLayouts();
    p->presetRegistry.loadItems();
    p->treeGroupRegistry.loadItems();

    QIcon::setThemeName(p->settingsManager->value<Settings::IconTheme>());

    p->mainWindow->setupUi();
    p->editableLayout->initialise();

    if(p->libraryManager->hasLibrary() && p->settingsManager->value<Settings::WaitForTracks>()) {
        connect(p->library, &Core::Library::MusicLibrary::tracksLoaded, p->mainWindow.get(), &MainWindow::open);
    }
    else {
        p->mainWindow->open();
    }
}

GuiApplication::~GuiApplication() = default;

void GuiApplication::shutdown()
{
    p->editableLayout->saveLayout();
    p->presetRegistry.saveItems();
    p->treeGroupRegistry.saveItems();
    p->playlistController.reset(nullptr);
    p->editableLayout.reset(nullptr);
    p->mainWindow.reset(nullptr);
}
} // namespace Fy::Gui
