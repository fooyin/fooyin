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
#include "core/application.h"
#include "core/corepaths.h"
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
#include "playlist/playlistbox.h"
#include "playlist/playlistcontroller.h"
#include "playlist/playlistinteractor.h"
#include "playlist/playlisttabs.h"
#include "playlist/playlistwidget.h"
#include "sandbox/sandboxdialog.h"
#include "search/searchcontroller.h"
#include "search/searchwidget.h"
#include "settings/artworkpage.h"
#include "settings/dirbrowser/dirbrowserpage.h"
#include "settings/enginepage.h"
#include "settings/generalpage.h"
#include "settings/guigeneralpage.h"
#include "settings/library/librarygeneralpage.h"
#include "settings/library/librarysortingpage.h"
#include "settings/librarytree/librarytreegrouppage.h"
#include "settings/librarytree/librarytreepage.h"
#include "settings/playlist/playlistcolumnpage.h"
#include "settings/playlist/playlistgeneralpage.h"
#include "settings/playlist/playlistpresetspage.h"
#include "settings/plugins/pluginspage.h"
#include "settings/shortcuts/shortcutspage.h"
#include "settings/widgets/statuswidgetpage.h"
#include "systemtrayicon.h"
#include "utils/actions/command.h"
#include "widgets/coverwidget.h"
#include "widgets/dummy.h"
#include "widgets/lyricswidget.h"
#include "widgets/spacer.h"
#include "widgets/splitterwidget.h"
#include "widgets/statuswidget.h"
#include "widgets/tabstackwidget.h"

#include <core/coresettings.h>
#include <core/engine/enginehandler.h>
#include <core/library/librarymanager.h>
#include <core/library/musiclibrary.h>
#include <core/playlist/playlisthandler.h>
#include <core/playlist/playlistparser.h>
#include <core/playlist/playlistparserregistry.h>
#include <core/plugins/coreplugincontext.h>
#include <core/plugins/pluginmanager.h>
#include <gui/coverprovider.h>
#include <gui/editablelayout.h>
#include <gui/guiconstants.h>
#include <gui/guisettings.h>
#include <gui/layoutprovider.h>
#include <gui/plugins/guiplugin.h>
#include <gui/plugins/guiplugincontext.h>
#include <gui/propertiesdialog.h>
#include <gui/trackselectioncontroller.h>
#include <gui/widgetprovider.h>
#include <gui/windowcontroller.h>
#include <utils/actions/actionmanager.h>
#include <utils/settings/settingsdialogcontroller.h>
#include <utils/settings/settingsmanager.h>
#include <utils/utils.h>

#include <QAction>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QPixmapCache>
#include <QPushButton>

constexpr auto LastFilePath = "Interface/LastFilePath";

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
    PlaylistParserRegistry* playlistParsers;

    WidgetProvider widgetProvider;
    GuiSettings guiSettings;
    LayoutProvider layoutProvider;
    std::unique_ptr<EditableLayout> editableLayout;
    std::unique_ptr<MainMenuBar> menubar;
    std::unique_ptr<MainWindow> mainWindow;
    std::unique_ptr<SystemTrayIcon> trayIcon;
    WidgetContext* mainContext;
    std::unique_ptr<PlaylistController> playlistController;
    PlaylistInteractor playlistInteractor;
    TrackSelectionController selectionController;
    SearchController* searchController;

    FileMenu* fileMenu;
    EditMenu* editMenu;
    ViewMenu* viewMenu;
    PlaybackMenu* playbackMenu;
    LibraryMenu* libraryMenu;
    HelpMenu* helpMenu;

    PropertiesDialog* propertiesDialog;
    WindowController* windowController;

    GeneralPage generalPage;
    GuiGeneralPage guiGeneralPage;
    ArtworkPage artworkPage;
    LibraryGeneralPage libraryGeneralPage;
    LibrarySortingPage librarySortingPage;
    ShortcutsPage shortcutsPage;
    PlaylistGeneralPage playlistGeneralPage;
    PlaylistPresetsPage playlistPresetsPage;
    PlaylistColumnPage playlistColumnPage;
    EnginePage enginePage;
    DirBrowserPage dirBrowserPage;
    LibraryTreePage libraryTreePage;
    LibraryTreeGroupPage libraryTreeGroupPage;
    StatusWidgetPage statusWidgetPage;
    PluginPage pluginPage;

    GuiPluginContext guiPluginContext;

    ScriptParser scriptParser;

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
        , playlistParsers{core.playlistParsers}
        , guiSettings{settingsManager}
        , editableLayout{std::make_unique<EditableLayout>(actionManager, &widgetProvider, &layoutProvider,
                                                          settingsManager)}
        , menubar{std::make_unique<MainMenuBar>(actionManager)}
        , mainWindow{std::make_unique<MainWindow>(actionManager, menubar.get(), settingsManager)}
        , mainContext{new WidgetContext(mainWindow.get(), Context{"Fooyin.MainWindow"}, self)}
        , playlistController{std::make_unique<PlaylistController>(playlistHandler, playerController,
                                                                  &selectionController, settingsManager)}
        , playlistInteractor{core.playlistHandler, playlistController.get(), core.library}
        , selectionController{actionManager, settingsManager, playlistController.get()}
        , searchController{new SearchController(editableLayout.get(), self)}
        , fileMenu{new FileMenu(actionManager, settingsManager, self)}
        , editMenu{new EditMenu(actionManager, settingsManager, self)}
        , viewMenu{new ViewMenu(actionManager, settingsManager, self)}
        , playbackMenu{new PlaybackMenu(actionManager, playerController, settingsManager, self)}
        , libraryMenu{new LibraryMenu(actionManager, library, settingsManager, self)}
        , helpMenu{new HelpMenu(actionManager, self)}
        , propertiesDialog{new PropertiesDialog(settingsManager, self)}
        , windowController{new WindowController(mainWindow.get())}
        , generalPage{settingsManager}
        , guiGeneralPage{&layoutProvider, editableLayout.get(), settingsManager}
        , artworkPage{settingsManager}
        , libraryGeneralPage{actionManager, libraryManager, library, settingsManager}
        , librarySortingPage{actionManager, settingsManager}
        , shortcutsPage{actionManager, settingsManager}
        , playlistGeneralPage{settingsManager}
        , playlistPresetsPage{settingsManager}
        , playlistColumnPage{actionManager, settingsManager}
        , enginePage{settingsManager, engine}
        , dirBrowserPage{settingsManager}
        , libraryTreePage{settingsManager}
        , libraryTreeGroupPage{actionManager, settingsManager}
        , statusWidgetPage{settingsManager}
        , pluginPage{settingsManager, pluginManager}
        , guiPluginContext{actionManager,    &layoutProvider, &selectionController, searchController,
                           propertiesDialog, &widgetProvider, editableLayout.get(), windowController}
    {
        setupConnections();
        registerActions();
        restoreIconTheme();
        registerLayouts();
        registerWidgets();
        createPropertiesTabs();

        actionManager->addContextObject(mainContext);

        initialisePlugins();
        layoutProvider.findLayouts();
        editableLayout->initialise();
        mainWindow->setCentralWidget(editableLayout.get());

        auto openMainWindow = [this]() {
            mainWindow->open();
            if(settingsManager->value<Settings::Core::FirstRun>()) {
                QMetaObject::invokeMethod(editableLayout.get(), &EditableLayout::showQuickSetup, Qt::QueuedConnection);
            }
        };

        if(libraryManager->hasLibrary() && library->isEmpty()
           && settingsManager->value<Settings::Gui::WaitForTracks>()) {
            connect(library, &MusicLibrary::tracksLoaded, openMainWindow);
        }
        else {
            openMainWindow();
        }

        initialiseTray();
    }

    void initialisePlugins()
    {
        if(pluginManager->allPluginInfo().empty()) {
            QMetaObject::invokeMethod(
                self, []() { showPluginsNotFoundMessage(); }, Qt::QueuedConnection);
            return;
        }

        pluginManager->initialisePlugins<GuiPlugin>(
            [this](GuiPlugin* plugin) { plugin->initialise(guiPluginContext); });
    }

    static void showPluginsNotFoundMessage()
    {
        QMessageBox message;
        message.setIcon(QMessageBox::Warning);
        message.setText(tr("Plugins not found"));
        message.setInformativeText(tr("Some plugins are required for full functionality."));
        message.setDetailedText(tr("Plugin search locations:\n\n") + Core::pluginPaths().join(QStringLiteral("\n")));

        message.addButton(QMessageBox::Ok);
        QPushButton* quitButton = message.addButton(tr("Quit"), QMessageBox::ActionRole);
        quitButton->setIcon(Utils::iconFromTheme(Constants::Icons::Quit));
        message.setDefaultButton(QMessageBox::Ok);

        message.exec();

        if(message.clickedButton() == quitButton) {
            Application::quit();
        }
    }

    void initialiseTray()
    {
        trayIcon = std::make_unique<SystemTrayIcon>(actionManager);

        QObject::connect(trayIcon.get(), &SystemTrayIcon::toggleVisibility, mainWindow.get(),
                         &MainWindow::toggleVisibility);

        if(settingsManager->value<Settings::Gui::Internal::ShowTrayIcon>()) {
            trayIcon->show();
        }

        settingsManager->subscribe<Settings::Gui::Internal::ShowTrayIcon>(
            self, [this](bool show) { trayIcon->setVisible(show); });
    }

    void updateWindowTitle()
    {
        if(playerController->playState() == PlayState::Stopped) {
            mainWindow->resetTitle();
            return;
        }

        const Track currentTrack = playerController->currentTrack();
        if(!currentTrack.isValid()) {
            mainWindow->resetTitle();
            return;
        }

        const QString script = settingsManager->value<Settings::Gui::Internal::WindowTitleTrackScript>();
        const QString title  = scriptParser.evaluate(script, currentTrack);
        mainWindow->setTitle(title);
    }

    static void removeExpiredCovers(const TrackList& tracks)
    {
        for(const Track& track : tracks) {
            if(track.metadataWasModified()) {
                CoverProvider::removeFromCache(track);
            }
        }
    }

    void setupConnections()
    {
        QObject::connect(library, &MusicLibrary::tracksUpdated, self,
                         [](const TrackList& tracks) { removeExpiredCovers(tracks); });

        QObject::connect(playerController, &PlayerController::playStateChanged, mainWindow.get(),
                         [this]() { updateWindowTitle(); });
        settingsManager->subscribe<Settings::Gui::Internal::WindowTitleTrackScript>(self,
                                                                                    [this]() { updateWindowTitle(); });
        QObject::connect(playerController, &PlayerController::currentTrackChanged, self,
                         [this]() { updateWindowTitle(); });
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
        QObject::connect(fileMenu, &FileMenu::requestLoadPlaylist, self, [this]() { loadPlaylist(); });
        QObject::connect(fileMenu, &FileMenu::requestSavePlaylist, self, [this]() { savePlaylist(); });
        QObject::connect(viewMenu, &ViewMenu::openQuickSetup, editableLayout.get(), &EditableLayout::showQuickSetup);
        QObject::connect(viewMenu, &ViewMenu::openScriptSandbox, self, [this]() {
            auto* sandboxDialog = new SandboxDialog(&selectionController, settingsManager, mainWindow.get());
            sandboxDialog->setAttribute(Qt::WA_DeleteOnClose);
            sandboxDialog->show();
        });
        QObject::connect(viewMenu, &ViewMenu::showNowPlaying, self, [this]() {
            if(auto* activePlaylist = playlistHandler->activePlaylist()) {
                playlistController->showNowPlaying();
                playlistController->changeCurrentPlaylist(activePlaylist);
            }
        });
        QObject::connect(engine, &EngineController::trackStatusChanged, self, [this](TrackStatus status) {
            if(status == TrackStatus::InvalidTrack) {
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

        QObject::connect(playlistController.get(), &PlaylistController::currentPlaylistChanged, mainWindow.get(),
                         [this]() {
                             if(auto* savePlaylistCommand = actionManager->command(Constants::Actions::SavePlaylist)) {
                                 if(const auto* playlist = playlistController->currentPlaylist()) {
                                     savePlaylistCommand->action()->setEnabled(playlist->trackCount() > 0);
                                 }
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

        QIcon::setFallbackThemeName(settingsManager->value<SystemIconTheme>());
    }

    void registerLayouts()
    {
        layoutProvider.registerLayout(R"({"Name":"Empty"})");

        layoutProvider.registerLayout(
            R"({"Name":"Simple","Widgets":[{"SplitterVertical":{"State":"AAAA/wAAAAEAAAADAAAAHAAAAn0AAAAXAP////8BAAAAAgA=",
            "Widgets":[{"SplitterHorizontal":{"State":"AAAA/wAAAAEAAAAEAAAAggAABqEAAAA+AAAAHAD/////AQAAAAEA","Widgets":[
            {"PlayerControls":{}},{"SeekBar":{}},{"PlaylistControls":{}},{"VolumeControls":{}}]}},{"SplitterHorizontal":{
            "State":"AAAA/wAAAAEAAAACAAAA9QAABAoA/////wEAAAABAA==","Widgets":[{"SplitterVertical":{
            "State":"AAAA/wAAAAEAAAACAAABfQAAAPEA/////wEAAAACAA==","Widgets":[{"LibraryTree":{}},{"ArtworkPanel":{}}]}},
            {"PlaylistTabs":{"Widgets":[{"Playlist":{}}]}}]}},{"StatusBar":{}}]}}]})");

        layoutProvider.registerLayout(
            R"({"Name":"Vision","Widgets":[{"SplitterVertical":{"State":"AAAA/wAAAAEAAAADAAAAHAAAA6EAAAAWAP////8BAAAAAgA=",
            "Widgets":[{"SplitterHorizontal":{"State":"AAAA/wAAAAEAAAAEAAAAiQAABk8AAABHAAAAIwD/////AQAAAAEA",
            "Widgets":[{"PlayerControls":{}},{"SeekBar":{}},{"PlaylistControls":{}},{"VolumeControls":{}}]}},
            {"SplitterHorizontal":{"State":"AAAA/wAAAAEAAAACAAADuwAAA48A/////wEAAAABAA==","Widgets":[
            {"TabStack":{"Position":"West","State":"Artwork\u001fInfo\u001fLibrary Tree\u001fPlaylist Organiser",
            "Widgets":[{"ArtworkPanel":{}},{"SelectionInfo":{}},{"LibraryTree":{"Grouping":"Artist/Album"}},
            {"PlaylistOrganiser":{}}]}},{"Playlist":{}}]}},{"StatusBar":{}}]}}]})");

        layoutProvider.registerLayout(
            R"({"Name":"Browser","Widgets":[{"SplitterVertical":{"State":"AAAA/wAAAAEAAAADAAAAFwAAA6YAAAAWAP////8BAAAAAgA=",
            "Widgets":[{"SplitterHorizontal":{"State":"AAAA/wAAAAEAAAAEAAAAcgAABRoAAAA2AAAAGAD/////AQAAAAEA",
            "Widgets":[{"PlayerControls":{}},{"SeekBar":{}},{"PlaylistControls":{}},{"VolumeControls":{}}]}},
            {"SplitterHorizontal":{"State":"AAAA/wAAAAEAAAACAAACeAAAAnoA/////wEAAAABAA==x","Widgets":[{"DirectoryBrowser":{}},
            {"ArtworkPanel":{}}]}},{"StatusBar":{}}]}}]})");
    }

    void registerWidgets()
    {
        widgetProvider.registerWidget(
            QStringLiteral("Dummy"), [this]() { return new Dummy(settingsManager, mainWindow.get()); }, tr("Dummy"));
        widgetProvider.setIsHidden(QStringLiteral("Dummy"), true);

        widgetProvider.registerWidget(
            QStringLiteral("SplitterVertical"),
            [this]() { return new VerticalSplitterWidget(&widgetProvider, settingsManager, mainWindow.get()); },
            tr("Vertical Splitter"));
        widgetProvider.setSubMenus(QStringLiteral("SplitterVertical"), {tr("Splitters")});

        widgetProvider.registerWidget(
            QStringLiteral("SplitterHorizontal"),
            [this]() { return new HorizontalSplitterWidget(&widgetProvider, settingsManager, mainWindow.get()); },
            tr("Horizontal Splitter"));
        widgetProvider.setSubMenus(QStringLiteral("SplitterHorizontal"), {tr("Splitters")});

        widgetProvider.registerWidget(
            QStringLiteral("PlaylistSwitcher"),
            [this]() { return new PlaylistBox(playlistController.get(), mainWindow.get()); }, tr("Playlist Switcher"));

        widgetProvider.registerWidget(
            QStringLiteral("PlaylistTabs"),
            [this]() {
                return new PlaylistTabs(actionManager, &widgetProvider, playlistController.get(), settingsManager,
                                        mainWindow.get());
            },
            tr("Playlist Tabs"));
        widgetProvider.setSubMenus(QStringLiteral("PlaylistTabs"), {tr("Splitters")});

        widgetProvider.registerWidget(
            QStringLiteral("PlaylistOrganiser"),
            [this]() {
                return new PlaylistOrganiser(actionManager, &playlistInteractor, settingsManager, mainWindow.get());
            },
            tr("Playlist Organiser"));

        widgetProvider.registerWidget(
            QStringLiteral("TabStack"),
            [this]() { return new TabStackWidget(&widgetProvider, settingsManager, mainWindow.get()); },
            tr("Tab Stack"));
        widgetProvider.setSubMenus(QStringLiteral("TabStack"), {tr("Splitters")});

        widgetProvider.registerWidget(
            QStringLiteral("LibraryTree"),
            [this]() {
                return new LibraryTreeWidget(library, &selectionController, settingsManager, mainWindow.get());
            },
            tr("Library Tree"));

        widgetProvider.registerWidget(
            QStringLiteral("PlayerControls"),
            [this]() { return new PlayerControl(actionManager, playerController, settingsManager, mainWindow.get()); },
            tr("Player Controls"));
        widgetProvider.setSubMenus(QStringLiteral("PlayerControls"), {tr("Controls")});

        widgetProvider.registerWidget(
            QStringLiteral("PlaylistControls"),
            [this]() { return new PlaylistControl(playerController, settingsManager, mainWindow.get()); },
            tr("Playlist Controls"));
        widgetProvider.setSubMenus(QStringLiteral("PlaylistControls"), {tr("Controls")});

        widgetProvider.registerWidget(
            QStringLiteral("VolumeControls"),
            [this]() { return new VolumeControl(actionManager, settingsManager, mainWindow.get()); },
            tr("Volume Controls"));
        widgetProvider.setSubMenus(QStringLiteral("VolumeControls"), {tr("Controls")});

        widgetProvider.registerWidget(
            QStringLiteral("SeekBar"),
            [this]() { return new SeekBar(playerController, settingsManager, mainWindow.get()); }, tr("Seekbar"));
        widgetProvider.setSubMenus(QStringLiteral("SeekBar"), {tr("Controls")});

        widgetProvider.registerWidget(
            QStringLiteral("SelectionInfo"),
            [this]() {
                return new InfoWidget(playerController, &selectionController, settingsManager, mainWindow.get());
            },
            tr("Selection Info"));

        widgetProvider.registerWidget(
            QStringLiteral("ArtworkPanel"),
            [this]() {
                return new CoverWidget(playerController, &selectionController, settingsManager, mainWindow.get());
            },
            tr("Artwork Panel"));

        widgetProvider.registerWidget(
            QStringLiteral("Lyrics"), [this]() { return new LyricsWidget(playerController, mainWindow.get()); },
            tr("Lyrics"));
        widgetProvider.setLimit(QStringLiteral("Lyrics"), 1);

        widgetProvider.registerWidget(
            QStringLiteral("Playlist"),
            [this]() {
                return new PlaylistWidget(actionManager, &playlistInteractor, settingsManager, mainWindow.get());
            },
            tr("Playlist"));
        widgetProvider.setLimit(QStringLiteral("Playlist"), 1);

        widgetProvider.registerWidget(
            QStringLiteral("Spacer"), [this]() { return new Spacer(mainWindow.get()); }, tr("Spacer"));

        widgetProvider.registerWidget(
            QStringLiteral("StatusBar"),
            [this]() {
                auto* statusWidget
                    = new StatusWidget(playerController, &selectionController, settingsManager, mainWindow.get());
                QObject::connect(library, &MusicLibrary::scanProgress, statusWidget,
                                 &StatusWidget::libraryScanProgress);
                return statusWidget;
            },
            tr("Status Bar"));
        widgetProvider.setLimit(QStringLiteral("StatusBar"), 1);

        widgetProvider.registerWidget(
            QStringLiteral("SearchBar"),
            [this]() { return new SearchWidget(searchController, settingsManager, mainWindow.get()); },
            tr("Search Bar"));

        widgetProvider.registerWidget(
            QStringLiteral("DirectoryBrowser"),
            [this]() {
                auto* browser = new DirBrowser(&playlistInteractor, settingsManager, mainWindow.get());
                QObject::connect(playerController, &PlayerController::playStateChanged, browser,
                                 &DirBrowser::playstateChanged);
                QObject::connect(playerController, &PlayerController::playlistTrackChanged, browser,
                                 &DirBrowser::playlistTrackChanged);
                QObject::connect(playlistHandler, &PlaylistHandler::activePlaylistChanged, browser,
                                 &DirBrowser::activePlaylistChanged);
                return browser;
            },
            tr("Directory Browser"));
        widgetProvider.setLimit(QStringLiteral("DirectoryBrowser"), 1);
    }

    void createPropertiesTabs()
    {
        propertiesDialog->addTab(tr("Details"), [this]() {
            return new InfoWidget(playerController, &selectionController, settingsManager);
        });
    }

    void showTrackNotFoundMessage(const Track& track) const
    {
        QMessageBox message;
        message.setIcon(QMessageBox::Warning);
        message.setText(tr("Track Not Found"));
        message.setInformativeText(track.filepath());

        message.addButton(QMessageBox::Ok);
        QPushButton* stopButton = message.addButton(tr("Stop"), QMessageBox::ActionRole);
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
        const QString audioExtensions    = Track::supportedFileExtensions().join(QStringLiteral(" "));
        const QString playlistExtensions = Playlist::supportedPlaylistExtensions().join(QStringLiteral(" "));
        const QString allExtensions      = QStringLiteral("%1 %2").arg(audioExtensions, playlistExtensions);

        const QString allFilter      = tr("All Supported Media Files (%1)").arg(allExtensions);
        const QString filesFilter    = tr("Audio Files (%1)").arg(audioExtensions);
        const QString playlistFilter = tr("Playlists (%1)").arg(playlistExtensions);

        const QStringList filters{allFilter, filesFilter, playlistFilter};

        QUrl dir = QUrl::fromLocalFile(QDir::homePath());
        if(const auto lastPath = settingsManager->fileValue(QString::fromLatin1(LastFilePath)).toString();
           !lastPath.isEmpty()) {
            dir = lastPath;
        }

        const auto files
            = QFileDialog::getOpenFileUrls(mainWindow.get(), tr("Add Files"), dir, filters.join(QStringLiteral(";;")));

        if(files.empty()) {
            return;
        }

        settingsManager->fileSet(QString::fromLatin1(LastFilePath), files.front());

        playlistInteractor.filesToCurrentPlaylist(files);
    }

    void addFolders() const
    {
        const auto dirs = QFileDialog::getExistingDirectoryUrl(mainWindow.get(), tr("Add Folders"), QDir::homePath());

        if(dirs.isEmpty()) {
            return;
        }

        playlistInteractor.filesToCurrentPlaylist({dirs});
    }

    void openFiles(const QList<QUrl>& urls) const
    {
        playlistInteractor.filesToNewPlaylist(QStringLiteral("Default"), urls, true);
    }

    void loadPlaylist() const
    {
        const QString playlistExtensions = Playlist::supportedPlaylistExtensions().join(QStringLiteral(" "));
        const QString playlistFilter     = tr("Playlists (%1)").arg(playlistExtensions);

        QUrl dir = QUrl::fromLocalFile(QDir::homePath());
        if(const auto lastPath = settingsManager->fileValue(QString::fromLatin1(LastFilePath)).toString();
           !lastPath.isEmpty()) {
            dir = lastPath;
        }

        const auto files = QFileDialog::getOpenFileUrls(mainWindow.get(), tr("Load Playlist"), dir, playlistFilter);

        if(files.empty()) {
            return;
        }

        settingsManager->fileSet(QString::fromLatin1(LastFilePath), files.front());

        const QFileInfo info{files.front().toLocalFile()};

        playlistInteractor.filesToNewPlaylist(info.completeBaseName(), files);
    }

    void savePlaylist() const
    {
        auto* playlist = playlistController->currentPlaylist();
        if(!playlist || playlist->trackCount() == 0) {
            return;
        }

        const QString playlistFilter
            = Utils::extensionsToFilterList(playlistParsers->supportedSaveExtensions(), QStringLiteral("files"));

        QUrl dir = QUrl::fromLocalFile(QDir::homePath());
        if(const auto lastPath = settingsManager->fileValue(QString::fromLatin1(LastFilePath)).toString();
           !lastPath.isEmpty()) {
            dir = lastPath;
        }

        QString selectedFilter;
        const auto file
            = QFileDialog::getSaveFileUrl(mainWindow.get(), tr("Save Playlist"), dir, playlistFilter, &selectedFilter);

        if(file.isEmpty()) {
            return;
        }

        settingsManager->fileSet(QString::fromLatin1(LastFilePath), file);

        const QString extension = Utils::extensionFromFilter(selectedFilter);
        if(extension.isEmpty()) {
            return;
        }

        if(auto* parser = playlistParsers->parserForExtension(extension)) {
            QFile playlistFile{file.toLocalFile()};
            if(!playlistFile.open(QIODevice::WriteOnly)) {
                qWarning() << QStringLiteral("Could not open playlist file %1 for writing: %2")
                                  .arg(playlistFile.fileName(), playlistFile.errorString());
                return;
            }

            const QFileInfo info{playlistFile};
            const QDir playlistDir{info.absolutePath()};
            const auto pathType
                = static_cast<PlaylistParser::PathType>(settingsManager->value<Settings::Core::PlaylistSavePathType>());
            const bool writeMetadata = settingsManager->value<Settings::Core::PlaylistSaveMetadata>();

            parser->savePlaylist(&playlistFile, extension, playlist->tracks(), playlistDir, pathType, writeMetadata);
        }
    }
};

GuiApplication::GuiApplication(const CorePluginContext& core)
    : p{std::make_unique<Private>(this, core)}
{
    auto updateCache = [](const int sizeMb) {
        QPixmapCache::setCacheLimit(sizeMb * 1024);
    };

    updateCache(p->settingsManager->value<Settings::Gui::Internal::PixmapCacheSize>());
    p->settingsManager->subscribe<Settings::Gui::Internal::PixmapCacheSize>(this, updateCache);
    p->settingsManager->subscribe<Settings::Gui::Internal::ArtworkThumbnailSize>(this, CoverProvider::clearCache);

    QObject::connect(p->settingsManager->settingsDialog(), &SettingsDialogController::opening, this, [this]() {
        const bool isLayoutEditing = p->settingsManager->value<Settings::Gui::LayoutEditing>();
        // Layout editing mode overrides the global action context, so disable it until the dialog closes
        p->settingsManager->set<Settings::Gui::LayoutEditing>(false);
        QObject::connect(
            p->settingsManager->settingsDialog(), &SettingsDialogController::closing, this,
            [this, isLayoutEditing]() { p->settingsManager->set<Settings::Gui::LayoutEditing>(isLayoutEditing); },
            Qt::SingleShotConnection);
    });
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

void GuiApplication::raise()
{
    p->mainWindow->raiseWindow();
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
