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
#include "settings/librarytree/librarytreeguipage.h"
#include "settings/librarytree/librarytreepage.h"
#include "settings/playlist/playlistgeneralpage.h"
#include "settings/playlist/playlistguipage.h"
#include "settings/playlist/playlistpresetspage.h"
#include "settings/plugins/pluginspage.h"
#include "settings/shortcuts/shortcutspage.h"
#include "widgets/spacer.h"
#include "widgets/tabstackwidget.h"

#include <core/library/librarymanager.h>
#include <core/library/musiclibrary.h>
#include <core/plugins/coreplugincontext.h>
#include <core/plugins/pluginmanager.h>
#include <gui/layoutprovider.h>
#include <gui/plugins/guiplugin.h>
#include <gui/plugins/guiplugincontext.h>
#include <gui/propertiesdialog.h>
#include <gui/searchcontroller.h>
#include <gui/splitterwidget.h>
#include <gui/trackselectioncontroller.h>
#include <utils/actions/actionmanager.h>
#include <utils/settings/settingsmanager.h>

using namespace Qt::Literals::StringLiterals;

namespace Fy::Gui {
struct GuiApplication::Private
{
    GuiApplication* self;

    Utils::SettingsManager* settingsManager;
    Utils::ActionManager* actionManager;

    Plugins::PluginManager* pluginManager;
    Core::Engine::EngineHandler* engineHandler;
    Core::Player::PlayerManager* playerManager;
    Core::Library::LibraryManager* libraryManager;
    Core::Library::MusicLibrary* library;
    Core::Library::SortingRegistry* sortingRegistry;
    Core::Playlist::PlaylistManager* playlistHandler;

    Widgets::WidgetProvider widgetProvider;
    Settings::GuiSettings guiSettings;
    LayoutProvider layoutProvider;
    std::unique_ptr<Widgets::EditableLayout> editableLayout;
    std::unique_ptr<MainWindow> mainWindow;
    Utils::WidgetContext* mainContext;
    std::unique_ptr<Widgets::Playlist::PlaylistController> playlistController;
    TrackSelectionController selectionController;
    Widgets::SearchController searchController;
    Widgets::Playlist::PresetRegistry presetRegistry;
    Widgets::LibraryTreeGroupRegistry treeGroupRegistry;

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
    Settings::PlaylistGeneralPage playlistGeneralPage;
    Settings::PlaylistGuiPage playlistGuiPage;
    Settings::PlaylistPresetsPage playlistPresetsPage;
    Settings::LibraryTreePage libraryTreePage;
    Settings::LibraryTreeGuiPage libraryTreeGuiPage;
    Settings::PluginPage pluginPage;
    Settings::ShortcutsPage shortcutsPage;

    GuiPluginContext guiPluginContext;

    explicit Private(GuiApplication* self, const Core::CorePluginContext& core)
        : self{self}
        , settingsManager{core.settingsManager}
        , actionManager{new Utils::ActionManager(settingsManager, self)}
        , pluginManager{core.pluginManager}
        , engineHandler{core.engineHandler}
        , playerManager{core.playerManager}
        , libraryManager{core.libraryManager}
        , library{core.library}
        , sortingRegistry{core.sortingRegistry}
        , playlistHandler{core.playlistHandler}
        , widgetProvider{actionManager}
        , guiSettings{settingsManager}
        , editableLayout{std::make_unique<Widgets::EditableLayout>(actionManager, &widgetProvider, &layoutProvider,
                                                                   settingsManager)}
        , mainWindow{std::make_unique<MainWindow>(actionManager, settingsManager, editableLayout.get())}
        , mainContext{new Utils::WidgetContext(mainWindow.get(), Utils::Context{"Fooyin.MainWindow"}, self)}
        , playlistController{std::make_unique<Widgets::Playlist::PlaylistController>(
              playlistHandler, playerManager, &presetRegistry, sortingRegistry, settingsManager)}
        , selectionController{actionManager, settingsManager, playlistController.get()}
        , presetRegistry{settingsManager}
        , treeGroupRegistry{settingsManager}
        , fileMenu{new FileMenu(actionManager, settingsManager, self)}
        , editMenu{new EditMenu(actionManager, settingsManager, self)}
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
        , playlistGeneralPage{settingsManager}
        , playlistGuiPage{settingsManager}
        , playlistPresetsPage{&presetRegistry, settingsManager}
        , libraryTreePage{&treeGroupRegistry, settingsManager}
        , libraryTreeGuiPage{settingsManager}
        , pluginPage{settingsManager, pluginManager}
        , shortcutsPage{actionManager, settingsManager}
        , guiPluginContext{actionManager,     &layoutProvider,  &selectionController,
                           &searchController, propertiesDialog, widgetProvider.widgetFactory()}
    {
        registerLayouts();
        registerWidgets();
        createPropertiesTabs();

        actionManager->addContextObject(mainContext);

        pluginManager->initialisePlugins<GuiPlugin>(
            [this](GuiPlugin* plugin) { plugin->initialise(guiPluginContext); });
    }

    void registerLayouts()
    {
        layoutProvider.registerLayout(u"Empty"_s,
                                      R"({"Layout":[{"SplitterVertical":{"Children":[],
                                     "State":"AAAA/wAAAAEAAAABAAACLwD/////AQAAAAIA"}}]})");

        layoutProvider.registerLayout(u"Simple"_s,
                                      R"({"Layout":[{"SplitterVertical":{"Children":["Status","Playlist","Controls"],
                                     "State":"AAAA/wAAAAEAAAAEAAAAGQAAA94AAAAUAAAAAAD/////AQAAAAIA"}}]})");

        layoutProvider.registerLayout(u"Vision"_s,
                                      R"({"Layout":[{"SplitterVertical":{"Children":["Status",{"SplitterHorizontal":{
                                     "Children":["Controls","Search"],"State":"AAAA/wAAAAEAAAADAAAD1wAAA3kAAAAAAP////
                                     8BAAAAAQA="}},{"SplitterHorizontal":{"Children":["Artwork","Playlist"],"State":
                                     "AAAA/wAAAAEAAAADAAAD2AAAA3gAAAAAAP////8BAAAAAQA="}}],"State":"AAAA/
                                     wAAAAEAAAAEAAAAGQAAAB4AAAPUAAAAFAD/////AQAAAAIA"}}]})");
    }

    void registerWidgets()
    {
        auto* factory = widgetProvider.widgetFactory();

        factory->registerClass<Widgets::VerticalSplitterWidget>(
            u"SplitterVertical"_s,
            [this]() {
                auto* splitter = new Widgets::VerticalSplitterWidget(actionManager, &widgetProvider, settingsManager,
                                                                     mainWindow.get());
                splitter->showPlaceholder(true);
                return splitter;
            },
            u"Vertical Splitter"_s, {"Splitters"});

        factory->registerClass<Widgets::HorizontalSplitterWidget>(
            u"SplitterHorizontal"_s,
            [this]() {
                auto* splitter = new Widgets::HorizontalSplitterWidget(actionManager, &widgetProvider, settingsManager,
                                                                       mainWindow.get());
                splitter->showPlaceholder(true);
                return splitter;
            },
            u"Horizontal Splitter"_s, {"Splitters"});

        factory->registerClass<Widgets::Playlist::PlaylistTabs>(
            u"PlaylistTabs"_s,
            [this]() {
                return new Widgets::Playlist::PlaylistTabs(actionManager, &widgetProvider, playlistController.get(),
                                                           settingsManager, mainWindow.get());
            },
            u"Playlist Tabs"_s, {"Splitters"});

        factory->registerClass<Widgets::TabStackWidget>(
            u"TabStack"_s,
            [this]() { return new Widgets::TabStackWidget(actionManager, &widgetProvider, mainWindow.get()); },
            u"Tab Stack"_s, {"Splitters"});

        factory->registerClass<Widgets::LibraryTreeWidget>(
            u"LibraryTree"_s,
            [this]() {
                return new Widgets::LibraryTreeWidget(library, &treeGroupRegistry, &selectionController,
                                                      settingsManager, mainWindow.get());
            },
            u"Library Tree"_s);

        factory->registerClass<Widgets::ControlWidget>(u"Controls"_s, [this]() {
            return new Widgets::ControlWidget(playerManager, settingsManager, mainWindow.get());
        });

        factory->registerClass<Widgets::Info::InfoWidget>(u"Info"_s, [this]() {
            return new Widgets::Info::InfoWidget(playerManager, &selectionController, settingsManager,
                                                 mainWindow.get());
        });

        factory->registerClass<Widgets::CoverWidget>(u"Artwork"_s, [this]() {
            return new Widgets::CoverWidget(playerManager, &selectionController, mainWindow.get());
        });

        factory->registerClass<Widgets::Playlist::PlaylistWidget>(u"Playlist"_s, [this]() {
            return new Widgets::Playlist::PlaylistWidget(actionManager, playlistController.get(), &selectionController,
                                                         settingsManager, mainWindow.get());
        });

        factory->registerClass<Widgets::Spacer>(u"Spacer"_s,
                                                [this]() { return new Widgets::Spacer(mainWindow.get()); });

        factory->registerClass<Widgets::StatusWidget>(u"Status"_s, [this]() {
            return new Widgets::StatusWidget(library, playerManager, settingsManager, mainWindow.get());
        });

        factory->registerClass<Widgets::SearchWidget>(u"Search"_s, [this]() {
            return new Widgets::SearchWidget(actionManager, &searchController, settingsManager, mainWindow.get());
        });
    }

    void createPropertiesTabs()
    {
        propertiesDialog->addTab(u"Details"_s, [this]() {
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

    p->presetRegistry.loadItems();
    p->treeGroupRegistry.loadItems();
    p->layoutProvider.findLayouts();

    QIcon::setThemeName(p->settingsManager->value<Settings::IconTheme>());

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
    p->actionManager->saveSettings();
    p->editableLayout->saveLayout();
    p->presetRegistry.saveItems();
    p->treeGroupRegistry.saveItems();
    p->playlistController.reset(nullptr);
    p->editableLayout.reset(nullptr);
    p->mainWindow.reset(nullptr);
}
} // namespace Fy::Gui
