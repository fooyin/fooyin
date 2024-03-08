/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
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

#include "controls/playercontrol.h"
#include "controls/playlistcontrol.h"
#include "controls/seekbar.h"
#include "controls/volumecontrol.h"
#include "core/internalcoresettings.h"
#include "dirbrowser/dirbrowser.h"
#include "info/infowidget.h"
#include "internalguisettings.h"
#include "librarytree/librarytreewidget.h"
#include "mainwindow.h"
#include "menubar/editmenu.h"
#include "menubar/filemenu.h"
#include "menubar/helpmenu.h"
#include "menubar/librarymenu.h"
#include "menubar/mainmenubar.h"
#include "menubar/playbackmenu.h"
#include "menubar/viewmenu.h"
#include "playlist/organiser/playlistorganiser.h"
#include "playlist/playlistcontroller.h"
#include "playlist/playlisttabs.h"
#include "playlist/playlistwidget.h"
#include "search/searchcontroller.h"
#include "search/searchwidget.h"
#include "settings/dirbrowser/dirbrowserpage.h"
#include "settings/enginepage.h"
#include "settings/generalpage.h"
#include "settings/guigeneralpage.h"
#include "settings/library/librarygeneralpage.h"
#include "settings/library/librarysortingpage.h"
#include "settings/librarytree/librarytreeguipage.h"
#include "settings/librarytree/librarytreepage.h"
#include "settings/playlist/playlistcolumnpage.h"
#include "settings/playlist/playlistgeneralpage.h"
#include "settings/playlist/playlistpresetspage.h"
#include "settings/plugins/pluginspage.h"
#include "settings/shortcuts/shortcutspage.h"
#include "settings/widgets/statuswidgetpage.h"
#include "widgets/coverwidget.h"
#include "widgets/spacer.h"
#include "widgets/splitterwidget.h"
#include "widgets/statuswidget.h"
#include "widgets/tabstackwidget.h"

#include <core/coresettings.h>
#include <core/engine/enginehandler.h>
#include <core/library/librarymanager.h>
#include <core/library/musiclibrary.h>
#include <core/playlist/playlisthandler.h>
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

#include <QAction>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QPushButton>

namespace Fooyin {
struct GuiApplication::Private
{
    GuiApplication* self;

    SettingsManager* settingsManager;
    ActionManager* actionManager;

    PluginManager* pluginManager;
    EngineController* engine;
    PlayerController* playerController;
    LibraryManager* libraryManager;
    MusicLibrary* library;
    PlaylistHandler* playlistHandler;

    WidgetProvider widgetProvider;
    GuiSettings guiSettings;
    LayoutProvider layoutProvider;
    std::unique_ptr<EditableLayout> editableLayout;
    std::unique_ptr<MainMenuBar> menubar;
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
    PlaylistPresetsPage playlistPresetsPage;
    PlaylistColumnPage playlistColumnPage;
    EnginePage enginePage;
    DirBrowserPage dirBrowserPage;
    LibraryTreePage libraryTreePage;
    LibraryTreeGuiPage libraryTreeGuiPage;
    StatusWidgetPage statusWidgetPage;
    PluginPage pluginPage;

    GuiPluginContext guiPluginContext;

    explicit Private(GuiApplication* self_, const CorePluginContext& core)
        : self{self_}
        , settingsManager{core.settingsManager}
        , actionManager{new ActionManager(settingsManager, self)}
        , pluginManager{core.pluginManager}
        , engine{core.engine}
        , playerController{core.playerController}
        , libraryManager{core.libraryManager}
        , library{core.library}
        , playlistHandler{core.playlistHandler}
        , widgetProvider{actionManager}
        , guiSettings{settingsManager}
        , editableLayout{std::make_unique<EditableLayout>(actionManager, &widgetProvider, &layoutProvider,
                                                          settingsManager)}
        , menubar{std::make_unique<MainMenuBar>(actionManager)}
        , mainWindow{std::make_unique<MainWindow>(actionManager, menubar.get(), settingsManager)}
        , mainContext{new WidgetContext(mainWindow.get(), Context{"Fooyin.MainWindow"}, self)}
        , playlistController{std::make_unique<PlaylistController>(playlistHandler, playerController, library,
                                                                  &selectionController, settingsManager)}
        , selectionController{actionManager, settingsManager, playlistController.get()}
        , searchController{new SearchController(editableLayout.get(), self)}
        , fileMenu{new FileMenu(actionManager, settingsManager, self)}
        , editMenu{new EditMenu(actionManager, settingsManager, self)}
        , viewMenu{new ViewMenu(actionManager, &selectionController, settingsManager, self)}
        , playbackMenu{new PlaybackMenu(actionManager, playerController, self)}
        , libraryMenu{new LibraryMenu(actionManager, library, settingsManager, self)}
        , helpMenu{new HelpMenu(actionManager, self)}
        , propertiesDialog{new PropertiesDialog(settingsManager, self)}
        , generalPage{settingsManager}
        , guiGeneralPage{&layoutProvider, editableLayout.get(), settingsManager}
        , libraryGeneralPage{actionManager, libraryManager, settingsManager}
        , librarySortingPage{actionManager, settingsManager}
        , shortcutsPage{actionManager, settingsManager}
        , playlistGeneralPage{settingsManager}
        , playlistPresetsPage{settingsManager}
        , playlistColumnPage{actionManager, settingsManager}
        , enginePage{settingsManager, engine}
        , dirBrowserPage{settingsManager}
        , libraryTreePage{actionManager, settingsManager}
        , libraryTreeGuiPage{settingsManager}
        , statusWidgetPage{settingsManager}
        , pluginPage{settingsManager, pluginManager}
        , guiPluginContext{actionManager,    &layoutProvider, &selectionController, searchController,
                           propertiesDialog, &widgetProvider, editableLayout.get()}
    {
        setupConnections();
        registerActions();
        restoreIconTheme();
        registerLayouts();
        registerWidgets();
        createPropertiesTabs();

        actionManager->addContextObject(mainContext);

        pluginManager->initialisePlugins<GuiPlugin>(
            [this](GuiPlugin* plugin) { plugin->initialise(guiPluginContext); });

        layoutProvider.findLayouts();
        editableLayout->initialise();
        mainWindow->setCentralWidget(editableLayout.get());

        auto openMainWindow = [this]() {
            mainWindow->open();
            if(settingsManager->value<Settings::Core::FirstRun>()) {
                QMetaObject::invokeMethod(editableLayout.get(), &EditableLayout::showQuickSetup, Qt::QueuedConnection);
            }
        };

        if(libraryManager->hasLibrary() && settingsManager->value<Settings::Gui::WaitForTracks>()) {
            connect(library, &MusicLibrary::tracksLoaded, openMainWindow);
        }
        else {
            openMainWindow();
        }
    }

    void setupConnections()
    {
        QObject::connect(&selectionController, &TrackSelectionController::actionExecuted, playlistController.get(),
                         &PlaylistController::handleTrackSelectionAction);
        QObject::connect(&selectionController, &TrackSelectionController::requestPropertiesDialog, propertiesDialog,
                         &PropertiesDialog::show);
        QObject::connect(fileMenu, &FileMenu::requestNewPlaylist, self, [this]() {
            if(auto* playlist = playlistHandler->createEmptyPlaylist()) {
                playlistController->changeCurrentPlaylist(playlist);
            }
        });
        QObject::connect(fileMenu, &FileMenu::requestAddFiles, self, [this]() { addFiles(); });
        QObject::connect(fileMenu, &FileMenu::requestAddFolders, self, [this]() { addFolders(); });
        QObject::connect(viewMenu, &ViewMenu::openQuickSetup, editableLayout.get(), &EditableLayout::showQuickSetup);
        QObject::connect(engine, &EngineController::trackStatusChanged, self, [this](TrackStatus status) {
            if(status == InvalidTrack) {
                const Track track = playerController->currentTrack();
                if(track.isValid() && !QFileInfo::exists(track.filepath())) {
                    showTrackNotFoundMessage(track);
                }
            }
        });
    }

    void registerActions()
    {
        auto* muteAction
            = new QAction(Utils::iconFromTheme(Constants::Icons::VolumeMute), tr("Mute"), mainWindow.get());
        actionManager->registerAction(muteAction, Constants::Actions::Mute);
        QObject::connect(muteAction, &QAction::triggered, mainWindow.get(), [this]() {
            const double volume = settingsManager->value<Settings::Core::OutputVolume>();
            if(volume > 0.0) {
                settingsManager->set<Settings::Core::Internal::MuteVolume>(volume);
                settingsManager->set<Settings::Core::OutputVolume>(0.0);
            }
            else {
                settingsManager->set<Settings::Core::OutputVolume>(
                    settingsManager->value<Settings::Core::Internal::MuteVolume>());
            }
        });
    }

    void restoreIconTheme()
    {
        using namespace Settings::Gui::Internal;

        const auto iconTheme = static_cast<IconThemeOption>(settingsManager->value<Settings::Gui::IconTheme>());
        switch(iconTheme) {
            case(IconThemeOption::AutoDetect):
                QIcon::setThemeName(Utils::isDarkMode() ? QString::fromLatin1(Constants::DarkIconTheme)
                                                        : QString::fromLatin1(Constants::LightIconTheme));
                break;
            case(IconThemeOption::System):
                QIcon::setThemeName(QIcon::themeName());
                break;
            case(IconThemeOption::Light):
                QIcon::setThemeName(QString::fromLatin1(Constants::LightIconTheme));
                break;
            case(IconThemeOption::Dark):
                QIcon::setThemeName(QString::fromLatin1(Constants::DarkIconTheme));
                break;
        }
    }

    void registerLayouts()
    {
        layoutProvider.registerLayout(R"({"Empty": [{}]})");

        layoutProvider.registerLayout(
            R"({"Simple":[{"SplitterVertical":{"State":"AAAA/wAAAAEAAAADAAAAFgAAA6YAAAAXAP////8BAAAAAgA=",
            "Widgets":[{"StatusBar":{}},{"SplitterHorizontal":{"State":"AAAA/wAAAAEAAAACAAABYAAABeoA/////wEAAAABAA==",
            "Widgets":[{"LibraryTree":{"Grouping":"Artist/Album","ID":"8c3bf224ae774bd780cc2ff3ad638081"}},
            {"SplitterVertical":{"State":"AAAA/wAAAAEAAAABAAAAGwD/////AQAAAAIA",
            "Widgets":[{"PlaylistTabs":{"Widgets":[{"Playlist":{}}]}}]}}]}},
            {"SplitterHorizontal":{"State":"AAAA/wAAAAEAAAAEAAAAcgAAAswAAAA2AAAAGAD/////AQAAAAEA",
            "Widgets":[{"PlayerControls":{}},{"SeekBar":{}},{"PlaylistControls":{}},{"VolumeControls":{}}]}}]}}]})");

        layoutProvider.registerLayout(
            R"({"Vision":[{"SplitterVertical":{"State":"AAAA/wAAAAEAAAADAAAAHAAAA6EAAAAWAP////8BAAAAAgA=",
            "Widgets":[{"SplitterHorizontal":{"State":"AAAA/wAAAAEAAAAEAAAAiQAABk8AAABHAAAAIwD/////AQAAAAEA",
            "Widgets":[{"PlayerControls":{}},{"SeekBar":{}},{"PlaylistControls":{}},{"VolumeControls":{}}]}},
            {"SplitterHorizontal":{"State":"AAAA/wAAAAEAAAACAAADuwAAA48A/////wEAAAABAA==","Widgets":[{"TabStack":{
            "Position":"West","State":"Artwork\u001fInfo\u001fLibrary Tree\u001fPlaylist Organiser",
            "Widgets":[{"ArtworkPanel":{}},{"SelectionInfo":{}},{"LibraryTree":{"Grouping":"Artist/Album"}},
            {"PlaylistOrganiser":{}}]}},{"SplitterVertical":{"State":"AAAA/wAAAAEAAAABAAAAwAD/////AQAAAAIA",
            "Widgets":[{"Playlist":{}}]}}]}},{"StatusBar":{}}]}}]})");

        layoutProvider.registerLayout(
            R"({"Browser":[{"SplitterVertical":{"State":"AAAA/wAAAAEAAAADAAAAFwAAA6YAAAAWAP////8BAAAAAgA=",
            "Widgets":[{"SplitterHorizontal":{"State":"AAAA/wAAAAEAAAAEAAAAcgAABRoAAAA2AAAAGAD/////AQAAAAEA",
            "Widgets":[{"PlayerControls":{}},{"SeekBar":{}},{"PlaylistControls":{}},{"VolumeControls":{}}]}},
            {"SplitterHorizontal":{"State":"AAAA/wAAAAEAAAACAAACeAAAAnoA/////wEAAAABAA==x","Widgets":[{"DirectoryBrowser":{}},
            {"ArtworkPanel":{}}]}},{"StatusBar":{}}]}}]})");
    }

    void registerWidgets()
    {
        widgetProvider.registerWidget(
            QStringLiteral("SplitterVertical"),
            [this]() {
                auto* splitter
                    = new VerticalSplitterWidget(actionManager, &widgetProvider, settingsManager, mainWindow.get());
                splitter->showPlaceholder(true);
                return splitter;
            },
            QStringLiteral("Vertical Splitter"));
        widgetProvider.setSubMenus(QStringLiteral("SplitterVertical"), {QStringLiteral("Splitters")});

        widgetProvider.registerWidget(
            QStringLiteral("SplitterHorizontal"),
            [this]() {
                auto* splitter
                    = new HorizontalSplitterWidget(actionManager, &widgetProvider, settingsManager, mainWindow.get());
                splitter->showPlaceholder(true);
                return splitter;
            },
            QStringLiteral("Horizontal Splitter"));
        widgetProvider.setSubMenus(QStringLiteral("SplitterHorizontal"), {QStringLiteral("Splitters")});

        widgetProvider.registerWidget(
            QStringLiteral("PlaylistTabs"),
            [this]() {
                return new PlaylistTabs(actionManager, &widgetProvider, playlistController.get(), settingsManager,
                                        mainWindow.get());
            },
            QStringLiteral("Playlist Tabs"));
        widgetProvider.setSubMenus(QStringLiteral("PlaylistTabs"), {QStringLiteral("Splitters")});

        widgetProvider.registerWidget(
            QStringLiteral("PlaylistOrganiser"),
            [this]() {
                return new PlaylistOrganiser(actionManager, playlistController.get(), settingsManager,
                                             mainWindow.get());
            },
            QStringLiteral("Playlist Organiser"));

        widgetProvider.registerWidget(
            QStringLiteral("TabStack"),
            [this]() { return new TabStackWidget(actionManager, &widgetProvider, mainWindow.get()); },
            QStringLiteral("Tab Stack"));
        widgetProvider.setSubMenus(QStringLiteral("TabStack"), {QStringLiteral("Splitters")});

        widgetProvider.registerWidget(
            QStringLiteral("LibraryTree"),
            [this]() {
                return new LibraryTreeWidget(library, &selectionController, settingsManager, mainWindow.get());
            },
            QStringLiteral("Library Tree"));

        widgetProvider.registerWidget(
            QStringLiteral("PlayerControls"),
            [this]() { return new PlayerControl(actionManager, playerController, settingsManager, mainWindow.get()); },
            QStringLiteral("Player Controls"));
        widgetProvider.setSubMenus(QStringLiteral("PlayerControls"), {QStringLiteral("Controls")});

        widgetProvider.registerWidget(
            QStringLiteral("PlaylistControls"),
            [this]() { return new PlaylistControl(playerController, settingsManager, mainWindow.get()); },
            QStringLiteral("Playlist Controls"));
        widgetProvider.setSubMenus(QStringLiteral("PlaylistControls"), {QStringLiteral("Controls")});

        widgetProvider.registerWidget(
            QStringLiteral("VolumeControls"),
            [this]() { return new VolumeControl(actionManager, settingsManager, mainWindow.get()); },
            QStringLiteral("Volume Controls"));
        widgetProvider.setSubMenus(QStringLiteral("VolumeControls"), {QStringLiteral("Controls")});

        widgetProvider.registerWidget(
            QStringLiteral("SeekBar"),
            [this]() { return new SeekBar(playerController, settingsManager, mainWindow.get()); },
            QStringLiteral("SeekBar"));
        widgetProvider.setSubMenus(QStringLiteral("SeekBar"), {QStringLiteral("Controls")});

        widgetProvider.registerWidget(
            QStringLiteral("SelectionInfo"),
            [this]() {
                return new InfoWidget(playerController, &selectionController, settingsManager, mainWindow.get());
            },
            QStringLiteral("Selection Info"));

        widgetProvider.registerWidget(
            QStringLiteral("ArtworkPanel"),
            [this]() { return new CoverWidget(playerController, &selectionController, mainWindow.get()); },
            QStringLiteral("Artwork Panel"));

        widgetProvider.registerWidget(QStringLiteral("Playlist"), [this]() {
            return new PlaylistWidget(actionManager, playlistController.get(), library, settingsManager,
                                      mainWindow.get());
        });
        widgetProvider.setLimit(QStringLiteral("Playlist"), 1);

        widgetProvider.registerWidget(QStringLiteral("Spacer"), [this]() { return new Spacer(mainWindow.get()); });

        widgetProvider.registerWidget(
            QStringLiteral("StatusBar"),
            [this]() {
                auto* statusWidget
                    = new StatusWidget(playerController, &selectionController, settingsManager, mainWindow.get());
                QObject::connect(library, &MusicLibrary::scanProgress, statusWidget,
                                 &StatusWidget::libraryScanProgress);
                return statusWidget;
            },
            QStringLiteral("Status Bar"));

        widgetProvider.registerWidget(
            QStringLiteral("SearchBar"),
            [this]() { return new SearchWidget(searchController, settingsManager, mainWindow.get()); },
            QStringLiteral("Search Bar"));

        widgetProvider.registerWidget(
            QStringLiteral("DirectoryBrowser"),
            [this]() {
                return new DirBrowser(&selectionController, playlistController.get(), settingsManager,
                                      mainWindow.get());
            },
            QStringLiteral("Directory Browser"));
        widgetProvider.setLimit(QStringLiteral("DirectoryBrowser"), 1);
    }

    void createPropertiesTabs()
    {
        propertiesDialog->addTab(QStringLiteral("Details"), [this]() {
            return new InfoWidget(playerController, &selectionController, settingsManager);
        });
    }

    void showTrackNotFoundMessage(const Track& track) const
    {
        QMessageBox message;
        message.setIcon(QMessageBox::Warning);
        message.setText(QStringLiteral("Track Not Found"));
        message.setInformativeText(track.filepath());

        message.addButton(QMessageBox::Ok);
        QPushButton* stopButton = message.addButton(QStringLiteral("Stop"), QMessageBox::ActionRole);
        stopButton->setIcon(Utils::iconFromTheme(Constants::Icons::Stop));
        message.setDefaultButton(QMessageBox::Ok);

        message.exec();

        if(message.clickedButton() == stopButton) {
            playerController->stop();
        }
        else {
            playerController->next();
        }
    }

    void addFiles() const
    {
        const auto extensions = QString{QStringLiteral("Audio Files (%1)")}.arg(
            Track::supportedFileExtensions().join(QStringLiteral(" ")));

        const auto files = QFileDialog::getOpenFileUrls(mainWindow.get(), QStringLiteral("Add Files"),
                                                        QStringLiteral(""), extensions);

        if(files.empty()) {
            return;
        }

        playlistController->filesToCurrentPlaylist(files);
    }

    void addFolders() const
    {
        const auto dirs
            = QFileDialog::getExistingDirectoryUrl(mainWindow.get(), QStringLiteral("Add Folders"), QStringLiteral(""));

        if(dirs.isEmpty()) {
            return;
        }

        playlistController->filesToCurrentPlaylist({dirs});
    }

    void openFiles(const QList<QUrl>& urls) const
    {
        playlistController->filesToNewPlaylist(QStringLiteral("Default"), urls);
    }
};

GuiApplication::GuiApplication(const CorePluginContext& core)
    : p{std::make_unique<Private>(this, core)}
{ }

GuiApplication::~GuiApplication() = default;

void GuiApplication::shutdown()
{
    p->actionManager->saveSettings();
    p->editableLayout->saveLayout();
    p->editableLayout.reset();
    p->playlistController.reset();
    p->mainWindow.reset();
}

void GuiApplication::openFiles(const QList<QUrl>& files)
{
    if(p->playlistController->playlistsHaveLoaded()) {
        p->openFiles(files);
        return;
    }

    QObject::connect(
        p->playlistController.get(), &PlaylistController::playlistsLoaded, this,
        [this, files]() { p->openFiles(files); }, Qt::SingleShotConnection);
}
} // namespace Fooyin

#include "moc_guiapplication.cpp"
