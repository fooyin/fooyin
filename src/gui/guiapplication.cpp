/*
 * Fooyin
 * Copyright 2022-2024, Luke Taylor <LukeT1@proton.me>
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
#include "info/infowidget.h"
#include "internalguisettings.h"
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
#include "search/searchcontroller.h"
#include "search/searchwidget.h"
#include "settings/enginepage.h"
#include "settings/generalpage.h"
#include "settings/guigeneralpage.h"
#include "settings/library/librarygeneralpage.h"
#include "settings/library/librarysortingpage.h"
#include "settings/librarytree/librarytreeguipage.h"
#include "settings/librarytree/librarytreepage.h"
#include "settings/playlist/playlistcolumnpage.h"
#include "settings/playlist/playlistgeneralpage.h"
#include "settings/playlist/playlistguipage.h"
#include "settings/playlist/playlistpresetspage.h"
#include "settings/plugins/pluginspage.h"
#include "settings/shortcuts/shortcutspage.h"
#include "settings/widgets/statuswidgetpage.h"
#include "widgets/spacer.h"
#include "widgets/splitterwidget.h"
#include "widgets/tabstackwidget.h"

#include <core/engine/enginehandler.h>
#include <core/library/librarymanager.h>
#include <core/library/musiclibrary.h>
#include <core/playlist/playlistmanager.h>
#include <core/plugins/coreplugincontext.h>
#include <core/plugins/pluginmanager.h>
#include <gui/editablelayout.h>
#include <gui/guiconstants.h>
#include <gui/guisettings.h>
#include <gui/layoutprovider.h>
#include <gui/plugins/guiplugin.h>
#include <gui/plugins/guiplugincontext.h>
#include <gui/propertiesdialog.h>
#include <gui/trackselectioncontroller.h>
#include <gui/widgetprovider.h>
#include <utils/actions/actionmanager.h>
#include <utils/settings/settingsmanager.h>
#include <utils/utils.h>

#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QPushButton>

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
    PlaylistManager* playlistHandler;

    WidgetProvider widgetProvider;
    GuiSettings guiSettings;
    LayoutProvider layoutProvider;
    std::unique_ptr<EditableLayout> editableLayout;
    std::unique_ptr<MainWindow> mainWindow;
    WidgetContext* mainContext;
    std::unique_ptr<PlaylistController> playlistController;
    TrackSelectionController selectionController;
    SearchController* searchController;

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
    PlaylistColumnPage playlistColumnPage;
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
        , playlistHandler{core.playlistHandler}
        , widgetProvider{actionManager}
        , guiSettings{settingsManager}
        , editableLayout{std::make_unique<EditableLayout>(actionManager, &widgetProvider, &layoutProvider,
                                                          settingsManager)}
        , mainWindow{std::make_unique<MainWindow>(actionManager, settingsManager, editableLayout.get())}
        , mainContext{new WidgetContext(mainWindow.get(), Context{"Fooyin.MainWindow"}, self)}
        , playlistController{std::make_unique<PlaylistController>(playlistHandler, playerManager, library,
                                                                  &selectionController, settingsManager)}
        , selectionController{actionManager, settingsManager, playlistController.get()}
        , searchController{new SearchController(editableLayout.get(), self)}
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
        , librarySortingPage{actionManager, settingsManager}
        , shortcutsPage{actionManager, settingsManager}
        , playlistGeneralPage{settingsManager}
        , playlistGuiPage{settingsManager}
        , playlistPresetsPage{settingsManager}
        , playlistColumnPage{actionManager, settingsManager}
        , enginePage{settingsManager, engineHandler}
        , libraryTreePage{actionManager, settingsManager}
        , libraryTreeGuiPage{settingsManager}
        , statusWidgetPage{settingsManager}
        , pluginPage{settingsManager, pluginManager}
        , guiPluginContext{actionManager,       &layoutProvider,  &selectionController,
                           searchController,    propertiesDialog, widgetProvider.widgetFactory(),
                           editableLayout.get()}
    {
        restoreIconTheme();
        registerLayouts();
        registerWidgets();
        createPropertiesTabs();

        actionManager->addContextObject(mainContext);

        pluginManager->initialisePlugins<GuiPlugin>(
            [this](GuiPlugin* plugin) { plugin->initialise(guiPluginContext); });
    }

    void restoreIconTheme()
    {
        using namespace Settings::Gui::Internal;

        const auto iconTheme = static_cast<IconThemeOption>(settingsManager->value<Settings::Gui::IconTheme>());
        switch(iconTheme) {
            case(IconThemeOption::AutoDetect):
                QIcon::setThemeName(Utils::isDarkMode() ? Constants::DarkIconTheme : Constants::LightIconTheme);
                break;
            case(IconThemeOption::System):
                QIcon::setThemeName(QIcon::themeName());
                break;
            case(IconThemeOption::Light):
                QIcon::setThemeName(Constants::LightIconTheme);
                break;
            case(IconThemeOption::Dark):
                QIcon::setThemeName(Constants::DarkIconTheme);
                break;
        }
    }

    void registerLayouts()
    {
        layoutProvider.registerLayout(R"({"Empty": [{}]})");

        layoutProvider.registerLayout(
            R"({"Simple":[{"SplitterVertical":{"State":"AAAA/wAAAAEAAAADAAAAGQAAA6kAAAAUAP////8BAAAAAgA=",
            "Widgets":[{"StatusBar":{}},{"SplitterHorizontal":{"State":"AAAA/wAAAAEAAAACAAABYAAABeoA/////wEAAAABAA==",
            "Widgets":[{"LibraryTree":{"Grouping":"Artist/Album","ID":"8c3bf224ae774bd780cc2ff3ad638081"}},
            {"SplitterVertical":{"State":"AAAA/wAAAAEAAAABAAAAGwD/////AQAAAAIA",
            "Widgets":[{"PlaylistTabs":{"Widgets":[{"Playlist":{"ID":"39b31f941a964e2894f6774f204140e3"}}]}}]}}]}},
            {"ControlBar":{}}]}}]})");

        layoutProvider.registerLayout(
            R"({"Vision":[{"SplitterVertical":{"State":"AAAA/wAAAAEAAAADAAAAFAAAA6kAAAAZAP////8BAAAAAgA=",
            "Widgets":[{"ControlBar":{}},{"SplitterHorizontal":{"State":"AAAA/wAAAAEAAAACAAADwgAAA4gA/////wEAAAABAA==",
            "Widgets":[{"TabStack":{"Position":"West","State":"Artwork\u001fInfo\u001fLibrary Tree\u001fPlaylist Organiser",
            "Widgets":[{"ArtworkPanel":{}},{"SelectionInfo":{}},{"LibraryTree":{"Grouping":"Artist/Album"}},{"PlaylistOrganiser":{}}]}},
            {"SplitterVertical":{"State":"AAAA/wAAAAEAAAABAAAAwAD/////AQAAAAIA","Widgets":[{"Playlist":{}}]}}]}},{"StatusBar":{}}]}}]})");
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
            u"Vertical Splitter"_s);
        factory->setSubMenus(u"SplitterVertical"_s, {"Splitters"});

        factory->registerClass<HorizontalSplitterWidget>(
            u"SplitterHorizontal"_s,
            [this]() {
                auto* splitter
                    = new HorizontalSplitterWidget(actionManager, &widgetProvider, settingsManager, mainWindow.get());
                splitter->showPlaceholder(true);
                return splitter;
            },
            u"Horizontal Splitter"_s);
        factory->setSubMenus(u"SplitterHorizontal"_s, {"Splitters"});

        factory->registerClass<PlaylistTabs>(
            u"PlaylistTabs"_s,
            [this]() {
                return new PlaylistTabs(actionManager, &widgetProvider, playlistController.get(), settingsManager,
                                        mainWindow.get());
            },
            u"Playlist Tabs"_s);
        factory->setSubMenus(u"PlaylistTabs"_s, {"Splitters"});

        factory->registerClass<PlaylistOrganiser>(
            u"PlaylistOrganiser"_s,
            [this]() {
                return new PlaylistOrganiser(actionManager, playlistController.get(), settingsManager,
                                             mainWindow.get());
            },
            u"Playlist Organiser"_s);

        factory->registerClass<TabStackWidget>(
            u"TabStack"_s, [this]() { return new TabStackWidget(actionManager, &widgetProvider, mainWindow.get()); },
            u"Tab Stack"_s);
        factory->setSubMenus(u"TabStack"_s, {"Splitters"});

        factory->registerClass<LibraryTreeWidget>(
            u"LibraryTree"_s,
            [this]() {
                return new LibraryTreeWidget(library, &selectionController, settingsManager, mainWindow.get());
            },
            u"Library Tree"_s);

        factory->registerClass<ControlWidget>(
            u"ControlBar"_s, [this]() { return new ControlWidget(playerManager, settingsManager, mainWindow.get()); },
            u"Control Bar"_s);

        factory->registerClass<InfoWidget>(
            u"SelectionInfo"_s,
            [this]() { return new InfoWidget(playerManager, &selectionController, settingsManager, mainWindow.get()); },
            u"Selection Info"_s);

        factory->registerClass<CoverWidget>(
            u"ArtworkPanel"_s,
            [this]() { return new CoverWidget(playerManager, &selectionController, mainWindow.get()); },
            u"Artwork Panel"_s);

        factory->registerClass<PlaylistWidget>(u"Playlist"_s, [this]() {
            return new PlaylistWidget(actionManager, playlistController.get(), library, settingsManager,
                                      mainWindow.get());
        });
        factory->setLimit(u"Playlist"_s, 1);

        factory->registerClass<Spacer>(u"Spacer"_s, [this]() { return new Spacer(mainWindow.get()); });

        factory->registerClass<StatusWidget>(
            u"StatusBar"_s,
            [this]() { return new StatusWidget(library, playerManager, settingsManager, mainWindow.get()); },
            u"Status Bar"_s);

        factory->registerClass<SearchWidget>(
            u"SearchBar"_s, [this]() { return new SearchWidget(searchController, settingsManager, mainWindow.get()); },
            u"Search Bar"_s);
    }

    void createPropertiesTabs()
    {
        propertiesDialog->addTab(
            u"Details"_s, [this]() { return new InfoWidget(playerManager, &selectionController, settingsManager); });
    }

    void showTrackNotFoundMessage(const Track& track) const
    {
        QMessageBox message;
        message.setIcon(QMessageBox::Warning);
        message.setText("Track Not Found");
        message.setInformativeText(track.filepath());

        message.addButton(QMessageBox::Ok);
        QPushButton* stopButton = message.addButton("Stop", QMessageBox::ActionRole);
        stopButton->setIcon(QIcon::fromTheme(Constants::Icons::Stop));
        message.setDefaultButton(QMessageBox::Ok);

        message.exec();

        if(message.clickedButton() == stopButton) {
            playerManager->stop();
        }
        else {
            playerManager->next();
        }
    }

    void addFiles() const
    {
        const auto extensions = QString{"Audio Files (%1)"}.arg(Track::supportedFileExtensions().join(" "_L1));

        const auto files = QFileDialog::getOpenFileUrls(mainWindow.get(), u"Add Files"_s, u""_s, extensions);

        if(files.empty()) {
            return;
        }

        playlistController->filesToPlaylist(files);
    }

    void addFolders() const
    {
        const auto dirs = QFileDialog::getExistingDirectoryUrl(mainWindow.get(), u"Add Folders"_s, u""_s);

        if(dirs.isEmpty()) {
            return;
        }

        playlistController->filesToPlaylist({dirs});
    }
};

GuiApplication::GuiApplication(const CorePluginContext& core)
    : p{std::make_unique<Private>(this, core)}
{
    QObject::connect(&p->selectionController, &TrackSelectionController::requestPropertiesDialog, p->propertiesDialog,
                     &PropertiesDialog::show);
    QObject::connect(p->fileMenu, &FileMenu::requestNewPlaylist, p->playlistHandler,
                     &PlaylistManager::createEmptyPlaylist);
    QObject::connect(p->fileMenu, &FileMenu::requestAddFiles, this, [this]() { p->addFiles(); });
    QObject::connect(p->fileMenu, &FileMenu::requestAddFolders, this, [this]() { p->addFolders(); });
    QObject::connect(p->viewMenu, &ViewMenu::openQuickSetup, p->editableLayout.get(), &EditableLayout::showQuickSetup);
    QObject::connect(p->engineHandler, &EngineHandler::trackStatusChanged, this, [this](TrackStatus status) {
        if(status == InvalidTrack) {
            const Track track = p->playerManager->currentTrack();
            if(track.isValid() && !QFileInfo::exists(track.filepath())) {
                p->showTrackNotFoundMessage(track);
            }
        }
    });

    p->layoutProvider.findLayouts();

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
    p->editableLayout.reset();
    p->playlistController.reset();
    p->mainWindow.reset();
}
} // namespace Fooyin

#include "moc_guiapplication.cpp"
