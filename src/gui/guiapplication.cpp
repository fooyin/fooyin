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
#include "dialog/saveplaylistsdialog.h"
#include "dialog/searchdialog.h"
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
#include "search/searchcontroller.h"
#include "systemtrayicon.h"
#include "utils/audioutils.h"
#include "widgets.h"

#include <core/application.h>
#include <core/corepaths.h>
#include <core/coresettings.h>
#include <core/database/database.h>
#include <core/engine/enginehandler.h>
#include <core/engine/ffmpeg/ffmpegreplaygain.h>
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
#include <gui/scripting/scripteditor.h>
#include <gui/theme/fytheme.h>
#include <gui/theme/themeregistry.h>
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
#include <utils/worker.h>

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QMessageBox>
#include <QPixmapCache>
#include <QProgressDialog>
#include <QPushButton>
#include <QStyle>
#include <QStyleFactory>
#include <QTimer>

Q_LOGGING_CATEGORY(GUI_APP, "fy.gui")

namespace Fooyin {
class GuiApplicationPrivate
{
public:
    explicit GuiApplicationPrivate(GuiApplication* self_, Application* core_);

    void setupConnections();
    void initialisePlugins();

    void showPluginsNotFoundMessage();
    void initialiseTray();
    void updateWindowTitle();
    void handleTrackStatus(AudioEngine::TrackStatus status);

    static void removeExpiredCovers(const TrackList& tracks);

    void registerActions();
    void rescanTracks(const TrackList& tracks, bool onlyModified) const;

    void setupScanMenu();
    void setupRatingMenu();
    void setupUtilitiesMenu() const;
    void setupReplayGainMenu();

    void changeVolume(double delta) const;
    void mute() const;
    void setStyle() const;
    void setTheme() const;
    void setIconTheme() const;
    void registerLayouts();

    void checkTracksNeedUpdate() const;
    void showNeedReloadMessage() const;

    void showSearchPlaylistDialog();
    void showSearchLibraryDialog();
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
    void saveAllPlaylist() const;

    GuiApplication* m_self;
    Application* m_core;

    SettingsManager* m_settings;
    ActionManager* m_actionManager;
    MusicLibrary* m_library;
    PlayerController* m_playerController;
    PlaylistHandler* m_playlistHandler;

    WidgetProvider m_widgetProvider;
    GuiSettings m_guiSettings;
    LayoutProvider m_layoutProvider;
    std::unique_ptr<EditableLayout> m_editableLayout;
    std::unique_ptr<MainMenuBar> m_menubar;
    std::unique_ptr<MainWindow> m_mainWindow;
    std::unique_ptr<SystemTrayIcon> m_trayIcon;
    WidgetContext* m_mainContext;
    std::unique_ptr<PlaylistController> m_playlistController;
    PlaylistInteractor m_playlistInteractor;
    TrackSelectionController m_selectionController;
    SearchController* m_searchController;
    FFmpegReplayGain* m_replayGain;
    QThread m_replayGainThread;

    FileMenu* m_fileMenu;
    EditMenu* m_editMenu;
    ViewMenu* m_viewMenu;
    LayoutMenu* m_layoutMenu;
    PlaybackMenu* m_playbackMenu;
    LibraryMenu* m_libraryMenu;
    HelpMenu* m_helpMenu;

    PropertiesDialog* m_propertiesDialog;
    WindowController* m_windowController;
    ThemeRegistry* m_themeRegistry;

    GuiPluginContext m_guiPluginContext;

    std::unique_ptr<LogWidget> m_logWidget;
    Widgets* m_widgets;
    ScriptParser m_scriptParser;
};

GuiApplicationPrivate::GuiApplicationPrivate(GuiApplication* self_, Application* core_)
    : m_self{self_}
    , m_core{core_}
    , m_settings{m_core->settingsManager()}
    , m_actionManager{new ActionManager(m_settings, m_self)}
    , m_library{m_core->library()}
    , m_playerController{m_core->playerController()}
    , m_playlistHandler{m_core->playlistHandler()}
    , m_guiSettings{m_settings}
    , m_editableLayout{std::make_unique<EditableLayout>(m_actionManager, &m_widgetProvider, &m_layoutProvider,
                                                        m_settings)}
    , m_menubar{std::make_unique<MainMenuBar>(m_actionManager, m_settings)}
    , m_mainWindow{std::make_unique<MainWindow>(m_actionManager, m_menubar.get(), m_settings)}
    , m_mainContext{new WidgetContext(m_mainWindow.get(), Context{"Fooyin.MainWindow"}, m_self)}
    , m_playlistController{std::make_unique<PlaylistController>(m_core->playlistHandler(), m_playerController,
                                                                &m_selectionController, m_settings)}
    , m_playlistInteractor{m_core->playlistHandler(), m_playlistController.get(), m_library}
    , m_selectionController{m_actionManager, m_settings, m_playlistController.get()}
    , m_searchController{new SearchController(m_editableLayout.get(), m_self)}
    , m_replayGain{new FFmpegReplayGain(m_library)}
    , m_fileMenu{new FileMenu(m_actionManager, m_settings, m_self)}
    , m_editMenu{new EditMenu(m_actionManager, m_settings, m_self)}
    , m_viewMenu{new ViewMenu(m_actionManager, m_settings, m_self)}
    , m_layoutMenu{new LayoutMenu(m_actionManager, &m_layoutProvider, m_settings, m_self)}
    , m_playbackMenu{new PlaybackMenu(m_actionManager, m_playerController, m_settings, m_self)}
    , m_libraryMenu{new LibraryMenu(m_core, m_actionManager, m_self)}
    , m_helpMenu{new HelpMenu(m_actionManager, m_self)}
    , m_propertiesDialog{new PropertiesDialog(m_settings, m_self)}
    , m_windowController{new WindowController(m_mainWindow.get())}
    , m_themeRegistry{new ThemeRegistry(m_settings, m_self)}
    , m_guiPluginContext{m_actionManager,        &m_layoutProvider,  &m_selectionController,
                         m_searchController,     m_propertiesDialog, &m_widgetProvider,
                         m_editableLayout.get(), m_windowController, m_themeRegistry}
    , m_logWidget{std::make_unique<LogWidget>(m_settings)}
    , m_widgets{new Widgets(m_core, m_mainWindow.get(), m_guiPluginContext, &m_playlistInteractor, m_self)}
{
    setupConnections();
    registerActions();
    setupScanMenu();
    setupRatingMenu();
    setupUtilitiesMenu();
    setupReplayGainMenu();
    setStyle();
    setIconTheme();
    registerLayouts();

    m_widgets->registerWidgets();
    m_widgets->registerPages();
    m_widgets->registerPropertiesTabs();
    m_widgets->registerFontEntries();

    m_actionManager->addContextObject(m_mainContext);

    initialisePlugins();
    m_layoutProvider.findLayouts();
    m_layoutMenu->setup();
    m_editableLayout->initialise();
    m_mainWindow->setCentralWidget(m_editableLayout.get());

    auto openMainWindow = [this]() {
        m_mainWindow->open();
        if(m_settings->value<Settings::Core::FirstRun>()) {
            QMetaObject::invokeMethod(m_editableLayout.get(), &EditableLayout::showQuickSetup, Qt::QueuedConnection);
        }
        QMetaObject::invokeMethod(m_mainWindow.get(), [this]() { checkTracksNeedUpdate(); }, Qt::QueuedConnection);
    };

    if(m_core->libraryManager()->hasLibrary() && m_core->library()->isEmpty()
       && m_settings->value<Settings::Gui::WaitForTracks>()) {
        QObject::connect(m_core->library(), &MusicLibrary::tracksLoaded, openMainWindow);
    }
    else {
        openMainWindow();
    }

    initialiseTray();
}

void GuiApplicationPrivate::setupConnections()
{
    QObject::connect(m_library, &MusicLibrary::tracksMetadataChanged, m_self,
                     [](const TrackList& tracks) { removeExpiredCovers(tracks); });
    QObject::connect(m_library, &MusicLibrary::tracksMetadataChanged, &m_selectionController,
                     &TrackSelectionController::tracksUpdated);
    QObject::connect(m_library, &MusicLibrary::tracksDeleted, &m_selectionController,
                     &TrackSelectionController::tracksRemoved);

    QObject::connect(m_playerController, &PlayerController::playStateChanged, m_mainWindow.get(),
                     [this]() { updateWindowTitle(); });
    m_settings->subscribe<Settings::Gui::Internal::WindowTitleTrackScript>(m_self, [this]() { updateWindowTitle(); });
    QObject::connect(m_playerController, &PlayerController::currentTrackChanged, m_self,
                     [this]() { updateWindowTitle(); });
    QObject::connect(&m_selectionController, &TrackSelectionController::actionExecuted, m_playlistController.get(),
                     &PlaylistController::handleTrackSelectionAction);
    QObject::connect(&m_selectionController, &TrackSelectionController::requestPropertiesDialog, m_self,
                     [this]() { showPropertiesDialog(); });
    QObject::connect(m_fileMenu, &FileMenu::requestNewPlaylist, m_self, [this]() {
        if(auto* playlist = m_playlistHandler->createEmptyPlaylist()) {
            m_playlistController->changeCurrentPlaylist(playlist);
        }
    });
    QObject::connect(m_fileMenu, &FileMenu::requestExit, m_mainWindow.get(), &QMainWindow::close);
    QObject::connect(m_fileMenu, &FileMenu::requestAddFiles, m_self, [this]() { addFiles(); });
    QObject::connect(m_fileMenu, &FileMenu::requestAddFolders, m_self, [this]() { addFolders(); });
    QObject::connect(m_fileMenu, &FileMenu::requestLoadPlaylist, m_self, [this]() { loadPlaylist(); });
    QObject::connect(m_fileMenu, &FileMenu::requestSavePlaylist, m_self, [this]() { savePlaylist(); });
    QObject::connect(m_fileMenu, &FileMenu::requestSaveAllPlaylists, m_self, [this]() { saveAllPlaylist(); });
    QObject::connect(m_editMenu, &EditMenu::requestSearch, m_self, [this]() { showSearchPlaylistDialog(); });
    QObject::connect(m_libraryMenu, &LibraryMenu::requestSearch, m_self, [this]() { showSearchLibraryDialog(); });
    QObject::connect(m_viewMenu, &ViewMenu::openQuickSetup, m_editableLayout.get(), &EditableLayout::showQuickSetup);
    QObject::connect(m_viewMenu, &ViewMenu::openLog, m_logWidget.get(), &LogWidget::show);
    QObject::connect(m_viewMenu, &ViewMenu::openScriptEditor, m_self, [this]() {
        auto* scriptEditor
            = new ScriptEditor(m_core->libraryManager(), m_selectionController.selectedTrack(), m_mainWindow.get());
        scriptEditor->setAttribute(Qt::WA_DeleteOnClose);
        scriptEditor->show();
    });
    QObject::connect(m_viewMenu, &ViewMenu::showNowPlaying, m_self, [this]() {
        if(auto* activePlaylist = m_playlistHandler->activePlaylist()) {
            m_playlistController->showNowPlaying();
            m_playlistController->changeCurrentPlaylist(activePlaylist);
        }
    });
    QObject::connect(m_layoutMenu, &LayoutMenu::changeLayout, m_editableLayout.get(), &EditableLayout::changeLayout);
    QObject::connect(m_layoutMenu, &LayoutMenu::importLayout, m_self,
                     [this]() { m_layoutProvider.importLayout(m_mainWindow.get()); });
    QObject::connect(m_layoutMenu, &LayoutMenu::exportLayout, m_self,
                     [this]() { m_editableLayout->exportLayout(m_editableLayout.get()); });

    QObject::connect(&m_layoutProvider, &LayoutProvider::requestChangeLayout, m_editableLayout.get(),
                     &EditableLayout::changeLayout);

    QObject::connect(m_core->engine(), &EngineController::engineError, m_self,
                     [this](const QString& error) { showEngineError(error); });
    QObject::connect(m_core->engine(), &EngineController::trackStatusChanged, m_self,
                     [this](AudioEngine::TrackStatus status) { handleTrackStatus(status); });

    m_settings->subscribe<Settings::Gui::LayoutEditing>(m_self, [this]() { updateWindowTitle(); });
    m_settings->subscribe<Settings::Gui::IconTheme>(m_self, [this]() {
        setIconTheme();
        QPixmapCache::clear();
    });
    m_settings->subscribe<Settings::Gui::Theme>(m_self, [this]() {
        setTheme();
        setIconTheme();
    });
    m_settings->subscribe<Settings::Gui::Style>(m_self, [this]() {
        setStyle();
        setTheme();
        setIconTheme();
    });
}

void GuiApplicationPrivate::initialisePlugins()
{
    if(m_core->pluginManager()->allPluginInfo().empty()) {
        QMetaObject::invokeMethod(m_self, [this]() { showPluginsNotFoundMessage(); }, Qt::QueuedConnection);
        return;
    }

    m_core->pluginManager()->initialisePlugins<GuiPlugin>(
        [this](GuiPlugin* plugin) { plugin->initialise(m_guiPluginContext); });
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
    m_trayIcon = std::make_unique<SystemTrayIcon>(m_actionManager);

    QObject::connect(m_trayIcon.get(), &SystemTrayIcon::toggleVisibility, m_mainWindow.get(),
                     &MainWindow::toggleVisibility);

    if(m_settings->value<Settings::Gui::Internal::ShowTrayIcon>()) {
        m_trayIcon->show();
    }

    m_settings->subscribe<Settings::Gui::Internal::ShowTrayIcon>(m_self,
                                                                 [this](bool show) { m_trayIcon->setVisible(show); });
}

void GuiApplicationPrivate::updateWindowTitle()
{
    if(m_playerController->playState() == Player::PlayState::Stopped) {
        m_mainWindow->resetTitle();
        return;
    }

    const Track currentTrack = m_playerController->currentTrack();
    if(!currentTrack.isValid()) {
        m_mainWindow->resetTitle();
        return;
    }

    const QString script = m_settings->value<Settings::Gui::Internal::WindowTitleTrackScript>();
    const QString title  = m_scriptParser.evaluate(script, currentTrack);
    m_mainWindow->setTitle(title);
}

void GuiApplicationPrivate::handleTrackStatus(AudioEngine::TrackStatus status)
{
    const Track track = m_playerController->currentTrack();

    if(status == AudioEngine::TrackStatus::Invalid) {
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
    else if(status == AudioEngine::TrackStatus::Unreadable) {
        showTrackUnreableMessage(track);
    }
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
    auto* volumeUp = new QAction(Utils::iconFromTheme(Constants::Icons::VolumeHigh), GuiApplication::tr("Volume up"),
                                 m_mainWindow.get());
    m_actionManager->registerAction(volumeUp, Constants::Actions::VolumeUp);
    QObject::connect(volumeUp, &QAction::triggered, m_mainWindow.get(),
                     [this]() { changeVolume(m_settings->value<Settings::Gui::VolumeStep>()); });

    auto* volumeDown = new QAction(Utils::iconFromTheme(Constants::Icons::VolumeLow), GuiApplication::tr("Volume down"),
                                   m_mainWindow.get());
    m_actionManager->registerAction(volumeDown, Constants::Actions::VolumeDown);
    QObject::connect(volumeDown, &QAction::triggered, m_mainWindow.get(),
                     [this]() { changeVolume(-m_settings->value<Settings::Gui::VolumeStep>()); });

    auto* muteAction = new QAction(Utils::iconFromTheme(Constants::Icons::VolumeMute), GuiApplication::tr("Mute"),
                                   m_mainWindow.get());
    m_actionManager->registerAction(muteAction, Constants::Actions::Mute);
    QObject::connect(muteAction, &QAction::triggered, m_mainWindow.get(), [this]() { mute(); });

    auto setSavePlaylistState = [this]() {
        if(auto* savePlaylistCommand = m_actionManager->command(Constants::Actions::SavePlaylist)) {
            if(const auto* playlist = m_playlistController->currentPlaylist()) {
                savePlaylistCommand->action()->setEnabled(playlist->trackCount() > 0);
            }
        }
    };

    QObject::connect(m_playlistController.get(), &PlaylistController::currentPlaylistChanged, m_mainWindow.get(),
                     setSavePlaylistState);
    QObject::connect(m_playlistController->playlistHandler(), &PlaylistHandler::tracksChanged, m_mainWindow.get(),
                     setSavePlaylistState);
    QObject::connect(m_playlistController->playlistHandler(), &PlaylistHandler::tracksAdded, m_mainWindow.get(),
                     setSavePlaylistState);
    QObject::connect(m_playlistController->playlistHandler(), &PlaylistHandler::tracksRemoved, m_mainWindow.get(),
                     setSavePlaylistState);

    auto* removePlaylist = new QAction(GuiApplication::tr("Remove Playlist"), m_mainWindow.get());
    auto* removeCmd      = m_actionManager->registerAction(removePlaylist, Constants::Actions::RemovePlaylist);
    removeCmd->setDefaultShortcut(QKeySequence{Qt::CTRL | Qt::Key_W});
    QObject::connect(removePlaylist, &QAction::triggered, m_mainWindow.get(), [this]() {
        if(auto* currentPlaylist = m_playlistController->currentPlaylist()) {
            m_core->playlistHandler()->removePlaylist(currentPlaylist->id());
        }
    });
}

void GuiApplicationPrivate::rescanTracks(const TrackList& tracks, bool onlyModified) const
{
    auto* scanDialog = new QProgressDialog(GuiApplication::tr("Reading tracks…"), QStringLiteral("Abort"), 0, 100,
                                           Utils::getMainWindow());
    scanDialog->setAttribute(Qt::WA_DeleteOnClose);
    scanDialog->setModal(true);
    scanDialog->setValue(0);

    auto* library = m_core->library();

    const ScanRequest request = onlyModified ? library->scanModifiedTracks(tracks) : library->scanTracks(tracks);

    QObject::connect(library, &MusicLibrary::scanProgress, scanDialog,
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
    auto* selectionMenu = m_actionManager->actionContainer(Constants::Menus::Context::TrackSelection);
    auto* taggingMenu   = m_actionManager->createMenu(Constants::Menus::Context::Tagging);
    taggingMenu->menu()->setTitle(GuiApplication::tr("Tagging"));
    selectionMenu->addMenu(taggingMenu);

    auto* rescanAction = new QAction(GuiApplication::tr("Reload tags from file(s)"), m_mainWindow.get());
    auto* rescanChangedAction
        = new QAction(GuiApplication::tr("Reload tags from modified file(s)"), m_mainWindow.get());

    rescanAction->setStatusTip(GuiApplication::tr("Replace tags in selected tracks with tags from the file(s)"));
    rescanChangedAction->setStatusTip(
        GuiApplication::tr("Replace tags in selected tracks with tags from the file(s) if modified"));

    auto rescan = [this](const bool onlyModified) {
        if(m_selectionController.hasTracks()) {
            const auto tracks = m_selectionController.selectedTracks();
            rescanTracks(tracks, onlyModified);
        }
    };

    QObject::connect(rescanAction, &QAction::triggered, m_mainWindow.get(), [rescan]() { rescan(false); });
    QObject::connect(rescanChangedAction, &QAction::triggered, m_mainWindow.get(), [rescan]() { rescan(true); });
    taggingMenu->menu()->addAction(rescanAction);
    taggingMenu->menu()->addAction(rescanChangedAction);

    QObject::connect(&m_selectionController, &TrackSelectionController::selectionChanged, m_mainWindow.get(),
                     [this, rescanAction]() { rescanAction->setEnabled(m_selectionController.hasTracks()); });
}

void GuiApplicationPrivate::setupRatingMenu()
{
    auto* selectionMenu = m_actionManager->actionContainer(Constants::Menus::Context::TrackSelection);
    auto* taggingMenu   = m_actionManager->createMenu(Constants::Menus::Context::Tagging);
    taggingMenu->menu()->setTitle(GuiApplication::tr("Tagging"));
    selectionMenu->addMenu(taggingMenu);

    auto* rate0 = new QAction(GuiApplication::tr("Rate 0"), m_mainWindow.get());
    auto* rate1 = new QAction(GuiApplication::tr("Rate 1"), m_mainWindow.get());
    auto* rate2 = new QAction(GuiApplication::tr("Rate 2"), m_mainWindow.get());
    auto* rate3 = new QAction(GuiApplication::tr("Rate 3"), m_mainWindow.get());
    auto* rate4 = new QAction(GuiApplication::tr("Rate 4"), m_mainWindow.get());
    auto* rate5 = new QAction(GuiApplication::tr("Rate 5"), m_mainWindow.get());

    m_actionManager->registerAction(rate0, Constants::Actions::Rate0);
    m_actionManager->registerAction(rate1, Constants::Actions::Rate1);
    m_actionManager->registerAction(rate2, Constants::Actions::Rate2);
    m_actionManager->registerAction(rate3, Constants::Actions::Rate3);
    m_actionManager->registerAction(rate4, Constants::Actions::Rate4);
    m_actionManager->registerAction(rate5, Constants::Actions::Rate5);

    auto setRating = [this](const int rating) {
        if(m_selectionController.selectedTrackCount() == 1) {
            const auto tracks = m_selectionController.selectedTracks();
            Track track       = tracks.front();
            const int stars   = rating * 2;
            if(track.ratingStars() != stars) {
                track.setRatingStars(stars);
                if(m_settings->value<Settings::Core::SaveRatingToMetadata>()) {
                    m_core->library()->writeTrackMetadata({track});
                }
                else {
                    m_core->library()->updateTrackStats(track);
                }
            }
        }
    };

    QObject::connect(rate0, &QAction::triggered, m_mainWindow.get(), [setRating]() { setRating(0); });
    QObject::connect(rate1, &QAction::triggered, m_mainWindow.get(), [setRating]() { setRating(1); });
    QObject::connect(rate2, &QAction::triggered, m_mainWindow.get(), [setRating]() { setRating(2); });
    QObject::connect(rate3, &QAction::triggered, m_mainWindow.get(), [setRating]() { setRating(3); });
    QObject::connect(rate4, &QAction::triggered, m_mainWindow.get(), [setRating]() { setRating(4); });
    QObject::connect(rate5, &QAction::triggered, m_mainWindow.get(), [setRating]() { setRating(5); });

    auto* ratingMenu = new QMenu(GuiApplication::tr("Rating"), taggingMenu->menu());
    ratingMenu->addAction(rate0);
    ratingMenu->addAction(rate1);
    ratingMenu->addAction(rate2);
    ratingMenu->addAction(rate3);
    ratingMenu->addAction(rate4);
    ratingMenu->addAction(rate5);
    taggingMenu->menu()->addMenu(ratingMenu);

    QObject::connect(&m_selectionController, &TrackSelectionController::selectionChanged, m_mainWindow.get(),
                     [this, ratingMenu]() { ratingMenu->setEnabled(m_selectionController.selectedTrackCount() == 1); });
}

void GuiApplicationPrivate::setupUtilitiesMenu() const
{
    auto* selectionMenu = m_actionManager->actionContainer(::Fooyin::Constants::Menus::Context::TrackSelection);
    auto* utilitiesMenu = m_actionManager->createMenu(::Fooyin::Constants::Menus::Context::Utilities);
    utilitiesMenu->menu()->setTitle(GuiApplication::tr("Utilities"));
    selectionMenu->addMenu(utilitiesMenu);
}

<<<<<<< HEAD
void GuiApplicationPrivate::changeVolume(double delta) const
{
    const double currentVolume = std::max(m_settings->value<Settings::Core::OutputVolume>(), 0.01);
    const double currentDb     = Audio::volumeToDb(currentVolume);
    const double newVolume     = Audio::dbToVolume(currentDb + delta);

    m_settings->set<Settings::Core::OutputVolume>(newVolume);
}

void GuiApplicationPrivate::setupReplayGainMenu()
{
    auto* selectionMenu  = m_actionManager->actionContainer(Constants::Menus::Context::TrackSelection);
    auto* replayGainMenu = m_actionManager->createMenu(Constants::Menus::Context::ReplayGain);
    replayGainMenu->menu()->setTitle(GuiApplication::tr("ReplayGain"));
    selectionMenu->addMenu(replayGainMenu);

    auto* replayGainTrackAction
        = new QAction(GuiApplication::tr("Calculate ReplayGain track values"), m_mainWindow.get());
    auto* replayGainAlbumAction
        = new QAction(GuiApplication::tr("Calculate ReplayGain album values"), m_mainWindow.get());
    replayGainTrackAction->setStatusTip(
        GuiApplication::tr("Calculate ReplayGain values for selected file(s), considering each file individually"));
    replayGainAlbumAction->setStatusTip(GuiApplication::tr(
        "Calculate ReplayGain values for selected files, considering all files as part of one album"));

    QObject::connect(m_replayGain, &Worker::finished, [this]() {
        m_replayGain->closeThread();
        m_replayGainThread.quit();
    });

    const auto calcGain = [this](bool asAlbum) {
        m_replayGain->moveToThread(&m_replayGainThread);
        m_replayGainThread.start();
        QMetaObject::invokeMethod(
            m_replayGain,
            [this, asAlbum]() { m_replayGain->calculate(m_selectionController.selectedTracks(), asAlbum); },
            Qt::QueuedConnection);
    };
    QObject::connect(replayGainTrackAction, &QAction::triggered, m_mainWindow.get(), [calcGain] { calcGain(false); });
    QObject::connect(replayGainAlbumAction, &QAction::triggered, m_mainWindow.get(), [calcGain] { calcGain(true); });
    QObject::connect(&m_selectionController, &TrackSelectionController::selectionChanged, m_mainWindow.get(),
                     [this, replayGainAlbumAction] {
                         replayGainAlbumAction->setEnabled(m_selectionController.selectedTrackCount() > 1);
                     });

    replayGainMenu->menu()->addAction(replayGainTrackAction);
    replayGainMenu->menu()->addAction(replayGainAlbumAction);
}

void GuiApplicationPrivate::mute() const
{
    const double volume = m_settings->value<Settings::Core::OutputVolume>();
    if(volume > 0.0) {
        m_settings->set<Settings::Core::Internal::MuteVolume>(volume);
        m_settings->set<Settings::Core::OutputVolume>(0.0);
    }
    else {
        m_settings->set<Settings::Core::OutputVolume>(m_settings->value<Settings::Core::Internal::MuteVolume>());
    }
}

void GuiApplicationPrivate::setStyle() const
{
    const auto currStyle = m_settings->value<Settings::Gui::Style>();
    if(auto* style = QStyleFactory::create(currStyle)) {
        QApplication::setStyle(style);
    }
    else if(auto* systemStyle = QStyleFactory::create(m_settings->value<Settings::Gui::Internal::SystemStyle>())) {
        QApplication::setStyle(systemStyle);
    }
}

void GuiApplicationPrivate::setTheme() const
{
    const auto currTheme = m_settings->value<Settings::Gui::Theme>().value<FyTheme>();

    auto newPalette = m_settings->value<Settings::Gui::Internal::SystemPalette>().value<QPalette>();
    for(const auto& [key, colour] : Utils::asRange(currTheme.colours)) {
        newPalette.setColor(key.group, key.role, colour);
    }
    QApplication::setPalette(newPalette);

    {
        // Reset all fonts to default first
        const auto systemFont = m_settings->value<Settings::Gui::Internal::SystemFont>().value<QFont>();
        QApplication::setFont(systemFont);
        const auto fontEntries = m_themeRegistry->fontEntries();
        for(const auto& [className, _] : fontEntries) {
            if(!className.isEmpty()) {
                QApplication::setFont(systemFont, className.toUtf8().constData());
            }
        }
    }

    auto updateFonts = [currTheme]() {
        for(const auto& [className, font] : Utils::asRange(currTheme.fonts)) {
            if(className.isEmpty()) {
                QApplication::setFont(font);
            }
            else {
                QApplication::setFont(font, className.toUtf8().constData());
            }
        }
        QPixmapCache::clear();
    };
    updateFonts();

    // Fonts can sometimes reset, so set again
    QTimer::singleShot(0, m_self, [updateFonts]() { updateFonts(); });
}

void GuiApplicationPrivate::setIconTheme() const
{
    const auto iconTheme = static_cast<IconThemeOption>(m_settings->value<Settings::Gui::IconTheme>());
    switch(iconTheme) {
        case(IconThemeOption::AutoDetect):
            QIcon::setThemeName(Utils::isDarkMode() ? QString::fromLatin1(Constants::DarkIconTheme)
                                                    : QString::fromLatin1(Constants::LightIconTheme));
            break;
        case(IconThemeOption::Light):
            QIcon::setThemeName(QString::fromLatin1(Constants::LightIconTheme));
            break;
        case(IconThemeOption::Dark):
            QIcon::setThemeName(QString::fromLatin1(Constants::DarkIconTheme));
            break;
        case(IconThemeOption::System):
            QIcon::setThemeName(m_settings->value<Settings::Gui::Internal::SystemIconTheme>());
            QIcon::setFallbackThemeName(QString::fromLatin1(Constants::DarkIconTheme));
            return;
    }

    QIcon::setFallbackThemeName(m_settings->value<Settings::Gui::Internal::SystemIconTheme>());
}

void GuiApplicationPrivate::registerLayouts()
{
    m_layoutProvider.registerLayout(R"({"Name":"Empty"})");

    m_layoutProvider.registerLayout(
        R"({"Name":"Simple","Widgets":[{"SplitterVertical":{"State":"AAAA/wAAAAEAAAADAAAAHAAAAn0AAAAXAP////8BAAAAAgA=",
            "Widgets":[{"SplitterHorizontal":{"State":"AAAA/wAAAAEAAAAEAAAAggAABqEAAAA+AAAAHAD/////AQAAAAEA","Widgets":[
            {"PlayerControls":{}},{"SeekBar":{}},{"PlaylistControls":{}},{"VolumeControls":{}}]}},{"SplitterHorizontal":{
            "State":"AAAA/wAAAAEAAAACAAAA9QAABAoA/////wEAAAABAA==","Widgets":[{"SplitterVertical":{
            "State":"AAAA/wAAAAEAAAACAAABfQAAAPEA/////wEAAAACAA==","Widgets":[{"LibraryTree":{}},{"ArtworkPanel":{}}]}},
            {"PlaylistTabs":{"Widgets":[{"Playlist":{}}]}}]}},{"StatusBar":{}}]}}]})");

    m_layoutProvider.registerLayout(
        R"({"Name":"Vision","Widgets":[{"SplitterVertical":{"State":"AAAA/wAAAAEAAAADAAAAHAAAA6EAAAAWAP////8BAAAAAgA=",
            "Widgets":[{"SplitterHorizontal":{"State":"AAAA/wAAAAEAAAAEAAAAiQAABk8AAABHAAAAIwD/////AQAAAAEA",
            "Widgets":[{"PlayerControls":{}},{"SeekBar":{}},{"PlaylistControls":{}},{"VolumeControls":{}}]}},
            {"SplitterHorizontal":{"State":"AAAA/wAAAAEAAAACAAADuwAAA48A/////wEAAAABAA==","Widgets":[
            {"TabStack":{"Position":"West","State":"Artwork\u001fInfo\u001fLibrary Tree\u001fPlaylist Organiser",
            "Widgets":[{"ArtworkPanel":{}},{"SelectionInfo":{}},{"LibraryTree":{"Grouping":"Artist/Album"}},
            {"PlaylistOrganiser":{}}]}},{"Playlist":{}}]}},{"StatusBar":{}}]}}]})");

    m_layoutProvider.registerLayout(
        R"({"Name":"Browser","Widgets":[{"SplitterVertical":{"State":"AAAA/wAAAAEAAAADAAAAFwAAA6YAAAAWAP////8BAAAAAgA=",
            "Widgets":[{"SplitterHorizontal":{"State":"AAAA/wAAAAEAAAAEAAAAcgAABRoAAAA2AAAAGAD/////AQAAAAEA",
            "Widgets":[{"PlayerControls":{}},{"SeekBar":{}},{"PlaylistControls":{}},{"VolumeControls":{}}]}},
            {"SplitterHorizontal":{"State":"AAAA/wAAAAEAAAACAAACeAAAAnoA/////wEAAAABAA==x","Widgets":[{"DirectoryBrowser":{}},
            {"ArtworkPanel":{}}]}},{"StatusBar":{}}]}}]})");
}

void GuiApplicationPrivate::checkTracksNeedUpdate() const
{
    const auto libraryOutOfDate = [this](int threshold) -> bool {
        const int prev = m_core->database()->previousRevision();
        const int curr = m_core->database()->currentRevision();
        return prev > 0 && prev < threshold && curr >= threshold;
    };

    if(m_library->hasLibrary()) {
        if(libraryOutOfDate(7)     /* changed codec storage type */
           || libraryOutOfDate(11) /* removed ReplayGain data in extra tags */
           || libraryOutOfDate(13) /* added more tech fields */) {
            showNeedReloadMessage();
        }
    }
}

void GuiApplicationPrivate::showNeedReloadMessage() const
{
    QMessageBox message;
    message.setIcon(QMessageBox::Warning);
    message.setText(GuiApplication::tr("Reload Required"));
    message.setInformativeText(GuiApplication::tr(
        "Due to a database change, tracks should be reloaded from disk to update their saved metadata."));

    message.addButton(QMessageBox::Ok);
    if(auto* button = message.button(QMessageBox::Ok)) {
        button->setText(GuiApplication::tr("Reload Now"));
    }
    QPushButton* okButton = message.addButton(GuiApplication::tr("OK"), QMessageBox::ActionRole);
    okButton->setIcon(Utils::iconFromTheme(Constants::Icons::Stop));
    message.setDefaultButton(QMessageBox::Ok);

    message.exec();

    if(message.clickedButton() != okButton) {
        m_library->rescanAll();
    }
}

void GuiApplicationPrivate::showSearchPlaylistDialog()
{
    auto* coverProvider = new CoverProvider(m_core->audioLoader(), m_settings, m_self);
    auto* search        = new SearchDialog(m_actionManager, &m_playlistInteractor, coverProvider, m_core,
                                           PlaylistWidget::Mode::DetachedPlaylist);
    search->setAttribute(Qt::WA_DeleteOnClose);
    coverProvider->setParent(search);

    search->show();
}

void GuiApplicationPrivate::showSearchLibraryDialog()
{
    auto* coverProvider = new CoverProvider(m_core->audioLoader(), m_settings, m_self);
    auto* search        = new SearchDialog(m_actionManager, &m_playlistInteractor, coverProvider, m_core,
                                           PlaylistWidget::Mode::DetachedLibrary);
    search->setAttribute(Qt::WA_DeleteOnClose);
    coverProvider->setParent(search);

    search->show();
}

void GuiApplicationPrivate::showPropertiesDialog() const
{
    const auto tracks = m_selectionController.selectedTracks();
    if(!tracks.empty()) {
        m_propertiesDialog->show(tracks);
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
    if(m_settings->fileValue(Settings::Core::Internal::PlaylistSkipUnavailable).toBool()) {
        m_playerController->next();
        if(m_playerController->playState() == Player::PlayState::Playing) {
            m_playerController->play();
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
        m_settings->fileSet(Settings::Core::Internal::PlaylistSkipUnavailable, true);
    }

    if(message.clickedButton() == stopButton) {
        m_playerController->stop();
    }
    else {
        m_playerController->next();
        if(m_playerController->playState() == Player::PlayState::Playing) {
            m_playerController->play();
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
        = Utils::extensionsToWildcards(m_core->audioLoader()->supportedFileExtensions()).join(u" ");
    const QString allExtensions = QStringLiteral("%1 %2").arg(audioExtensions, QStringLiteral("*.cue"));

    const QString allFilter   = GuiApplication::tr("All Supported Media Files (%1)").arg(allExtensions);
    const QString filesFilter = GuiApplication::tr("Audio Files (%1)").arg(audioExtensions);

    const QStringList filters{allFilter, filesFilter};

    QUrl dir = QUrl::fromLocalFile(QDir::homePath());
    if(const auto lastPath = m_settings->fileValue(Settings::Gui::Internal::LastFilePath).toString();
       !lastPath.isEmpty()) {
        dir = lastPath;
    }

    const auto files
        = QFileDialog::getOpenFileUrls(m_mainWindow.get(), GuiApplication::tr("Add Files"), dir, filters.join(u";;"));

    if(files.empty()) {
        return;
    }

    m_settings->fileSet(Settings::Gui::Internal::LastFilePath, files.front());

    m_playlistInteractor.filesToCurrentPlaylist(files);
}

void GuiApplicationPrivate::addFolders() const
{
    const auto dirs
        = QFileDialog::getExistingDirectoryUrl(m_mainWindow.get(), GuiApplication::tr("Add Folders"), QDir::homePath());

    if(dirs.isEmpty()) {
        return;
    }

    m_playlistInteractor.filesToCurrentPlaylist({dirs});
}

void GuiApplicationPrivate::openFiles(const QList<QUrl>& urls) const
{
    const QString playlistName = m_settings->value<Settings::Core::OpenFilesPlaylist>();
    if(m_settings->value<Settings::Core::OpenFilesSendTo>()) {
        m_playlistInteractor.filesToNewPlaylistReplace(playlistName, urls, true);
    }
    else {
        m_playlistInteractor.filesToNewPlaylist(playlistName, urls, true);
    }
}

void GuiApplicationPrivate::loadPlaylist() const
{
    const QString allExtensions
        = Utils::extensionsToWildcards(m_core->playlistLoader()->supportedExtensions()).join(u" ");
    const auto playlistFilter
        = Utils::extensionsToFilterList(m_core->playlistLoader()->supportedExtensions(), QStringLiteral("playlists"));

    const QString allFilter = GuiApplication::tr("All Supported Playlists (%1)").arg(allExtensions);

    const QStringList filters{allFilter, playlistFilter};

    QUrl dir = QUrl::fromLocalFile(QDir::homePath());
    if(const auto lastPath = m_settings->fileValue(Settings::Gui::Internal::LastFilePath).toString();
       !lastPath.isEmpty()) {
        dir = lastPath;
    }

    const auto files = QFileDialog::getOpenFileUrls(m_mainWindow.get(), GuiApplication::tr("Load Playlist"), dir,
                                                    filters.join(u";;"));

    if(files.empty()) {
        return;
    }

    m_settings->fileSet(Settings::Gui::Internal::LastFilePath, files.front());

    const QFileInfo info{files.front().toLocalFile()};

    m_playlistInteractor.loadPlaylist(info.completeBaseName(), files);
}

void GuiApplicationPrivate::savePlaylist() const
{
    auto* playlist = m_playlistController->currentPlaylist();
    if(!playlist || playlist->trackCount() == 0) {
        return;
    }

    const QString playlistFilter
        = Utils::extensionsToFilterList(m_core->playlistLoader()->supportedSaveExtensions(), QStringLiteral("files"));

    QDir dir{QDir::homePath()};
    if(const auto lastPath = m_settings->fileValue(Settings::Gui::Internal::LastFilePath).toString();
       !lastPath.isEmpty()) {
        dir = lastPath;
    }

    QString selectedFilter;
    QString file
        = QFileDialog::getSaveFileName(m_mainWindow.get(), GuiApplication::tr("Save Playlist"),
                                       dir.absoluteFilePath(playlist->name()), playlistFilter, &selectedFilter);
    if(file.isEmpty()) {
        return;
    }

    m_settings->fileSet(Settings::Gui::Internal::LastFilePath, file);

    const QString extension = Utils::extensionFromFilter(selectedFilter);
    if(extension.isEmpty()) {
        return;
    }

    const QFileInfo info{file};
    if(info.suffix() != extension) {
        file.append(u"." + extension);
    }

    if(auto* parser = m_core->playlistLoader()->parserForExtension(extension)) {
        QFile playlistFile{file};
        if(!playlistFile.open(QIODevice::WriteOnly)) {
            qCWarning(GUI_APP) << "Could not open playlist file" << file
                               << "for writing:" << playlistFile.errorString();
            return;
        }

        const QDir playlistDir{info.absolutePath()};
        const auto pathType = static_cast<PlaylistParser::PathType>(
            m_settings->fileValue(Settings::Core::Internal::PlaylistSavePathType, 0).toInt());
        const bool writeMetadata
            = m_settings->fileValue(Settings::Core::Internal::PlaylistSaveMetadata, false).toBool();

        parser->savePlaylist(&playlistFile, extension, playlist->tracks(), playlistDir, pathType, writeMetadata);
    }
}

void GuiApplicationPrivate::saveAllPlaylist() const
{
    auto* savePlaylists = new SavePlaylistsDialog(m_core, m_mainWindow.get());
    savePlaylists->setAttribute(Qt::WA_DeleteOnClose);
    savePlaylists->show();
}

GuiApplication::GuiApplication(Application* core)
    : p{std::make_unique<GuiApplicationPrivate>(this, core)}
{
    auto updateCache = [](const int sizeMb) {
        QPixmapCache::setCacheLimit(sizeMb * 1024);
    };

    updateCache(p->m_settings->value<Settings::Gui::Internal::PixmapCacheSize>());
    p->m_settings->subscribe<Settings::Gui::Internal::PixmapCacheSize>(this, updateCache);

    QObject::connect(p->m_settings->settingsDialog(), &SettingsDialogController::opening, this, [this]() {
        const bool isLayoutEditing = p->m_settings->value<Settings::Gui::LayoutEditing>();
        // Layout editing mode overrides the global action context, so disable it until the dialog closes
        p->m_settings->set<Settings::Gui::LayoutEditing>(false);
        QObject::connect(
            p->m_settings->settingsDialog(), &SettingsDialogController::closing, this,
            [this, isLayoutEditing]() { p->m_settings->set<Settings::Gui::LayoutEditing>(isLayoutEditing); },
            Qt::SingleShotConnection);
    });
}

GuiApplication::~GuiApplication() = default;

void GuiApplication::shutdown()
{
    p->m_actionManager->saveSettings();
    p->m_editableLayout->saveLayout();
    p->m_editableLayout.reset();
    p->m_playlistController.reset();
    p->m_mainWindow.reset();
}

void GuiApplication::raise()
{
    p->m_mainWindow->raiseWindow();
}

void GuiApplication::openFiles(const QList<QUrl>& files)
{
    if(p->m_playlistController->playlistsHaveLoaded()) {
        p->openFiles(files);
        return;
    }

    QObject::connect(
        p->m_playlistController.get(), &PlaylistController::playlistsLoaded, this,
        [this, files]() { p->openFiles(files); }, Qt::SingleShotConnection);
}
} // namespace Fooyin

#include "moc_guiapplication.cpp"
