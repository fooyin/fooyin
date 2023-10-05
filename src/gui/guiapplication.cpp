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

#include <core/coreplugincontext.h>
#include <core/coresettings.h>
#include <core/library/librarymanager.h>
#include <core/library/musiclibrary.h>
#include <core/plugins/pluginmanager.h>

#include <utils/settings/settingsmanager.h>

#include "editablelayout.h"
#include "gui/controls/controlwidget.h"
#include "gui/dialog/propertiesdialog.h"
#include "gui/guiplugincontext.h"
#include "gui/info/infowidget.h"
#include "gui/library/coverwidget.h"
#include "gui/library/statuswidget.h"
#include "gui/librarytree/librarytreegroupregistry.h"
#include "gui/librarytree/librarytreewidget.h"
#include "gui/menu/editmenu.h"
#include "gui/menu/filemenu.h"
#include "gui/menu/helpmenu.h"
#include "gui/menu/librarymenu.h"
#include "gui/menu/playbackmenu.h"
#include "gui/menu/viewmenu.h"
#include "gui/playlist/playlistcontroller.h"
#include "gui/playlist/playlisttabs.h"
#include "gui/playlist/playlistwidget.h"
#include "gui/playlist/presetregistry.h"
#include "gui/search/searchcontroller.h"
#include "gui/search/searchwidget.h"
#include "gui/settings/generalpage.h"
#include "gui/settings/guigeneralpage.h"
#include "gui/settings/library/librarygeneralpage.h"
#include "gui/settings/library/librarysortingpage.h"
#include "gui/settings/librarytree/librarytreepage.h"
#include "gui/settings/playlist/playlistguipage.h"
#include "gui/settings/playlist/playlistpresetspage.h"
#include "gui/settings/plugins/pluginspage.h"
#include "gui/widgets/spacer.h"
#include "guiplugin.h"
#include "layoutprovider.h"
#include "mainwindow.h"
#include "trackselectioncontroller.h"
#include "widgetfactory.h"

namespace Fy::Gui {
struct GuiApplication::Private
{
    GuiApplication* self;

    Utils::ActionManager* actionManager;
    Utils::SettingsManager* settingsManager;

    Plugins::PluginManager* pluginManager;
    Core::Player::PlayerManager* playerManager;
    Core::Library::LibraryManager* libraryManager;
    Core::Library::MusicLibrary* library;
    Core::Library::SortingRegistry* sortingRegistry;
    Core::Playlist::PlaylistHandler* playlistHandler;

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
        , actionManager{core.actionManager}
        , settingsManager{core.settingsManager}
        , pluginManager{core.pluginManager}
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
        , viewMenu{new ViewMenu(actionManager, editableLayout.get(), settingsManager, self)}
        , playbackMenu{new PlaybackMenu(actionManager, playerManager, self)}
        , libraryMenu{new LibraryMenu(actionManager, library, settingsManager, self)}
        , helpMenu{new HelpMenu(actionManager, self)}
        , propertiesDialog{new PropertiesDialog(self)}
        , generalPage{settingsManager}
        , libraryGeneralPage{libraryManager, settingsManager}
        , librarySortingPage{sortingRegistry, settingsManager}
        , guiGeneralPage{&layoutProvider, editableLayout.get(), settingsManager}
        , playlistGuiPage{settingsManager}
        , playlistPresetsPage{&presetRegistry, settingsManager}
        , libraryTreePage{&treeGroupRegistry, settingsManager}
        , pluginPage{settingsManager, pluginManager}
        , guiPluginContext{&layoutProvider, &selectionController, &searchController, propertiesDialog, &widgetFactory}
    {
        registerLayouts();
        registerWidgets();
        createPropertiesTabs();

        pluginManager->initialisePlugins<GuiPlugin>(guiPluginContext);
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
        widgetFactory.registerClass<Widgets::Playlist::PlaylistTabs>(
            "PlaylistTabs",
            [this]() {
                return new Widgets::Playlist::PlaylistTabs(playlistHandler, playlistController.get());
            },
            "Playlist Tabs");

        widgetFactory.registerClass<Widgets::LibraryTreeWidget>(
            "LibraryTree",
            [this]() {
                return new Widgets::LibraryTreeWidget(library, &treeGroupRegistry, &selectionController,
                                                      settingsManager);
            },
            "Library Tree");

        widgetFactory.registerClass<Widgets::ControlWidget>("Controls", [this]() {
            return new Widgets::ControlWidget(playerManager, settingsManager);
        });

        widgetFactory.registerClass<Widgets::Info::InfoWidget>("Info", [this]() {
            return new Widgets::Info::InfoWidget(playerManager, &selectionController, settingsManager);
        });

        widgetFactory.registerClass<Widgets::CoverWidget>("Artwork", [this]() {
            return new Widgets::CoverWidget(library, playerManager);
        });

        widgetFactory.registerClass<Widgets::Playlist::PlaylistWidget>("Playlist", [this]() {
            return new Widgets::Playlist::PlaylistWidget(playerManager, playlistController.get(), &selectionController,
                                                         settingsManager);
        });

        widgetFactory.registerClass<Widgets::Spacer>("Spacer", []() {
            return new Widgets::Spacer();
        });

        widgetFactory.registerClass<Widgets::StatusWidget>("Status", [this]() {
            return new Widgets::StatusWidget(library, playerManager);
        });

        widgetFactory.registerClass<Widgets::SearchWidget>("Search", [this]() {
            return new Widgets::SearchWidget(&searchController, settingsManager);
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
