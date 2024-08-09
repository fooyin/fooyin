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
#include "internalguisettings.h"
#include "mainwindow.h"
#include "menubar/editmenu.h"
#include "menubar/filemenu.h"
#include "menubar/helpmenu.h"
#include "menubar/layoutmenu.h"
#include "menubar/librarymenu.h"
#include "menubar/mainmenubar.h"
#include "menubar/playbackmenu.h"
#include "menubar/viewmenu.h"
#include "playlist/playlistcontroller.h"
#include "playlist/playlistinteractor.h"
#include "sandbox/sandboxdialog.h"
#include "search/searchcontroller.h"
#include "systemtrayicon.h"
#include "widgets.h"

#include <core/application.h>
#include <core/corepaths.h>
#include <core/coresettings.h>
#include <core/engine/enginehandler.h>
#include <core/internalcoresettings.h>
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
#include <utils/logging/logwidget.h>
#include <utils/settings/settingsdialogcontroller.h>
#include <utils/settings/settingsmanager.h>
#include <utils/utils.h>

#include <QAction>
#include <QCheckBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QMessageBox>
#include <QPixmapCache>
#include <QProgressDialog>
#include <QPushButton>

Q_LOGGING_CATEGORY(GUI_APP, "GUI")

constexpr auto LastFilePath = "Interface/LastFilePath";

namespace Fooyin {
class GuiApplicationPrivate
{
public:
    explicit GuiApplicationPrivate(GuiApplication* self_, CorePluginContext core_);

    void setupConnections();
    void initialisePlugins();

    void showPluginsNotFoundMessage();
    void initialiseTray();
    void updateWindowTitle();
    static void removeExpiredCovers(const TrackList& tracks);

    void registerActions();
    void rescanTracks(const TrackList& tracks) const;

    void setupScanMenu();
    void setupRatingMenu();
    void setupUtilitesMenu() const;

    void mute() const;
    void restoreIconTheme() const;
    void registerLayouts();

    void showPropertiesDialog() const;
    void showEngineError(const QString& error) const;
    void showMessage(const QString& title, const Track& track) const;
    void showTrackNotFoundMessage(const Track& track) const;
    void showTrackUnreableMessage(const Track& track) const;

    void addFiles() const;
    void addFolders() const;
    void openFiles(const QList<QUrl>& urls) const;

    void loadPlaylist() const;
    void savePlaylist() const;

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
    LayoutMenu* layoutMenu;
    PlaybackMenu* playbackMenu;
    LibraryMenu* libraryMenu;
    HelpMenu* helpMenu;

    PropertiesDialog* propertiesDialog;
    WindowController* windowController;

    GuiPluginContext guiPluginContext;

    std::unique_ptr<LogWidget> logWidget;
    Widgets* widgets;
    ScriptParser scriptParser;
};

GuiApplicationPrivate::GuiApplicationPrivate(GuiApplication* self_, CorePluginContext core_)
    : self{self_}
    , core{std::move(core_)}
    , settings{core.settingsManager}
    , actionManager{new ActionManager(settings, self)}
    , guiSettings{settings}
    , editableLayout{std::make_unique<EditableLayout>(actionManager, &widgetProvider, &layoutProvider, settings)}
    , menubar{std::make_unique<MainMenuBar>(actionManager, settings)}
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
    , layoutMenu{new LayoutMenu(actionManager, &layoutProvider, settings, self)}
    , playbackMenu{new PlaybackMenu(actionManager, core.playerController, settings, self)}
    , libraryMenu{new LibraryMenu(actionManager, core.library, settings, self)}
    , helpMenu{new HelpMenu(actionManager, self)}
    , propertiesDialog{new PropertiesDialog(settings, self)}
    , windowController{new WindowController(mainWindow.get())}
    , guiPluginContext{actionManager,    &layoutProvider, &selectionController, searchController,
                       propertiesDialog, &widgetProvider, editableLayout.get(), windowController}
    , logWidget{std::make_unique<LogWidget>()}
    , widgets{new Widgets(core, guiPluginContext, &playlistInteractor, self)}
{
    setupConnections();
    registerActions();
    setupScanMenu();
    setupRatingMenu();
    setupUtilitesMenu();
    restoreIconTheme();
    registerLayouts();

    widgets->registerWidgets();
    widgets->registerPages();
    widgets->registerPropertiesTabs();

    actionManager->addContextObject(mainContext);

    initialisePlugins();
    layoutProvider.findLayouts();
    layoutMenu->setup();
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
        QObject::connect(core.library, &MusicLibrary::tracksLoaded, openMainWindow);
    }
    else {
        openMainWindow();
    }

    initialiseTray();
}

void GuiApplicationPrivate::setupConnections()
{
    QObject::connect(core.library, &MusicLibrary::tracksMetadataChanged, self,
                     [](const TrackList& tracks) { removeExpiredCovers(tracks); });
    QObject::connect(core.library, &MusicLibrary::tracksMetadataChanged, &selectionController,
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
    QObject::connect(&selectionController, &TrackSelectionController::requestPropertiesDialog, self,
                     [this]() { showPropertiesDialog(); });
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
    QObject::connect(viewMenu, &ViewMenu::openLog, logWidget.get(), &LogWidget::show);
    QObject::connect(viewMenu, &ViewMenu::openScriptSandbox, self, [this]() {
        auto* sandboxDialog = new SandboxDialog(core.libraryManager, &selectionController, settings, mainWindow.get());
        sandboxDialog->setAttribute(Qt::WA_DeleteOnClose);
        sandboxDialog->show();
    });
    QObject::connect(viewMenu, &ViewMenu::showNowPlaying, self, [this]() {
        if(auto* activePlaylist = core.playlistHandler->activePlaylist()) {
            playlistController->showNowPlaying();
            playlistController->changeCurrentPlaylist(activePlaylist);
        }
    });
    QObject::connect(layoutMenu, &LayoutMenu::changeLayout, editableLayout.get(), &EditableLayout::changeLayout);
    QObject::connect(layoutMenu, &LayoutMenu::importLayout, self,
                     [this]() { layoutProvider.importLayout(mainWindow.get()); });
    QObject::connect(layoutMenu, &LayoutMenu::exportLayout, self,
                     [this]() { editableLayout->exportLayout(editableLayout.get()); });

    QObject::connect(&layoutProvider, &LayoutProvider::layoutAdded, layoutMenu, &LayoutMenu::setup);
    QObject::connect(&layoutProvider, &LayoutProvider::requestChangeLayout, editableLayout.get(),
                     &EditableLayout::changeLayout);

    QObject::connect(core.engine, &EngineController::engineError, self,
                     [this](const QString& error) { showEngineError(error); });
    QObject::connect(core.engine, &EngineController::trackStatusChanged, self, [this](TrackStatus status) {
        const Track track = core.playerController->currentTrack();
        if(status == TrackStatus::Invalid) {
            if(track.isValid()) {
                if(track.isInArchive()) {
                    if(!QFileInfo::exists(track.archivePath())) {
                        showTrackNotFoundMessage(track);
                    }
                }
                else if(!QFileInfo::exists(track.filepath())) {
                    showTrackNotFoundMessage(track);
                }
            }
        }
        else if(status == TrackStatus::Unreadable) {
            showTrackUnreableMessage(track);
        }
    });

    settings->subscribe<Settings::Gui::LayoutEditing>(self, [this]() { updateWindowTitle(); });
}

void GuiApplicationPrivate::initialisePlugins()
{
    if(core.pluginManager->allPluginInfo().empty()) {
        QMetaObject::invokeMethod(self, [this]() { showPluginsNotFoundMessage(); }, Qt::QueuedConnection);
        return;
    }

    core.pluginManager->initialisePlugins<GuiPlugin>(
        [this](GuiPlugin* plugin) { plugin->initialise(guiPluginContext); });
}

void GuiApplicationPrivate::showPluginsNotFoundMessage()
{
    QMessageBox message;
    message.setIcon(QMessageBox::Warning);
    message.setText(GuiApplication::tr("Plugins not found"));
    message.setInformativeText(GuiApplication::tr("Some plugins are required for full functionality."));
    message.setDetailedText(GuiApplication::tr("Plugin search locations:\n\n")
                            + Core::pluginPaths().join(QStringLiteral("\n")));

    message.addButton(QMessageBox::Ok);
    QPushButton* quitButton = message.addButton(GuiApplication::tr("Quit"), QMessageBox::ActionRole);
    quitButton->setIcon(Utils::iconFromTheme(Constants::Icons::Quit));
    message.setDefaultButton(QMessageBox::Ok);

    message.exec();

    if(message.clickedButton() == quitButton) {
        Application::quit();
    }
}

void GuiApplicationPrivate::initialiseTray()
{
    trayIcon = std::make_unique<SystemTrayIcon>(actionManager);

    QObject::connect(trayIcon.get(), &SystemTrayIcon::toggleVisibility, mainWindow.get(),
                     &MainWindow::toggleVisibility);

    if(settings->value<Settings::Gui::Internal::ShowTrayIcon>()) {
        trayIcon->show();
    }

    settings->subscribe<Settings::Gui::Internal::ShowTrayIcon>(self, [this](bool show) { trayIcon->setVisible(show); });
}

void GuiApplicationPrivate::updateWindowTitle()
{
    if(core.playerController->playState() == Player::PlayState::Stopped) {
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

void GuiApplicationPrivate::removeExpiredCovers(const TrackList& tracks)
{
    for(const Track& track : tracks) {
        if(track.metadataWasModified()) {
            CoverProvider::removeFromCache(track);
        }
    }
}

void GuiApplicationPrivate::registerActions()
{
    auto* muteAction
        = new QAction(Utils::iconFromTheme(Constants::Icons::VolumeMute), GuiApplication::tr("Mute"), mainWindow.get());
    actionManager->registerAction(muteAction, Constants::Actions::Mute);
    QObject::connect(muteAction, &QAction::triggered, mainWindow.get(), [this]() { mute(); });

    auto setSavePlaylistState = [this]() {
        if(auto* savePlaylistCommand = actionManager->command(Constants::Actions::SavePlaylist)) {
            if(const auto* playlist = playlistController->currentPlaylist()) {
                savePlaylistCommand->action()->setEnabled(playlist->trackCount() > 0);
            }
        }
    };

    QObject::connect(playlistController.get(), &PlaylistController::currentPlaylistChanged, mainWindow.get(),
                     setSavePlaylistState);
    QObject::connect(playlistController->playlistHandler(), &PlaylistHandler::tracksChanged, mainWindow.get(),
                     setSavePlaylistState);
    QObject::connect(playlistController->playlistHandler(), &PlaylistHandler::tracksAdded, mainWindow.get(),
                     setSavePlaylistState);
    QObject::connect(playlistController->playlistHandler(), &PlaylistHandler::tracksRemoved, mainWindow.get(),
                     setSavePlaylistState);

    auto* removePlaylist = new QAction(GuiApplication::tr("Remove Playlist"), mainWindow.get());
    auto* removeCmd      = actionManager->registerAction(removePlaylist, Constants::Actions::RemovePlaylist);
    removeCmd->setDefaultShortcut(QKeySequence{Qt::CTRL | Qt::Key_W});
    QObject::connect(removePlaylist, &QAction::triggered, mainWindow.get(), [this]() {
        if(auto* currentPlaylist = playlistController->currentPlaylist()) {
            core.playlistHandler->removePlaylist(currentPlaylist->id());
        }
    });
}

void GuiApplicationPrivate::rescanTracks(const TrackList& tracks) const
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

void GuiApplicationPrivate::setupScanMenu()
{
    auto* selectionMenu = actionManager->actionContainer(Constants::Menus::Context::TrackSelection);
    auto* taggingMenu   = actionManager->createMenu(Constants::Menus::Context::Tagging);
    taggingMenu->menu()->setTitle(GuiApplication::tr("Tagging"));
    selectionMenu->addMenu(taggingMenu);

    auto* rescanAction = new QAction(GuiApplication::tr("Rescan"), mainWindow.get());

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

void GuiApplicationPrivate::setupRatingMenu()
{
    auto* selectionMenu = actionManager->actionContainer(Constants::Menus::Context::TrackSelection);
    auto* taggingMenu   = actionManager->createMenu(Constants::Menus::Context::Tagging);
    taggingMenu->menu()->setTitle(GuiApplication::tr("Tagging"));
    selectionMenu->addMenu(taggingMenu);

    auto* rate0 = new QAction(GuiApplication::tr("Rate 0"), mainWindow.get());
    auto* rate1 = new QAction(GuiApplication::tr("Rate 1"), mainWindow.get());
    auto* rate2 = new QAction(GuiApplication::tr("Rate 2"), mainWindow.get());
    auto* rate3 = new QAction(GuiApplication::tr("Rate 3"), mainWindow.get());
    auto* rate4 = new QAction(GuiApplication::tr("Rate 4"), mainWindow.get());
    auto* rate5 = new QAction(GuiApplication::tr("Rate 5"), mainWindow.get());

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
                    core.library->writeTrackMetadata({track});
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

    auto* ratingMenu = new QMenu(GuiApplication::tr("Rating"), taggingMenu->menu());
    ratingMenu->addAction(rate0);
    ratingMenu->addAction(rate1);
    ratingMenu->addAction(rate2);
    ratingMenu->addAction(rate3);
    ratingMenu->addAction(rate4);
    ratingMenu->addAction(rate5);
    taggingMenu->menu()->addMenu(ratingMenu);

    QObject::connect(&selectionController, &TrackSelectionController::selectionChanged, mainWindow.get(),
                     [this, ratingMenu]() { ratingMenu->setEnabled(selectionController.selectedTrackCount() == 1); });
}

void GuiApplicationPrivate::setupUtilitesMenu() const
{
    auto* selectionMenu = actionManager->actionContainer(::Fooyin::Constants::Menus::Context::TrackSelection);
    auto* utilitiesMenu = actionManager->createMenu(::Fooyin::Constants::Menus::Context::Utilities);
    utilitiesMenu->menu()->setTitle(GuiApplication::tr("Utilities"));
    selectionMenu->addMenu(utilitiesMenu);
}

void GuiApplicationPrivate::mute() const
{
    const double volume = settings->value<Settings::Core::OutputVolume>();
    if(volume > 0.0) {
        settings->set<Settings::Core::Internal::MuteVolume>(volume);
        settings->set<Settings::Core::OutputVolume>(0.0);
    }
    else {
        settings->set<Settings::Core::OutputVolume>(settings->value<Settings::Core::Internal::MuteVolume>());
    }
}

void GuiApplicationPrivate::restoreIconTheme() const
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

void GuiApplicationPrivate::registerLayouts()
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

void GuiApplicationPrivate::showPropertiesDialog() const
{
    const auto tracks = selectionController.selectedTracks();
    if(!tracks.empty()) {
        propertiesDialog->show(tracks);
    }
}

void GuiApplicationPrivate::showEngineError(const QString& error) const
{
    if(error.isEmpty()) {
        return;
    }

    QMessageBox message;
    message.setIcon(QMessageBox::Warning);
    message.setText(GuiApplication::tr("Playback Error"));
    message.setInformativeText(error);

    message.addButton(QMessageBox::Ok);

    message.exec();
}

void GuiApplicationPrivate::showMessage(const QString& title, const Track& track) const
{
    if(settings->fileValue(Settings::Core::Internal::PlaylistSkipUnavailable).toBool()) {
        core.playerController->next();
        if(core.playerController->playState() == Player::PlayState::Playing) {
            core.playerController->play();
        }
        return;
    }

    QMessageBox message;
    message.setIcon(QMessageBox::Warning);
    message.setText(title);
    message.setInformativeText(track.filepath());

    message.addButton(QMessageBox::Ok);
    if(auto* button = message.button(QMessageBox::Ok)) {
        button->setText(GuiApplication::tr("Continue"));
    }
    QPushButton* stopButton = message.addButton(GuiApplication::tr("Stop"), QMessageBox::ActionRole);
    stopButton->setIcon(Utils::iconFromTheme(Constants::Icons::Stop));
    message.setDefaultButton(QMessageBox::Ok);

    auto* alwaysSkip = new QCheckBox(GuiApplication::tr("Always continue playing if a track is unavailable"), &message);
    message.setCheckBox(alwaysSkip);

    message.exec();

    if(alwaysSkip->isChecked()) {
        settings->fileSet(Settings::Core::Internal::PlaylistSkipUnavailable, true);
    }

    if(message.clickedButton() == stopButton) {
        core.playerController->stop();
    }
    else {
        core.playerController->next();
        if(core.playerController->playState() == Player::PlayState::Playing) {
            core.playerController->play();
        }
    }
}

void GuiApplicationPrivate::showTrackNotFoundMessage(const Track& track) const
{
    showMessage(GuiApplication::tr("Track Not Found"), track);
}

void GuiApplicationPrivate::showTrackUnreableMessage(const Track& track) const
{
    showMessage(GuiApplication::tr("No Decoder Available"), track);
}

void GuiApplicationPrivate::addFiles() const
{
    const QString audioExtensions
        = Utils::extensionsToWildcards(core.audioLoader->supportedFileExtensions()).join(u" ");
    const QString allExtensions = QStringLiteral("%1 %2").arg(audioExtensions, QStringLiteral("*.cue"));

    const QString allFilter   = GuiApplication::tr("All Supported Media Files (%1)").arg(allExtensions);
    const QString filesFilter = GuiApplication::tr("Audio Files (%1)").arg(audioExtensions);

    const QStringList filters{allFilter, filesFilter};

    QUrl dir = QUrl::fromLocalFile(QDir::homePath());
    if(const auto lastPath = settings->fileValue(LastFilePath).toString(); !lastPath.isEmpty()) {
        dir = lastPath;
    }

    const auto files
        = QFileDialog::getOpenFileUrls(mainWindow.get(), GuiApplication::tr("Add Files"), dir, filters.join(u";;"));

    if(files.empty()) {
        return;
    }

    settings->fileSet(LastFilePath, files.front());

    playlistInteractor.filesToCurrentPlaylist(files);
}

void GuiApplicationPrivate::addFolders() const
{
    const auto dirs
        = QFileDialog::getExistingDirectoryUrl(mainWindow.get(), GuiApplication::tr("Add Folders"), QDir::homePath());

    if(dirs.isEmpty()) {
        return;
    }

    playlistInteractor.filesToCurrentPlaylist({dirs});
}

void GuiApplicationPrivate::openFiles(const QList<QUrl>& urls) const
{
    const QString playlistName = settings->value<Settings::Core::OpenFilesPlaylist>();
    if(settings->value<Settings::Core::OpenFilesSendTo>()) {
        playlistInteractor.filesToNewPlaylistReplace(playlistName, urls, true);
    }
    else {
        playlistInteractor.filesToNewPlaylist(playlistName, urls, true);
    }
}

void GuiApplicationPrivate::loadPlaylist() const
{
    const QString playlistExtensions = Playlist::supportedPlaylistExtensions().join(QStringLiteral(" "));
    const QString playlistFilter     = GuiApplication::tr("Playlists (%1)").arg(playlistExtensions);

    QUrl dir = QUrl::fromLocalFile(QDir::homePath());
    if(const auto lastPath = settings->fileValue(LastFilePath).toString(); !lastPath.isEmpty()) {
        dir = lastPath;
    }

    const auto files
        = QFileDialog::getOpenFileUrls(mainWindow.get(), GuiApplication::tr("Load Playlist"), dir, playlistFilter);

    if(files.empty()) {
        return;
    }

    settings->fileSet(LastFilePath, files.front());

    const QFileInfo info{files.front().toLocalFile()};

    playlistInteractor.loadPlaylist(info.completeBaseName(), files);
}

void GuiApplicationPrivate::savePlaylist() const
{
    auto* playlist = playlistController->currentPlaylist();
    if(!playlist || playlist->trackCount() == 0) {
        return;
    }

    const QString playlistFilter
        = Utils::extensionsToFilterList(core.playlistLoader->supportedSaveExtensions(), QStringLiteral("files"));

    QDir dir{QDir::homePath()};
    if(const auto lastPath = settings->fileValue(LastFilePath).toString(); !lastPath.isEmpty()) {
        dir = lastPath;
    }

    QString selectedFilter;
    const auto file
        = QFileDialog::getSaveFileName(mainWindow.get(), GuiApplication::tr("Save Playlist"),
                                       dir.absoluteFilePath(playlist->name()), playlistFilter, &selectedFilter);

    if(file.isEmpty()) {
        return;
    }

    settings->fileSet(LastFilePath, file);

    const QString extension = Utils::extensionFromFilter(selectedFilter);
    if(extension.isEmpty()) {
        return;
    }

    if(auto* parser = core.playlistLoader->parserForExtension(extension)) {
        QFile playlistFile{file};
        if(!playlistFile.open(QIODevice::WriteOnly)) {
            qCWarning(GUI_APP) << "Could not open playlist file" << file
                               << "for writing:" << playlistFile.errorString();
            return;
        }

        const QFileInfo info{playlistFile};
        const QDir playlistDir{info.absolutePath()};
        const auto pathType = static_cast<PlaylistParser::PathType>(
            settings->fileValue(Settings::Core::Internal::PlaylistSavePathType, 0).toInt());
        const bool writeMetadata = settings->fileValue(Settings::Core::Internal::PlaylistSaveMetadata, false).toBool();

        parser->savePlaylist(&playlistFile, extension, playlist->tracks(), playlistDir, pathType, writeMetadata);
    }
}

GuiApplication::GuiApplication(const CorePluginContext& core)
    : p{std::make_unique<GuiApplicationPrivate>(this, core)}
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
