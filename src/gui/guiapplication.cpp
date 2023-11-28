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
#include "librarytree/librarytreewidget.h"
#include "mainwindow.h"
#include "menu/editmenu.h"
#include "menu/filemenu.h"
#include "menu/helpmenu.h"
#include "menu/librarymenu.h"
#include "menu/playbackmenu.h"
#include "menu/viewmenu.h"
#include "playlist/organiser/playlistorganiser.h"
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
#include "settings/widgets/statuswidgetpage.h"
#include "widgets/spacer.h"
#include "widgets/tabstackwidget.h"

#include <core/coresettings.h>
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

namespace Fooyin {
struct GuiApplication::Private
{
    GuiApplication* self;

    SettingsManager* settingsManager;
    ActionManager* actionManager;

    PluginManager* pluginManager;
    EngineHandler* engineHandler;
    PlayerManager* playerManager;
    LibraryManager* libraryManager;
    MusicLibrary* library;
    CoreSettings* coreSettings;
    PlaylistManager* playlistHandler;

    WidgetProvider widgetProvider;
    GuiSettings guiSettings;
    LayoutProvider layoutProvider;
    std::unique_ptr<EditableLayout> editableLayout;
    std::unique_ptr<MainWindow> mainWindow;
    WidgetContext* mainContext;
    std::unique_ptr<PlaylistController> playlistController;
    TrackSelectionController selectionController;
    SearchController searchController;

    FileMenu* fileMenu;
    EditMenu* editMenu;
    ViewMenu* viewMenu;
    PlaybackMenu* playbackMenu;
    LibraryMenu* libraryMenu;
    HelpMenu* helpMenu;

    PropertiesDialog* propertiesDialog;

    GeneralPage generalPage;
    GuiGeneralPage guiGeneralPage;
    LibraryGeneralPage libraryGeneralPage;
    LibrarySortingPage librarySortingPage;
    ShortcutsPage shortcutsPage;
    PlaylistGeneralPage playlistGeneralPage;
    PlaylistGuiPage playlistGuiPage;
    PlaylistPresetsPage playlistPresetsPage;
    EnginePage enginePage;
    LibraryTreePage libraryTreePage;
    LibraryTreeGuiPage libraryTreeGuiPage;
    StatusWidgetPage statusWidgetPage;
    PluginPage pluginPage;

    GuiPluginContext guiPluginContext;

    explicit Private(GuiApplication* self, const CorePluginContext& core)
        : self{self}
        , settingsManager{core.settingsManager}
        , actionManager{new ActionManager(settingsManager, self)}
        , pluginManager{core.pluginManager}
        , engineHandler{core.engineHandler}
        , playerManager{core.playerManager}
        , libraryManager{core.libraryManager}
        , library{core.library}
        , coreSettings{core.coreSettings}
        , playlistHandler{core.playlistHandler}
        , widgetProvider{actionManager}
        , guiSettings{settingsManager}
        , editableLayout{std::make_unique<EditableLayout>(actionManager, &widgetProvider, &layoutProvider,
                                                          settingsManager)}
        , mainWindow{std::make_unique<MainWindow>(actionManager, settingsManager, editableLayout.get())}
        , mainContext{new WidgetContext(mainWindow.get(), Context{"Fooyin.MainWindow"}, self)}
        , playlistController{std::make_unique<PlaylistController>(
              playlistHandler, playerManager, guiSettings.playlistPresetRegistry(), coreSettings->sortingRegistry(),
              &selectionController, settingsManager)}
        , selectionController{actionManager, settingsManager, playlistController.get()}
        , fileMenu{new FileMenu(actionManager, settingsManager, self)}
        , editMenu{new EditMenu(actionManager, settingsManager, self)}
        , viewMenu{new ViewMenu(actionManager, &selectionController, settingsManager, self)}
        , playbackMenu{new PlaybackMenu(actionManager, playerManager, self)}
        , libraryMenu{new LibraryMenu(actionManager, library, settingsManager, self)}
        , helpMenu{new HelpMenu(actionManager, self)}
        , propertiesDialog{new PropertiesDialog(settingsManager, self)}
        , generalPage{settingsManager}
        , guiGeneralPage{&layoutProvider, editableLayout.get(), settingsManager}
        , libraryGeneralPage{actionManager, libraryManager, settingsManager}
        , librarySortingPage{actionManager, coreSettings->sortingRegistry(), settingsManager}
        , shortcutsPage{actionManager, settingsManager}
        , playlistGeneralPage{settingsManager}
        , playlistGuiPage{settingsManager}
        , playlistPresetsPage{guiSettings.playlistPresetRegistry(), settingsManager}
        , enginePage{settingsManager, engineHandler}
        , libraryTreePage{actionManager, guiSettings.libraryTreeGroupRegistry(), settingsManager}
        , libraryTreeGuiPage{settingsManager}
        , statusWidgetPage{settingsManager}
        , pluginPage{settingsManager, pluginManager}
        , guiPluginContext{actionManager,     &layoutProvider,  &selectionController,
                           &searchController, propertiesDialog, widgetProvider.widgetFactory(),
                           &guiSettings}
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
        layoutProvider.registerLayout(R"({"Empty":[{"SplitterVertical":{"Children":[],
                                     "State":"AAAA/wAAAAEAAAABAAACLwD/////AQAAAAIA"}}]})");

        layoutProvider.registerLayout(R"({"Simple":[{"SplitterVertical":{"Children":["Status","Playlist","Controls"],
                                     "State":"AAAA/wAAAAEAAAAEAAAAGQAAA94AAAAUAAAAAAD/////AQAAAAIA"}}]})");

        layoutProvider.registerLayout(R"({"Stone":[{"SplitterVertical":{"Children":["Status","Search",{
            "SplitterHorizontal":{"Children":[{"LibraryTree":{"Grouping":"Artist/Album"}},{
            "PlaylistTabs":["Playlist"]}],"State":"AAAA/wAAAAEAAAACAAABQgAABggA/////wEAAAABAA=="}},"Controls"],
            "State":"AAAA/wAAAAEAAAAEAAAAGQAAAB4AAAO8AAAAFAD/////AQAAAAIA"}}]})");

        layoutProvider.registerLayout(R"({"Vision":[{"SplitterVertical":{"Children":["Status",{"SplitterHorizontal":{
                                     "Children":["Controls","Search"],"State":"AAAA/wAAAAEAAAADAAAD1wAAA3kAAAAAAP////
                                     8BAAAAAQA="}},{"SplitterHorizontal":{"Children":["Artwork","Playlist"],"State":
                                     "AAAA/wAAAAEAAAADAAAD2AAAA3gAAAAAAP////8BAAAAAQA="}}],"State":"AAAA/
                                     wAAAAEAAAAEAAAAGQAAAB4AAAPUAAAAFAD/////AQAAAAIA"}}]})");
    }

    void registerWidgets()
    {
        auto* factory = widgetProvider.widgetFactory();

        factory->registerClass<VerticalSplitterWidget>(
            u"SplitterVertical"_s,
            [this]() {
                auto* splitter
                    = new VerticalSplitterWidget(actionManager, &widgetProvider, settingsManager, mainWindow.get());
                splitter->showPlaceholder(true);
                return splitter;
            },
            u"Vertical Splitter"_s, {"Splitters"});

        factory->registerClass<HorizontalSplitterWidget>(
            u"SplitterHorizontal"_s,
            [this]() {
                auto* splitter
                    = new HorizontalSplitterWidget(actionManager, &widgetProvider, settingsManager, mainWindow.get());
                splitter->showPlaceholder(true);
                return splitter;
            },
            u"Horizontal Splitter"_s, {"Splitters"});

        factory->registerClass<PlaylistTabs>(u"PlaylistTabs"_s,
                                             [this]() {
                                                 return new PlaylistTabs(actionManager, &widgetProvider,
                                                                         playlistController.get(), settingsManager,
                                                                         mainWindow.get());
                                             },
                                             u"Playlist Tabs"_s, {"Splitters"});

        factory->registerClass<PlaylistOrganiser>(
            u"PlaylistOrganiser"_s,
            [this]() {
                return new PlaylistOrganiser(actionManager, playlistController.get(), settingsManager,
                                             mainWindow.get());
            },
            u"Playlist Organiser"_s);

        factory->registerClass<TabStackWidget>(
            u"TabStack"_s, [this]() { return new TabStackWidget(actionManager, &widgetProvider, mainWindow.get()); },
            u"Tab Stack"_s, {"Splitters"});

        factory->registerClass<LibraryTreeWidget>(
            u"LibraryTree"_s,
            [this]() {
                return new LibraryTreeWidget(library, guiSettings.libraryTreeGroupRegistry(), &selectionController,
                                             settingsManager, mainWindow.get());
            },
            u"Library Tree"_s);

        factory->registerClass<ControlWidget>(
            u"Controls"_s, [this]() { return new ControlWidget(playerManager, settingsManager, mainWindow.get()); });

        factory->registerClass<InfoWidget>(u"Info"_s, [this]() {
            return new InfoWidget(playerManager, &selectionController, settingsManager, mainWindow.get());
        });

        factory->registerClass<CoverWidget>(
            u"Artwork"_s, [this]() { return new CoverWidget(playerManager, &selectionController, mainWindow.get()); });

        factory->registerClass<PlaylistWidget>(u"Playlist"_s, [this]() {
            return new PlaylistWidget(actionManager, playlistController.get(), library, settingsManager,
                                      mainWindow.get());
        });

        factory->registerClass<Spacer>(u"Spacer"_s, [this]() { return new Spacer(mainWindow.get()); });

        factory->registerClass<StatusWidget>(u"Status"_s, [this]() {
            return new StatusWidget(library, playerManager, settingsManager, mainWindow.get());
        });

        factory->registerClass<SearchWidget>(u"Search"_s, [this]() {
            return new SearchWidget(actionManager, &searchController, settingsManager, mainWindow.get());
        });
    }

    void createPropertiesTabs()
    {
        propertiesDialog->addTab(
            u"Details"_s, [this]() { return new InfoWidget(playerManager, &selectionController, settingsManager); });
    }
};

GuiApplication::GuiApplication(const CorePluginContext& core)
    : p{std::make_unique<Private>(this, core)}
{
    QObject::connect(&p->selectionController, &TrackSelectionController::requestPropertiesDialog, p->propertiesDialog,
                     &PropertiesDialog::show);
    QObject::connect(p->viewMenu, &ViewMenu::openQuickSetup, p->editableLayout.get(), &EditableLayout::showQuickSetup);

    p->layoutProvider.findLayouts();

    QIcon::setThemeName(p->settingsManager->value<Settings::Gui::IconTheme>());

    p->editableLayout->initialise();

    if(p->libraryManager->hasLibrary() && p->settingsManager->value<Settings::Gui::WaitForTracks>()) {
        connect(p->library, &MusicLibrary::tracksLoaded, p->mainWindow.get(), &MainWindow::open);
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
    p->playlistController.reset(nullptr);
    p->editableLayout.reset(nullptr);
    p->mainWindow.reset(nullptr);
}
} // namespace Fooyin

#include "moc_guiapplication.cpp"
