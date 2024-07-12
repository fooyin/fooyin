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
#include "queueviewer/queueviewer.h"
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
#include "settings/playbackpage.h"
#include "settings/playlist/playlistcolumnpage.h"
#include "settings/playlist/playlistgeneralpage.h"
#include "settings/playlist/playlistpresetspage.h"
#include "settings/plugins/pluginspage.h"
#include "settings/shortcuts/shortcutspage.h"
#include "settings/widgets/playbackqueuepage.h"
#include "settings/widgets/statuswidgetpage.h"
#include "systemtrayicon.h"
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
#include <core/playlist/playlistloader.h>
#include <core/playlist/playlistparser.h>
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
#include <utils/actions/actioncontainer.h>
#include <utils/actions/actionmanager.h>
#include <utils/actions/command.h>
#include <utils/settings/settingsdialogcontroller.h>
#include <utils/settings/settingsmanager.h>
#include <utils/utils.h>

#include <QAction>
#include <QCheckBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QPixmapCache>
#include <QProgressDialog>
#include <QPushButton>

constexpr auto LastFilePath = "Interface/LastFilePath";

namespace Fooyin {
struct GuiApplication::Private
{
    GuiApplication* self;
    CorePluginContext core;

    SettingsManager* settings;
    ActionManager* actionManager;

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
    StatusWidget* statusWidget{nullptr};

    GeneralPage generalPage;
    GuiGeneralPage guiGeneralPage;
    ArtworkPage artworkPage;
    LibraryGeneralPage libraryGeneralPage;
    LibrarySortingPage librarySortingPage;
    ShortcutsPage shortcutsPage;
    PlaylistGeneralPage playlistGeneralPage;
    PlaylistPresetsPage playlistPresetsPage;
    PlaylistColumnPage playlistColumnPage;
    PlaybackPage playbackPage;
    EnginePage enginePage;
    DirBrowserPage dirBrowserPage;
    LibraryTreePage libraryTreePage;
    LibraryTreeGroupPage libraryTreeGroupPage;
    PlaybackQueuePage playbackQueuePage;
    StatusWidgetPage statusWidgetPage;
    PluginPage pluginPage;

    GuiPluginContext guiPluginContext;

    ScriptParser scriptParser;
    CoverProvider coverProvider;

    explicit Private(GuiApplication* self_, CorePluginContext core_)
        : self{self_}
        , core{std::move(core_)}
        , settings{core.settingsManager}
        , actionManager{new ActionManager(settings, self)}
        , guiSettings{settings}
        , editableLayout{std::make_unique<EditableLayout>(actionManager, &widgetProvider, &layoutProvider, settings)}
        , menubar{std::make_unique<MainMenuBar>(actionManager)}
        , mainWindow{std::make_unique<MainWindow>(actionManager, menubar.get(), settings)}
        , mainContext{new WidgetContext(mainWindow.get(), Context{"Fooyin.MainWindow"}, self)}
        , playlistController{std::make_unique<PlaylistController>(core.playlistHandler, core.playerController,
                                                                  &selectionController, settings)}
        , playlistInteractor{core.playlistHandler, playlistController.get(), core.library}
        , selectionController{actionManager, settings, playlistController.get()}
        , searchController{new SearchController(editableLayout.get(), self)}
        , fileMenu{new FileMenu(actionManager, settings, self)}
        , editMenu{new EditMenu(actionManager, settings, self)}
        , viewMenu{new ViewMenu(actionManager, settings, self)}
        , playbackMenu{new PlaybackMenu(actionManager, core.playerController, settings, self)}
        , libraryMenu{new LibraryMenu(actionManager, core.library, settings, self)}
        , helpMenu{new HelpMenu(actionManager, self)}
        , propertiesDialog{new PropertiesDialog(settings, self)}
        , windowController{new WindowController(mainWindow.get())}
        , generalPage{settings}
        , guiGeneralPage{&layoutProvider, editableLayout.get(), settings}
        , artworkPage{settings}
        , libraryGeneralPage{actionManager, core.libraryManager, core.library, settings}
        , librarySortingPage{actionManager, settings}
        , shortcutsPage{actionManager, settings}
        , playlistGeneralPage{settings}
        , playlistPresetsPage{settings}
        , playlistColumnPage{actionManager, settings}
        , playbackPage{settings}
        , enginePage{settings, core.engine}
        , dirBrowserPage{settings}
        , libraryTreePage{settings}
        , libraryTreeGroupPage{actionManager, settings}
        , playbackQueuePage{settings}
        , statusWidgetPage{settings}
        , pluginPage{settings, core.pluginManager}
        , guiPluginContext{actionManager,    &layoutProvider, &selectionController, searchController,
                           propertiesDialog, &widgetProvider, editableLayout.get(), windowController}
        , coverProvider{core.tagLoader, settings}
    {
        setupConnections();
        registerActions();
        setupScanMenu();
        setupRatingMenu();
        restoreIconTheme();
        registerLayouts();
        registerWidgets();
        createPropertiesTabs();

        actionManager->addContextObject(mainContext);

        initialisePlugins();
        layoutProvider.findLayouts();
        setupLayoutMenu();
        editableLayout->initialise();
        mainWindow->setCentralWidget(editableLayout.get());

        auto openMainWindow = [this]() {
            mainWindow->open();
            if(settings->value<Settings::Core::FirstRun>()) {
                QMetaObject::invokeMethod(editableLayout.get(), &EditableLayout::showQuickSetup, Qt::QueuedConnection);
            }
        };

        if(core.libraryManager->hasLibrary() && core.library->isEmpty()
           && settings->value<Settings::Gui::WaitForTracks>()) {
            connect(core.library, &MusicLibrary::tracksLoaded, openMainWindow);
        }
        else {
            openMainWindow();
        }

        initialiseTray();
    }

    void setupLayoutMenu()
    {
        auto* viewActionMenu = actionManager->actionContainer(Constants::Menus::View);

        auto* layoutMenu = actionManager->createMenu(Constants::Menus::ViewLayout);
        layoutMenu->menu()->setTitle(tr("&Layout"));
        viewActionMenu->addMenu(layoutMenu, Actions::Groups::One);

        const auto layouts = layoutProvider.layouts();
        for(const auto& layout : layouts) {
            auto* layoutAction = new QAction(layout.name(), mainWindow.get());
            QObject::connect(layoutAction, &QAction::triggered, mainWindow.get(),
                             [this, layout]() { editableLayout->changeLayout(layout); });
            layoutMenu->addAction(layoutAction);
        }
    }

    void initialisePlugins()
    {
        if(core.pluginManager->allPluginInfo().empty()) {
            QMetaObject::invokeMethod(self, []() { showPluginsNotFoundMessage(); }, Qt::QueuedConnection);
            return;
        }

        core.pluginManager->initialisePlugins<GuiPlugin>(
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

        if(settings->value<Settings::Gui::Internal::ShowTrayIcon>()) {
            trayIcon->show();
        }

        settings->subscribe<Settings::Gui::Internal::ShowTrayIcon>(self,
                                                                   [this](bool show) { trayIcon->setVisible(show); });
    }

    void updateWindowTitle()
    {
        if(core.playerController->playState() == PlayState::Stopped) {
            mainWindow->resetTitle();
            return;
        }

        const Track currentTrack = core.playerController->currentTrack();
        if(!currentTrack.isValid()) {
            mainWindow->resetTitle();
            return;
        }

        const QString script = settings->value<Settings::Gui::Internal::WindowTitleTrackScript>();
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
        QObject::connect(core.library, &MusicLibrary::tracksUpdated, self,
                         [](const TrackList& tracks) { removeExpiredCovers(tracks); });
        QObject::connect(core.library, &MusicLibrary::tracksUpdated, &selectionController,
                         &TrackSelectionController::tracksUpdated);
        QObject::connect(core.library, &MusicLibrary::tracksDeleted, &selectionController,
                         &TrackSelectionController::tracksRemoved);

        QObject::connect(core.playerController, &PlayerController::playStateChanged, mainWindow.get(),
                         [this]() { updateWindowTitle(); });
        settings->subscribe<Settings::Gui::Internal::WindowTitleTrackScript>(self, [this]() { updateWindowTitle(); });
        QObject::connect(core.playerController, &PlayerController::currentTrackChanged, self,
                         [this]() { updateWindowTitle(); });
        QObject::connect(&selectionController, &TrackSelectionController::actionExecuted, playlistController.get(),
                         &PlaylistController::handleTrackSelectionAction);
        QObject::connect(&selectionController, &TrackSelectionController::requestPropertiesDialog, propertiesDialog,
                         &PropertiesDialog::show);
        QObject::connect(fileMenu, &FileMenu::requestNewPlaylist, self, [this]() {
            if(auto* playlist = core.playlistHandler->createEmptyPlaylist()) {
                playlistController->changeCurrentPlaylist(playlist);
            }
        });
        QObject::connect(fileMenu, &FileMenu::requestAddFiles, self, [this]() { addFiles(); });
        QObject::connect(fileMenu, &FileMenu::requestAddFolders, self, [this]() { addFolders(); });
        QObject::connect(fileMenu, &FileMenu::requestLoadPlaylist, self, [this]() { loadPlaylist(); });
        QObject::connect(fileMenu, &FileMenu::requestSavePlaylist, self, [this]() { savePlaylist(); });
        QObject::connect(viewMenu, &ViewMenu::openQuickSetup, editableLayout.get(), &EditableLayout::showQuickSetup);
        QObject::connect(viewMenu, &ViewMenu::openScriptSandbox, self, [this]() {
            auto* sandboxDialog = new SandboxDialog(&selectionController, settings, mainWindow.get());
            sandboxDialog->setAttribute(Qt::WA_DeleteOnClose);
            sandboxDialog->show();
        });
        QObject::connect(viewMenu, &ViewMenu::showNowPlaying, self, [this]() {
            if(auto* activePlaylist = core.playlistHandler->activePlaylist()) {
                playlistController->showNowPlaying();
                playlistController->changeCurrentPlaylist(activePlaylist);
            }
        });
        QObject::connect(core.engine, &EngineController::engineError, self,
                         [this](const QString& error) { showEngineError(error); });
        QObject::connect(core.engine, &EngineController::trackStatusChanged, self, [this](TrackStatus status) {
            if(status == TrackStatus::InvalidTrack) {
                const Track track = core.playerController->currentTrack();
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
            const double volume = settings->value<Settings::Core::OutputVolume>();
            if(volume > 0.0) {
                settings->set<Settings::Core::Internal::MuteVolume>(volume);
                settings->set<Settings::Core::OutputVolume>(0.0);
            }
            else {
                settings->set<Settings::Core::OutputVolume>(settings->value<Settings::Core::Internal::MuteVolume>());
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

    void rescanTracks(const TrackList& tracks) const
    {
        auto* scanDialog = new QProgressDialog(QStringLiteral("Reading tracks..."), QStringLiteral("Abort"), 0, 100,
                                               Utils::getMainWindow());
        scanDialog->setAttribute(Qt::WA_DeleteOnClose);
        scanDialog->setModal(true);
        scanDialog->setValue(0);

        const ScanRequest request = core.library->scanTracks(tracks);

        QObject::connect(core.library, &MusicLibrary::scanProgress, scanDialog,
                         [scanDialog, request](const ScanProgress& progress) {
                             if(progress.id != request.id) {
                                 return;
                             }

                             if(scanDialog->wasCanceled()) {
                                 request.cancel();
                                 scanDialog->close();
                             }

                             scanDialog->setValue(progress.percentage());
                         });
    }

    void setupScanMenu()
    {
        auto* selectionMenu = actionManager->actionContainer(Constants::Menus::Context::TrackSelection);
        auto* taggingMenu   = actionManager->createMenu(Constants::Menus::Context::Tagging);
        taggingMenu->menu()->setTitle(tr("Tagging"));
        selectionMenu->addMenu(taggingMenu);

        auto* rescanAction = new QAction(tr("Rescan"), mainWindow.get());

        auto rescan = [this]() {
            if(selectionController.hasTracks()) {
                const auto tracks = selectionController.selectedTracks();
                rescanTracks(tracks);
            }
        };

        QObject::connect(rescanAction, &QAction::triggered, mainWindow.get(), [rescan]() { rescan(); });
        taggingMenu->menu()->addAction(rescanAction);

        QObject::connect(&selectionController, &TrackSelectionController::selectionChanged, mainWindow.get(),
                         [this, rescanAction]() { rescanAction->setEnabled(selectionController.hasTracks()); });
    }

    void setupRatingMenu()
    {
        auto* selectionMenu = actionManager->actionContainer(Constants::Menus::Context::TrackSelection);
        auto* taggingMenu   = actionManager->createMenu(Constants::Menus::Context::Tagging);
        taggingMenu->menu()->setTitle(tr("Tagging"));
        selectionMenu->addMenu(taggingMenu);

        auto* rate0 = new QAction(tr("Rate 0"), mainWindow.get());
        auto* rate1 = new QAction(tr("Rate 1"), mainWindow.get());
        auto* rate2 = new QAction(tr("Rate 2"), mainWindow.get());
        auto* rate3 = new QAction(tr("Rate 3"), mainWindow.get());
        auto* rate4 = new QAction(tr("Rate 4"), mainWindow.get());
        auto* rate5 = new QAction(tr("Rate 5"), mainWindow.get());

        actionManager->registerAction(rate0, Constants::Actions::Rate0);
        actionManager->registerAction(rate1, Constants::Actions::Rate1);
        actionManager->registerAction(rate2, Constants::Actions::Rate2);
        actionManager->registerAction(rate3, Constants::Actions::Rate3);
        actionManager->registerAction(rate4, Constants::Actions::Rate4);
        actionManager->registerAction(rate5, Constants::Actions::Rate5);

        auto setRating = [this](const int rating) {
            if(selectionController.selectedTrackCount() == 1) {
                const auto tracks = selectionController.selectedTracks();
                Track track       = tracks.front();
                if(track.ratingStars() != rating) {
                    track.setRatingStars(rating);
                    if(settings->value<Settings::Core::SaveRatingToMetadata>()) {
                        core.library->updateTrackMetadata({track});
                    }
                    else {
                        core.library->updateTrackStats(track);
                    }
                }
            }
        };

        QObject::connect(rate0, &QAction::triggered, mainWindow.get(), [setRating]() { setRating(0); });
        QObject::connect(rate1, &QAction::triggered, mainWindow.get(), [setRating]() { setRating(1); });
        QObject::connect(rate2, &QAction::triggered, mainWindow.get(), [setRating]() { setRating(2); });
        QObject::connect(rate3, &QAction::triggered, mainWindow.get(), [setRating]() { setRating(3); });
        QObject::connect(rate4, &QAction::triggered, mainWindow.get(), [setRating]() { setRating(4); });
        QObject::connect(rate5, &QAction::triggered, mainWindow.get(), [setRating]() { setRating(5); });

        auto* ratingMenu = new QMenu(tr("Rating"), taggingMenu->menu());
        ratingMenu->addAction(rate0);
        ratingMenu->addAction(rate1);
        ratingMenu->addAction(rate2);
        ratingMenu->addAction(rate3);
        ratingMenu->addAction(rate4);
        ratingMenu->addAction(rate5);
        taggingMenu->menu()->addMenu(ratingMenu);

        QObject::connect(
            &selectionController, &TrackSelectionController::selectionChanged, mainWindow.get(),
            [this, ratingMenu]() { ratingMenu->setEnabled(selectionController.selectedTrackCount() == 1); });
    }

    void restoreIconTheme() const
    {
        using namespace Settings::Gui::Internal;

        const auto iconTheme = static_cast<IconThemeOption>(settings->value<Settings::Gui::IconTheme>());
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

        QIcon::setFallbackThemeName(settings->value<SystemIconTheme>());
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
            QStringLiteral("Dummy"), [this]() { return new Dummy(settings, mainWindow.get()); }, tr("Dummy"));
        widgetProvider.setIsHidden(QStringLiteral("Dummy"), true);

        widgetProvider.registerWidget(
            QStringLiteral("SplitterVertical"),
            [this]() { return new VerticalSplitterWidget(&widgetProvider, settings, mainWindow.get()); },
            tr("Splitter (Top/Bottom)"));
        widgetProvider.setSubMenus(QStringLiteral("SplitterVertical"), {tr("Splitters")});

        widgetProvider.registerWidget(
            QStringLiteral("SplitterHorizontal"),
            [this]() { return new HorizontalSplitterWidget(&widgetProvider, settings, mainWindow.get()); },
            tr("Splitter (Left/Right)"));
        widgetProvider.setSubMenus(QStringLiteral("SplitterHorizontal"), {tr("Splitters")});

        widgetProvider.registerWidget(
            QStringLiteral("PlaylistSwitcher"),
            [this]() { return new PlaylistBox(playlistController.get(), mainWindow.get()); }, tr("Playlist Switcher"));

        widgetProvider.registerWidget(
            QStringLiteral("PlaylistTabs"),
            [this]() {
                auto* playlistTabs = new PlaylistTabs(actionManager, &widgetProvider, playlistController.get(),
                                                      settings, mainWindow.get());
                QObject::connect(playlistTabs, &PlaylistTabs::filesDropped, &playlistInteractor,
                                 &PlaylistInteractor::filesToPlaylist);
                QObject::connect(playlistTabs, &PlaylistTabs::tracksDropped, &playlistInteractor,
                                 &PlaylistInteractor::trackMimeToPlaylist);
                return playlistTabs;
            },
            tr("Playlist Tabs"));
        widgetProvider.setSubMenus(QStringLiteral("PlaylistTabs"), {tr("Splitters")});

        widgetProvider.registerWidget(
            QStringLiteral("PlaylistOrganiser"),
            [this]() { return new PlaylistOrganiser(actionManager, &playlistInteractor, settings, mainWindow.get()); },
            tr("Playlist Organiser"));

        widgetProvider.registerWidget(
            QStringLiteral("PlaybackQueue"),
            [this]() {
                return new QueueViewer(actionManager, &playlistInteractor, core.tagLoader, core.settingsManager,
                                       mainWindow.get());
            },
            tr("Playback Queue"));
        widgetProvider.setLimit(QStringLiteral("PlaybackQueue"), 1);

        widgetProvider.registerWidget(
            QStringLiteral("TabStack"),
            [this]() { return new TabStackWidget(&widgetProvider, settings, mainWindow.get()); }, tr("Tab Stack"));
        widgetProvider.setSubMenus(QStringLiteral("TabStack"), {tr("Splitters")});

        widgetProvider.registerWidget(
            QStringLiteral("LibraryTree"),
            [this]() {
                return new LibraryTreeWidget(actionManager, core.library, playlistController.get(), settings,
                                             mainWindow.get());
            },
            tr("Library Tree"));

        widgetProvider.registerWidget(
            QStringLiteral("PlayerControls"),
            [this]() { return new PlayerControl(actionManager, core.playerController, settings, mainWindow.get()); },
            tr("Player Controls"));
        widgetProvider.setSubMenus(QStringLiteral("PlayerControls"), {tr("Controls")});

        widgetProvider.registerWidget(
            QStringLiteral("PlaylistControls"),
            [this]() { return new PlaylistControl(core.playerController, settings, mainWindow.get()); },
            tr("Playlist Controls"));
        widgetProvider.setSubMenus(QStringLiteral("PlaylistControls"), {tr("Controls")});

        widgetProvider.registerWidget(
            QStringLiteral("VolumeControls"),
            [this]() { return new VolumeControl(actionManager, settings, mainWindow.get()); }, tr("Volume Controls"));
        widgetProvider.setSubMenus(QStringLiteral("VolumeControls"), {tr("Controls")});

        widgetProvider.registerWidget(
            QStringLiteral("SeekBar"), [this]() { return new SeekBar(core.playerController, mainWindow.get()); },
            tr("Seekbar"));
        widgetProvider.setSubMenus(QStringLiteral("SeekBar"), {tr("Controls")});

        widgetProvider.registerWidget(
            QStringLiteral("SelectionInfo"),
            [this]() {
                return new InfoWidget(core.playerController, &selectionController, settings, mainWindow.get());
            },
            tr("Selection Info"));

        widgetProvider.registerWidget(
            QStringLiteral("ArtworkPanel"),
            [this]() {
                return new CoverWidget(core.playerController, &selectionController, core.tagLoader, settings,
                                       mainWindow.get());
            },
            tr("Artwork Panel"));

        widgetProvider.registerWidget(
            QStringLiteral("Lyrics"), [this]() { return new LyricsWidget(core.playerController, mainWindow.get()); },
            tr("Lyrics"));
        widgetProvider.setLimit(QStringLiteral("Lyrics"), 1);

        widgetProvider.registerWidget(
            QStringLiteral("Playlist"),
            [this]() {
                return new PlaylistWidget(actionManager, &playlistInteractor, &coverProvider, settings,
                                          mainWindow.get());
            },
            tr("Playlist"));
        widgetProvider.setLimit(QStringLiteral("Playlist"), 1);

        widgetProvider.registerWidget(
            QStringLiteral("Spacer"), [this]() { return new Spacer(mainWindow.get()); }, tr("Spacer"));

        widgetProvider.registerWidget(
            QStringLiteral("StatusBar"),
            [this]() {
                statusWidget
                    = new StatusWidget(core.playerController, &selectionController, settings, mainWindow.get());
                QObject::connect(statusWidget, &QObject::destroyed, statusWidget, [this]() { statusWidget = nullptr; });
                QObject::connect(core.library, &MusicLibrary::scanProgress, statusWidget,
                                 [this](const ScanProgress& progress) { showScanProgress(progress); });
                return statusWidget;
            },
            tr("Status Bar"));
        widgetProvider.setLimit(QStringLiteral("StatusBar"), 1);

        widgetProvider.registerWidget(
            QStringLiteral("SearchBar"),
            [this]() { return new SearchWidget(searchController, settings, mainWindow.get()); }, tr("Search Bar"));

        widgetProvider.registerWidget(
            QStringLiteral("DirectoryBrowser"),
            [this]() {
                auto* browser = new DirBrowser(core.tagLoader->supportedFileExtensions(), &playlistInteractor, settings,
                                               mainWindow.get());
                QObject::connect(core.playerController, &PlayerController::playStateChanged, browser,
                                 &DirBrowser::playstateChanged);
                QObject::connect(core.playerController, &PlayerController::playlistTrackChanged, browser,
                                 &DirBrowser::playlistTrackChanged);
                QObject::connect(core.playlistHandler, &PlaylistHandler::activePlaylistChanged, browser,
                                 &DirBrowser::activePlaylistChanged);
                return browser;
            },
            tr("Directory Browser"));
        widgetProvider.setLimit(QStringLiteral("DirectoryBrowser"), 1);
    }

    void createPropertiesTabs()
    {
        propertiesDialog->addTab(
            tr("Details"), [this]() { return new InfoWidget(core.playerController, &selectionController, settings); });
    }

    void showEngineError(const QString& error) const
    {
        if(error.isEmpty()) {
            return;
        }

        QMessageBox message;
        message.setIcon(QMessageBox::Warning);
        message.setText(tr("Playback Error"));
        message.setInformativeText(error);

        message.addButton(QMessageBox::Ok);

        message.exec();
    }

    void showScanProgress(const ScanProgress& progress) const
    {
        if(!statusWidget || progress.id < 0 || progress.total == 0) {
            return;
        }

        QString scanType;
        switch(progress.type) {
            case(ScanRequest::Files):
                scanType = QStringLiteral("files");
                break;
            case(ScanRequest::Tracks):
                scanType = QStringLiteral("tracks");
                break;
            case(ScanRequest::Library):
                scanType = QStringLiteral("library");
                break;
        }

        const auto scanText = QStringLiteral("Scanning %1: %2%").arg(scanType).arg(progress.percentage());
        statusWidget->showMessage(scanText);
    }

    void showTrackNotFoundMessage(const Track& track) const
    {
        if(settings->value<Settings::Core::SkipUnavailable>()) {
            core.playerController->next();
            core.playerController->play();
            return;
        }

        QMessageBox message;
        message.setIcon(QMessageBox::Warning);
        message.setText(tr("Track Not Found"));
        message.setInformativeText(track.filepath());

        message.addButton(QMessageBox::Ok);
        if(auto* button = message.button(QMessageBox::Ok)) {
            button->setText(tr("Continue"));
        }
        QPushButton* stopButton = message.addButton(tr("Stop"), QMessageBox::ActionRole);
        stopButton->setIcon(Utils::iconFromTheme(Constants::Icons::Stop));
        message.setDefaultButton(QMessageBox::Ok);

        auto* alwaysSkip = new QCheckBox(tr("Always continue playing if a track is unavailable"), &message);
        message.setCheckBox(alwaysSkip);

        message.exec();

        if(alwaysSkip->isChecked()) {
            settings->set<Settings::Core::SkipUnavailable>(true);
        }

        if(message.clickedButton() == stopButton) {
            core.playerController->stop();
        }
        else {
            core.playerController->next();
            core.playerController->play();
        }
    }

    void addFiles() const
    {
        const QString audioExtensions
            = Utils::extensionsToWildcards(core.tagLoader->supportedFileExtensions()).join(u" ");
        const QString playlistExtensions = Playlist::supportedPlaylistExtensions().join(u" ");
        const QString allExtensions      = QStringLiteral("%1 %2").arg(audioExtensions, playlistExtensions);

        const QString allFilter      = tr("All Supported Media Files (%1)").arg(allExtensions);
        const QString filesFilter    = tr("Audio Files (%1)").arg(audioExtensions);
        const QString playlistFilter = tr("Playlists (%1)").arg(playlistExtensions);

        const QStringList filters{allFilter, filesFilter, playlistFilter};

        QUrl dir = QUrl::fromLocalFile(QDir::homePath());
        if(const auto lastPath = settings->fileValue(QString::fromLatin1(LastFilePath)).toString();
           !lastPath.isEmpty()) {
            dir = lastPath;
        }

        const auto files = QFileDialog::getOpenFileUrls(mainWindow.get(), tr("Add Files"), dir, filters.join(u";;"));

        if(files.empty()) {
            return;
        }

        settings->fileSet(QString::fromLatin1(LastFilePath), files.front());

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
        const QString playlistName = settings->value<Settings::Core::OpenFilesPlaylist>();
        if(settings->value<Settings::Core::OpenFilesSendTo>()) {
            playlistInteractor.filesToNewPlaylistReplace(playlistName, urls, true);
        }
        else {
            playlistInteractor.filesToNewPlaylist(playlistName, urls, true);
        }
    }

    void loadPlaylist() const
    {
        const QString playlistExtensions = Playlist::supportedPlaylistExtensions().join(QStringLiteral(" "));
        const QString playlistFilter     = tr("Playlists (%1)").arg(playlistExtensions);

        QUrl dir = QUrl::fromLocalFile(QDir::homePath());
        if(const auto lastPath = settings->fileValue(QString::fromLatin1(LastFilePath)).toString();
           !lastPath.isEmpty()) {
            dir = lastPath;
        }

        const auto files = QFileDialog::getOpenFileUrls(mainWindow.get(), tr("Load Playlist"), dir, playlistFilter);

        if(files.empty()) {
            return;
        }

        settings->fileSet(QString::fromLatin1(LastFilePath), files.front());

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
            = Utils::extensionsToFilterList(core.playlistLoader->supportedSaveExtensions(), QStringLiteral("files"));

        QUrl dir = QUrl::fromLocalFile(QDir::homePath());
        if(const auto lastPath = settings->fileValue(QString::fromLatin1(LastFilePath)).toString();
           !lastPath.isEmpty()) {
            dir = lastPath;
        }

        QString selectedFilter;
        const auto file
            = QFileDialog::getSaveFileUrl(mainWindow.get(), tr("Save Playlist"), dir, playlistFilter, &selectedFilter);

        if(file.isEmpty()) {
            return;
        }

        settings->fileSet(QString::fromLatin1(LastFilePath), file);

        const QString extension = Utils::extensionFromFilter(selectedFilter);
        if(extension.isEmpty()) {
            return;
        }

        if(auto* parser = core.playlistLoader->parserForExtension(extension)) {
            QFile playlistFile{file.toLocalFile()};
            if(!playlistFile.open(QIODevice::WriteOnly)) {
                qWarning() << QStringLiteral("Could not open playlist file %1 for writing: %2")
                                  .arg(playlistFile.fileName(), playlistFile.errorString());
                return;
            }

            const QFileInfo info{playlistFile};
            const QDir playlistDir{info.absolutePath()};
            const auto pathType
                = static_cast<PlaylistParser::PathType>(settings->value<Settings::Core::PlaylistSavePathType>());
            const bool writeMetadata = settings->value<Settings::Core::PlaylistSaveMetadata>();

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

    updateCache(p->settings->value<Settings::Gui::Internal::PixmapCacheSize>());
    p->settings->subscribe<Settings::Gui::Internal::PixmapCacheSize>(this, updateCache);

    QObject::connect(p->settings->settingsDialog(), &SettingsDialogController::opening, this, [this]() {
        const bool isLayoutEditing = p->settings->value<Settings::Gui::LayoutEditing>();
        // Layout editing mode overrides the global action context, so disable it until the dialog closes
        p->settings->set<Settings::Gui::LayoutEditing>(false);
        QObject::connect(
            p->settings->settingsDialog(), &SettingsDialogController::closing, this,
            [this, isLayoutEditing]() { p->settings->set<Settings::Gui::LayoutEditing>(isLayoutEditing); },
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
