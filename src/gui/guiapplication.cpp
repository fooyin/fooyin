/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <luket@pm.me>
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

#include "artwork/artworkdialog.h"
#include "artwork/artworkfinder.h"
#include "artwork/artworksaveutils.h"
#include "dialog/autoplaylistdialog.h"
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
#include "playlist/manager/playlistmanagerwidget.h"
#include "playlist/playlistcontroller.h"
#include "playlist/playlistinteractor.h"
#include "playlist/playlistuicontroller.h"
#include "playlist/playlistwidget.h"
#include "queueviewer/queueviewer.h"
#include "scripting/scriptcommandhandler.h"
#include "scripting/scriptvariableproviders.h"
#include "search/searchcontroller.h"
#include "search/searchwidget.h"
#include "systemtrayicon.h"
#include "widgets.h"
#include <gui/playlist/currentplaylistcontroller.h>

#include <core/application.h>
#include <core/corepaths.h>
#include <core/coresettings.h>
#include <core/database/database.h>
#include <core/engine/enginehandler.h>
#include <core/engine/enginehelpers.h>
#include <core/internalcoresettings.h>
#include <core/library/librarymanager.h>
#include <core/library/musiclibrary.h>
#include <core/playlist/playlisthandler.h>
#include <core/playlist/playlistloader.h>
#include <core/playlist/playlistparser.h>
#include <core/plugins/plugininfo.h>
#include <core/plugins/pluginmanager.h>
#include <core/scripting/scriptenvironmenthelpers.h>
#include <gui/coverprovider.h>
#include <gui/coverrepository.h>
#include <gui/editablelayout.h>
#include <gui/guiconstants.h>
#include <gui/guisettings.h>
#include <gui/guistyleprovider.h>
#include <gui/guiutils.h>
#include <gui/iconloader.h>
#include <gui/layoutprovider.h>
#include <gui/plugins/dspguiplugin.h>
#include <gui/plugins/guiplugin.h>
#include <gui/plugins/guiplugincontext.h>
#include <gui/plugins/pluginconfigguiplugin.h>
#include <gui/propertiesdialog.h>
#include <gui/scripting/scripteditor.h>
#include <gui/statusevent.h>
#include <gui/theme/fytheme.h>
#include <gui/theme/themeregistry.h>
#include <gui/trackselectioncontroller.h>
#include <gui/widgetprovider.h>
#include <gui/widgets/elapsedprogressdialog.h>
#include <gui/windowcontroller.h>
#include <utils/actions/actioncontainer.h>
#include <utils/actions/actionmanager.h>
#include <utils/actions/command.h>
#include <utils/audioutils.h>
#include <utils/fileutils.h>
#include <utils/logging/logwidget.h>
#include <utils/settings/advancedsettingsregistry.h>
#include <utils/settings/settingsdialogcontroller.h>
#include <utils/settings/settingsmanager.h>
#include <utils/utils.h>

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QImageReader>
#include <QInputDialog>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QMessageBox>
#include <QMimeDatabase>
#include <QPixmapCache>
#include <QPointer>
#include <QPushButton>
#include <QStyle>
#include <QStyleFactory>
#include <QTimer>

Q_LOGGING_CATEGORY(GUI_APP, "fy.gui")

using namespace std::chrono_literals;
using namespace Qt::StringLiterals;

constexpr auto ThemeUpdateDelayMs = 50;

namespace Fooyin {
namespace {
QString pluginIdentifierForRoot(const PluginManager& pluginManager, const QObject* root)
{
    if(!root) {
        return {};
    }

    for(const auto& [pluginId, pluginInfo] : pluginManager.allPluginInfo()) {
        if(pluginInfo && pluginInfo->root() == root) {
            return pluginId;
        }
    }

    return {};
}

std::pair<QString, QString> resolveIconThemes(SettingsManager* settings)
{
    if(!settings) {
        return {};
    }

    QString primaryTheme;
    QString fallbackTheme;

    const auto iconTheme = static_cast<IconThemeOption>(settings->value<Settings::Gui::IconTheme>());
    switch(iconTheme) {
        case IconThemeOption::AutoDetect:
            primaryTheme = Utils::isDarkMode() ? QString::fromLatin1(Constants::DarkIconTheme)
                                               : QString::fromLatin1(Constants::LightIconTheme);
            break;
        case IconThemeOption::Light:
            primaryTheme = QString::fromLatin1(Constants::LightIconTheme);
            break;
        case IconThemeOption::Dark:
            primaryTheme = QString::fromLatin1(Constants::DarkIconTheme);
            break;
        case IconThemeOption::System:
            fallbackTheme = Utils::isDarkMode() ? QString::fromLatin1(Constants::DarkIconTheme)
                                                : QString::fromLatin1(Constants::LightIconTheme);
            break;
    }

    return {primaryTheme, fallbackTheme};
}

void applyInitialIconThemes(SettingsManager* settings)
{
    const auto [primaryTheme, fallbackTheme] = resolveIconThemes(settings);
    Gui::setThemeIconOverrides(primaryTheme, fallbackTheme);
}

Application* prepareCoreForGuiApplication(Application* core)
{
    // Some widgets and actions resolve icons during construction, so icon
    // theme overrides must be set up before initialising GuiApplication members.
    applyInitialIconThemes(core ? core->settingsManager() : nullptr);
    return core;
}
} // namespace

GuiApplication::GuiApplication(Application* core)
    : m_core{prepareCoreForGuiApplication(core)}
    , m_settings{m_core->settingsManager()}
    , m_actionManager{new ActionManager(m_settings, this)}
    , m_library{m_core->library()}
    , m_playerController{m_core->playerController()}
    , m_playlistHandler{m_core->playlistHandler()}
    , m_widgetProvider{std::make_unique<WidgetProvider>()}
    , m_guiSettings{m_settings}
    , m_layoutProvider{std::make_unique<LayoutProvider>()}
    , m_editableLayout{std::make_unique<EditableLayout>(m_actionManager, m_widgetProvider.get(), m_layoutProvider.get(),
                                                        m_settings)}
    , m_menubar{std::make_unique<MainMenuBar>(m_actionManager, m_settings)}
    , m_mainWindow{std::make_unique<MainWindow>(m_actionManager, m_menubar.get(), m_library, m_settings)}
    , m_mainContext{new WidgetContext(m_mainWindow.get(), Context{"Fooyin.MainWindow"}, this)}
    , m_playlistController{std::make_unique<PlaylistController>(m_core)}
    , m_playlistInteractor{m_core->playlistHandler(), m_playlistController.get(), m_library, m_settings}
    , m_selectionController{std::make_unique<TrackSelectionController>(m_actionManager, m_core->audioLoader().get(),
                                                                       m_settings, m_playlistController.get())}
    , m_searchController{new SearchController(m_editableLayout.get(), this)}
    , m_fileMenu{new FileMenu(m_actionManager, this)}
    , m_editMenu{new EditMenu(m_actionManager, m_settings, this)}
    , m_viewMenu{new ViewMenu(m_actionManager, m_settings, this)}
    , m_layoutMenu{new LayoutMenu(m_actionManager, m_layoutProvider.get(), m_settings, this)}
    , m_playbackMenu{new PlaybackMenu(m_actionManager, m_playerController, m_settings, this)}
    , m_libraryMenu{new LibraryMenu(m_core, m_actionManager, this)}
    , m_helpMenu{new HelpMenu(m_actionManager, this)}
    , m_propertiesDialog{new PropertiesDialog(m_actionManager, m_settings, this)}
    , m_scriptCommandHandler{std::make_unique<ScriptCommandHandler>(m_actionManager, m_playerController,
                                                                    m_propertiesDialog)}
    , m_windowController{new WindowController(m_mainWindow.get())}
    , m_themeRegistry{new ThemeRegistry(m_settings, this)}
    , m_styleProvider{new GuiStyleProvider(m_settings, this)}
    , m_advancedSettingsRegistry{std::make_unique<AdvancedSettingsRegistry>(m_settings)}
    , m_coverRepository{new CoverRepository(m_core->audioLoader(), m_core->remoteIoService(), m_settings, this)}
    , m_guiPluginContext{m_actionManager,
                         m_layoutProvider.get(),
                         m_selectionController.get(),
                         m_searchController,
                         m_playlistController.get(),
                         m_propertiesDialog,
                         m_scriptCommandHandler.get(),
                         m_widgetProvider.get(),
                         m_editableLayout.get(),
                         m_windowController,
                         m_themeRegistry,
                         m_styleProvider,
                         m_advancedSettingsRegistry.get(),
                         m_coverRepository}
    , m_logWidget{std::make_unique<LogWidget>(m_settings)}
    , m_widgets{new Widgets(m_core, this, m_guiPluginContext, m_mainWindow.get(), &m_playlistInteractor, this)}
    , m_coverProvider{m_coverRepository}
    , m_themeUpdatePending{false}
    , m_refreshSystemBaseline{false}
    , m_applyingTheme{false}
    , m_resolvedAppStyleRevision{0}
{
    m_coverRepository->setPendingTrackCoverProvider(m_core->pendingTrackCoverProvider());

    m_scriptParser.addProvider(playlistVariableProvider());

    QObject::connect(m_settings->settingsDialog(), &SettingsDialogController::opening, this, [this]() {
        const bool isLayoutEditing = m_settings->value<Settings::Gui::LayoutEditing>();
        // Layout editing mode overrides the global action context, so disable it until the dialog closes
        m_settings->set<Settings::Gui::LayoutEditing>(false);
        QObject::connect(
            m_settings->settingsDialog(), &SettingsDialogController::closing, this,
            [this, isLayoutEditing]() { m_settings->set<Settings::Gui::LayoutEditing>(isLayoutEditing); },
            Qt::SingleShotConnection);
    });
}

void GuiApplication::initialise()
{
    qApp->installEventFilter(this);

    setupConnections();
    registerActions();
    setupScanMenu();
    setupRatingMenu();
    setupUtilitiesMenu();
    setStyle();
    setIconTheme();
    registerLayouts();

    QImageReader::setAllocationLimit(m_settings->value<Settings::Gui::Internal::ImageAllocationLimit>());

    m_widgets->registerWidgets();
    m_widgets->registerPages();
    m_widgets->registerAdvancedSettings();
    m_widgets->registerDspSettings();
    m_widgets->registerPropertiesTabs();
    m_widgets->registerFontEntries();

    m_actionManager->addContextObject(m_mainContext);

    initialisePlugins();

    m_widgets->registerDspWidgets();
    m_viewMenu->registerDspSettingsActions(m_widgets->dspSettingsRegistry(), m_widgets->dspSettingsController());
    m_layoutProvider->findLayouts();
    m_layoutMenu->setup();
    m_editableLayout->initialise();
    m_mainWindow->setCentralWidget(m_editableLayout.get());
    m_searchController->loadWidgets();

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

void GuiApplication::setupConnections()
{
    QObject::connect(m_library, &MusicLibrary::tracksMetadataChanged, this, &GuiApplication::removeExpiredCovers);
    QObject::connect(m_library, &MusicLibrary::tracksMetadataChanged, m_selectionController.get(),
                     &TrackSelectionController::tracksUpdated);
    QObject::connect(m_library, &MusicLibrary::tracksUpdated, m_selectionController.get(),
                     &TrackSelectionController::tracksUpdated);
    QObject::connect(m_library, &MusicLibrary::tracksDeleted, m_selectionController.get(),
                     &TrackSelectionController::tracksRemoved);
    QObject::connect(m_library, &MusicLibrary::tracksDeleted, this, &GuiApplication::handleTracksDeleted);

    QObject::connect(m_playerController, &PlayerController::playStateChanged, this, &GuiApplication::updateWindowTitle);
    QObject::connect(m_playerController, &PlayerController::playlistTrackUpdated, this,
                     &GuiApplication::updateWindowTitle);
    QObject::connect(m_playerController, &PlayerController::positionChangedSeconds, this,
                     &GuiApplication::updateWindowTitle);
    QObject::connect(m_playerController, &PlayerController::trackQueueChanged, this,
                     &GuiApplication::updateWindowTitle);
    QObject::connect(m_playlistHandler, &PlaylistHandler::activePlaylistChanged, this,
                     &GuiApplication::updateWindowTitle);

    const auto updateTitleForActivePlaylist = [this](Playlist* playlist) {
        if(playlist == m_playlistHandler->activePlaylist()) {
            updateWindowTitle();
        }
    };
    QObject::connect(m_playlistHandler, &PlaylistHandler::tracksAdded, this, updateTitleForActivePlaylist);
    QObject::connect(m_playlistHandler, &PlaylistHandler::tracksChanged, this, updateTitleForActivePlaylist);
    QObject::connect(m_playlistHandler, &PlaylistHandler::tracksUpdated, this, updateTitleForActivePlaylist);
    QObject::connect(m_playlistHandler, &PlaylistHandler::tracksRemoved, this, updateTitleForActivePlaylist);

    m_settings->subscribe<Settings::Gui::Internal::WindowTitleTrackScript>(this, &GuiApplication::updateWindowTitle);
    m_settings->subscribe<Settings::Gui::RatingFullStarSymbol>(this, &GuiApplication::updateWindowTitle);
    m_settings->subscribe<Settings::Gui::RatingHalfStarSymbol>(this, &GuiApplication::updateWindowTitle);
    m_settings->subscribe<Settings::Gui::RatingEmptyStarSymbol>(this, &GuiApplication::updateWindowTitle);

    QObject::connect(m_playerController, &PlayerController::currentTrackChanged, this,
                     &GuiApplication::updateWindowTitle);
    QObject::connect(m_selectionController.get(), &TrackSelectionController::actionExecuted, m_playlistController.get(),
                     &PlaylistController::handleTrackSelectionAction);
    QObject::connect(m_selectionController.get(), &TrackSelectionController::requestPropertiesDialog, this,
                     &GuiApplication::showPropertiesDialog);
    QObject::connect(
        m_selectionController.get(), &TrackSelectionController::requestArtworkSearch, this,
        [this](const TrackList& tracks, bool quick) { this->searchForArtwork(tracks, Track::Cover::Front, quick); });
    QObject::connect(m_selectionController.get(), &TrackSelectionController::requestArtworkAttach, this,
                     &GuiApplication::attachArtwork);
    QObject::connect(m_selectionController.get(), &TrackSelectionController::requestArtworkRemoval, this,
                     &GuiApplication::removeAllArtwork);
    QObject::connect(m_fileMenu, &FileMenu::requestNewPlaylist, this, &GuiApplication::createNewPlaylist);
    QObject::connect(m_fileMenu, &FileMenu::requestNewAutoPlaylist, this, &GuiApplication::createNewAutoPlaylist);
    QObject::connect(m_fileMenu, &FileMenu::requestExit, this, &GuiApplication::close);
    QObject::connect(m_fileMenu, &FileMenu::requestAddFiles, this, &GuiApplication::addFiles);
    QObject::connect(m_fileMenu, &FileMenu::requestAddFolders, this, &GuiApplication::addFolders);
    QObject::connect(m_fileMenu, &FileMenu::requestAddStreamUrl, this, &GuiApplication::addStreamUrl);
    QObject::connect(m_fileMenu, &FileMenu::requestLoadPlaylist, this, &GuiApplication::loadPlaylist);
    QObject::connect(m_fileMenu, &FileMenu::requestSavePlaylist, this, &GuiApplication::saveCurrentPlaylist);
    QObject::connect(m_fileMenu, &FileMenu::requestSaveAllPlaylists, this, &GuiApplication::saveAllPlaylist);
    QObject::connect(m_editMenu, &EditMenu::requestSearch, this, &GuiApplication::showSearchPlaylistDialog);
    QObject::connect(m_libraryMenu, &LibraryMenu::requestSearch, this, &GuiApplication::showSearchLibraryDialog);
    QObject::connect(m_libraryMenu, &LibraryMenu::requestQuickSearch, this, &GuiApplication::showQuickSearch);
    QObject::connect(m_viewMenu, &ViewMenu::openQuickSetup, m_editableLayout.get(), &EditableLayout::showQuickSetup);
    QObject::connect(m_viewMenu, &ViewMenu::openPlaybackQueue, this, &GuiApplication::showPlaybackQueue);
    QObject::connect(m_viewMenu, &ViewMenu::openPlaylistManager, this, &GuiApplication::showPlaylistManager);
    QObject::connect(m_viewMenu, &ViewMenu::focusSearchBar, this, &GuiApplication::focusSearchBar);
    QObject::connect(m_viewMenu, &ViewMenu::openLog, m_logWidget.get(), &LogWidget::show);
    QObject::connect(m_viewMenu, &ViewMenu::openScriptEditor, this, &GuiApplication::showScriptEditor);
    QObject::connect(m_viewMenu, &ViewMenu::showNowPlaying, this, [this]() {
        if(auto* activePlaylist = m_playlistHandler->activePlaylist()) {
            m_playlistController->uiController()->showNowPlaying();
            m_playlistController->changeCurrentPlaylist(activePlaylist);
        }
    });
    QObject::connect(m_layoutMenu, &LayoutMenu::changeLayout, m_editableLayout.get(), &EditableLayout::changeLayout);
    QObject::connect(m_layoutMenu, &LayoutMenu::clearLayout, this, [this] {
        const QString name = m_layoutProvider->currentLayout().name();
        QJsonObject layout;
        layout["Name"_L1] = name.isEmpty() ? u"Default"_s : name;
        m_editableLayout->changeLayout(FyLayout{layout.value("Name"_L1).toString(), layout});
    });
    QObject::connect(m_layoutMenu, &LayoutMenu::importLayout, this,
                     [this]() { m_layoutProvider->importLayout(m_mainWindow.get()); });
    QObject::connect(m_layoutMenu, &LayoutMenu::exportLayout, this,
                     [this]() { m_editableLayout->exportLayout(m_editableLayout.get()); });

    QObject::connect(m_layoutProvider.get(), &LayoutProvider::requestChangeLayout, m_editableLayout.get(),
                     &EditableLayout::changeLayout);
    QObject::connect(m_editableLayout.get(), &EditableLayout::layoutChanged, m_searchController,
                     &SearchController::loadWidgets);

    QObject::connect(m_core->engine(), &EngineController::engineError, this, &GuiApplication::showEngineError);
    QObject::connect(m_core->engine(), &EngineController::trackStatusContextChanged, this,
                     &GuiApplication::handleTrackStatus);

    m_settings->subscribe<Settings::Gui::LayoutEditing>(this, &GuiApplication::updateWindowTitle);

    m_settings->subscribe<Settings::Gui::IconTheme>(this, [this]() {
        setIconTheme();
        QPixmapCache::clear();
        refreshThemeIcons();
    });
    m_settings->subscribe<Settings::Gui::CustomTheme>(this, [this]() {
        scheduleThemeUpdate();
        if(setIconTheme()) {
            QPixmapCache::clear();
            m_settings->refresh<Settings::Gui::IconTheme>();
        }
    });
    m_settings->subscribe<Settings::Gui::Style>(this, [this]() {
        setStyle();
        scheduleThemeUpdate(true);
        if(setIconTheme()) {
            QPixmapCache::clear();
            m_settings->refresh<Settings::Gui::IconTheme>();
        }
    });

    m_settings->subscribe<Settings::Gui::Internal::ImageAllocationLimit>(m_settings, &QImageReader::setAllocationLimit);
}

void GuiApplication::initialisePlugins()
{
    if(m_core->pluginManager()->allPluginInfo().empty()) {
        QMetaObject::invokeMethod(this, [this]() { showPluginsNotFoundMessage(); }, Qt::QueuedConnection);
        return;
    }

    m_core->pluginManager()->initialisePlugins<GuiPlugin>(
        [this](GuiPlugin* plugin) { plugin->initialise(m_guiPluginContext); });

    m_core->pluginManager()->initialisePlugins<DspGuiPlugin>([this](DspGuiPlugin* plugin) {
        auto providers = plugin->settingsProviders();
        for(auto& provider : providers) {
            if(provider && m_widgets->dspSettingsRegistry()) {
                m_widgets->dspSettingsRegistry()->registerProvider(std::move(provider));
            }
        }
    });

    m_core->pluginManager()->initialisePlugins<PluginConfigGuiPlugin>([this](PluginConfigGuiPlugin* plugin) {
        auto provider          = plugin->settingsProvider();
        auto* root             = dynamic_cast<QObject*>(plugin);
        const QString pluginId = pluginIdentifierForRoot(*m_core->pluginManager(), root);
        if(provider && m_widgets->pluginSettingsRegistry() && !pluginId.isEmpty()) {
            m_widgets->pluginSettingsRegistry()->registerProvider(pluginId, std::move(provider));
        }
    });
}

void GuiApplication::showPluginsNotFoundMessage()
{
    QMessageBox message;
    message.setIcon(QMessageBox::Warning);
    message.setText(tr("Plugins not found"));
    message.setInformativeText(tr("Some plugins are required for full functionality."));
    message.setDetailedText(tr("Plugin search locations:\n\n") + Core::pluginPaths().join(u"\n"_s));

    message.addButton(QMessageBox::Ok);
    QPushButton* quitButton = message.addButton(tr("Quit"), QMessageBox::ActionRole);
    quitButton->setIcon(Gui::iconFromTheme(Constants::Icons::Quit));
    message.setDefaultButton(QMessageBox::Ok);

    message.exec();

    if(message.clickedButton() == quitButton) {
        Application::quit();
    }
}

void GuiApplication::initialiseTray()
{
    m_trayIcon = std::make_unique<SystemTrayIcon>(m_actionManager);

    QObject::connect(m_trayIcon.get(), &SystemTrayIcon::toggleVisibility, m_mainWindow.get(),
                     &MainWindow::toggleVisibility);

    if(m_settings->value<Settings::Gui::Internal::ShowTrayIcon>()) {
        m_trayIcon->show();
    }

    m_settings->subscribe<Settings::Gui::Internal::ShowTrayIcon>(this,
                                                                 [this](bool show) { m_trayIcon->setVisible(show); });
}

void GuiApplication::updateWindowTitle()
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

    auto contextData     = makePlaybackScriptContext(m_playerController, m_playlistHandler->activePlaylist(),
                                                     TrackListContextPolicy::Fallback, {}, false, false,
                                                     Gui::ratingStarSymbols(*m_settings));
    const QString script = m_settings->value<Settings::Gui::Internal::WindowTitleTrackScript>();
    const QString title  = m_scriptParser.evaluate(script, currentTrack, contextData.context);
    m_mainWindow->setTitle(title);
}

void GuiApplication::checkArtwork()
{
    if(m_settings->value<Settings::Gui::Internal::ArtworkAutoSearch>()) {
        if(const auto track = m_playerController->currentTrack(); track.isValid()) {
            m_coverProvider.trackHasCover(track).then([this, track](const bool hasCover) {
                if(!hasCover) {
                    m_widgets->showArtworkDialog({track}, Track::Cover::Front, true);
                }
            });
        }
    }
}

void GuiApplication::handleTrackStatus(const Engine::TrackStatusContext& context)
{
    const Track& track = context.track;

    if(context.status == Engine::TrackStatus::Invalid) {
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
    else if(context.status == Engine::TrackStatus::Unreadable) {
        showTrackUnreableMessage(track);
    }
}

void GuiApplication::handleTracksDeleted(const TrackList& tracks)
{
    const PlaylistTrack currentTrack = m_playerController->currentPlaylistTrack();

    const bool deletingCurrentTrack = currentTrack.isValid()
                                   && m_playerController->playState() == Player::PlayState::Playing
                                   && std::ranges::any_of(tracks, [&currentTrack](const Track& track) {
                                          return sameTrackIdentity(track, currentTrack.track);
                                      });

    if(deletingCurrentTrack) {
        // At this point, the tracks have been removed from playlists,
        // so the next track is now the current track (at the same index)
        const PlaylistTrack nextTrack = m_playlistHandler->currentTrack();
        if(nextTrack.isValid()) {
            m_playerController->changeCurrentTrack(
                nextTrack, {.reason = Player::AdvanceReason::ManualNext, .userInitiated = false});
        }
        else {
            m_playerController->stop();
        }
    }
}

void GuiApplication::removeExpiredCovers(const TrackList& tracks)
{
    for(const Track& track : tracks) {
        if(track.metadataWasModified()) {
            m_coverRepository->removeFromCache(track, *m_settings);
        }
    }
}

void GuiApplication::registerActions()
{
    const QStringList volumeCategory = {tr("Volume")};

    auto* volumeUp = new QAction(tr("Volume up"), m_mainWindow.get());
    Gui::setThemeIcon(volumeUp, Constants::Icons::VolumeHigh);
    auto* volumeUpCmd = m_actionManager->registerAction(volumeUp, Constants::Actions::VolumeUp);
    volumeUpCmd->setCategories(volumeCategory);
    QObject::connect(volumeUp, &QAction::triggered, m_mainWindow.get(),
                     [this]() { changeVolume(m_settings->value<Settings::Gui::VolumeStep>()); });

    auto* volumeDown = new QAction(tr("Volume down"), m_mainWindow.get());
    Gui::setThemeIcon(volumeDown, Constants::Icons::VolumeLow);
    auto* volumeDownCmd = m_actionManager->registerAction(volumeDown, Constants::Actions::VolumeDown);
    volumeDownCmd->setCategories(volumeCategory);
    QObject::connect(volumeDown, &QAction::triggered, m_mainWindow.get(),
                     [this]() { changeVolume(-m_settings->value<Settings::Gui::VolumeStep>()); });

    auto* muteAction = new QAction(tr("Mute"), m_mainWindow.get());
    Gui::setThemeIcon(muteAction, Constants::Icons::VolumeMute);
    muteAction->setCheckable(true);
    muteAction->setChecked(m_settings->value<Settings::Core::OutputVolume>() <= 0.0);
    auto* muteCmd = m_actionManager->registerAction(muteAction, Constants::Actions::Mute);
    muteCmd->setCategories(volumeCategory);
    QObject::connect(muteAction, &QAction::triggered, m_mainWindow.get(), [this]() { mute(); });
    m_settings->subscribe<Settings::Core::OutputVolume>(
        muteAction, [muteAction](double volume) { muteAction->setChecked(volume <= 0.0); });

    auto* clearPlaylistAction = new QAction(tr("Clear Current Playlist"), m_mainWindow.get());
    clearPlaylistAction->setStatusTip(tr("Remove all tracks from the current playlist"));
    Gui::setThemeIcon(clearPlaylistAction, Constants::Icons::Clear);
    auto* clearPlaylistCmd = m_actionManager->registerAction(clearPlaylistAction, Constants::Actions::ClearPlaylist);
    clearPlaylistCmd->setCategories({tr("Playlist")});
    clearPlaylistCmd->setAttribute(ProxyAction::UpdateText);
    QObject::connect(clearPlaylistAction, &QAction::triggered, m_mainWindow.get(),
                     [this]() { m_playlistController->clearCurrentPlaylist(); });

    auto updateClearPlaylistState = [this, clearPlaylistAction]() {
        clearPlaylistAction->setEnabled(m_playlistController->canClearCurrentPlaylist());
    };
    QObject::connect(m_playlistController.get(), &PlaylistController::playlistsLoaded, m_mainWindow.get(),
                     updateClearPlaylistState);
    QObject::connect(m_playlistController.get(), &PlaylistController::currentPlaylistChanged, m_mainWindow.get(),
                     updateClearPlaylistState);
    QObject::connect(m_playlistController.get(), &PlaylistController::currentPlaylistUpdated, m_mainWindow.get(),
                     updateClearPlaylistState);
    QObject::connect(m_playlistController.get(), &PlaylistController::currentPlaylistTracksPatched, m_mainWindow.get(),
                     updateClearPlaylistState);
    QObject::connect(m_playlistController.get(), &PlaylistController::currentPlaylistTracksChanged, m_mainWindow.get(),
                     updateClearPlaylistState);
    QObject::connect(m_playlistController.get(), &PlaylistController::currentPlaylistTracksRemoved, m_mainWindow.get(),
                     updateClearPlaylistState);
    updateClearPlaylistState();

    const QStringList seekCategory = {tr("Playback"), tr("Seek")};

    auto* seekForwardSmall    = new QAction(tr("Seek forward (small step)"), m_mainWindow.get());
    auto* seekForwardSmallCmd = m_actionManager->registerAction(seekForwardSmall, Constants::Actions::SeekForwardSmall);
    seekForwardSmallCmd->setCategories(seekCategory);
    seekForwardSmallCmd->setDefaultShortcut(QKeySequence{Qt::ALT | Qt::Key_Right});
    QObject::connect(seekForwardSmall, &QAction::triggered, m_mainWindow.get(),
                     [this]() { m_playerController->seekForward(m_settings->value<Settings::Gui::SeekStepSmall>()); });

    auto* seekForwardLarge    = new QAction(tr("Seek forward (large step)"), m_mainWindow.get());
    auto* seekForwardLargeCmd = m_actionManager->registerAction(seekForwardLarge, Constants::Actions::SeekForwardLarge);
    seekForwardLargeCmd->setCategories(seekCategory);
    seekForwardLargeCmd->setDefaultShortcut(QKeySequence{Qt::CTRL | Qt::Key_Right});
    QObject::connect(seekForwardLarge, &QAction::triggered, m_mainWindow.get(),
                     [this]() { m_playerController->seekForward(m_settings->value<Settings::Gui::SeekStepLarge>()); });

    auto* seekBackwardSmall = new QAction(tr("Seek backward (small step)"), m_mainWindow.get());
    auto* seekBackwardSmallCmd
        = m_actionManager->registerAction(seekBackwardSmall, Constants::Actions::SeekBackwardSmall);
    seekBackwardSmallCmd->setCategories(seekCategory);
    seekBackwardSmallCmd->setDefaultShortcut(QKeySequence{Qt::ALT | Qt::Key_Left});
    QObject::connect(seekBackwardSmall, &QAction::triggered, m_mainWindow.get(),
                     [this]() { m_playerController->seekBackward(m_settings->value<Settings::Gui::SeekStepSmall>()); });

    auto* seekBackwardLarge = new QAction(tr("Seek backward (large step)"), m_mainWindow.get());
    auto* seekBackwardLargeCmd
        = m_actionManager->registerAction(seekBackwardLarge, Constants::Actions::SeekBackwardLarge);
    seekBackwardLargeCmd->setCategories(seekCategory);
    seekBackwardLargeCmd->setDefaultShortcut(QKeySequence{Qt::CTRL | Qt::Key_Left});
    QObject::connect(seekBackwardLarge, &QAction::triggered, m_mainWindow.get(),
                     [this]() { m_playerController->seekBackward(m_settings->value<Settings::Gui::SeekStepLarge>()); });

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

    auto* removePlaylist = new QAction(tr("Remove Playlist"), m_mainWindow.get());
    auto* removeCmd      = m_actionManager->registerAction(removePlaylist, Constants::Actions::RemovePlaylist);
    removeCmd->setCategories({tr("Playlist")});
    removeCmd->setDescription(tr("Remove Current Playlist"));
    removeCmd->setDefaultShortcut(QKeySequence{Qt::CTRL | Qt::Key_W});
    QObject::connect(removePlaylist, &QAction::triggered, m_mainWindow.get(), [this]() {
        if(auto* currentPlaylist = m_playlistController->currentPlaylist()) {
            m_core->playlistHandler()->removePlaylist(currentPlaylist->id());
        }
    });

    auto* toggleMenubar = new QAction(tr("Toggle Menubar"), m_mainWindow.get());
    toggleMenubar->setCheckable(true);
    toggleMenubar->setChecked(m_settings->value<Settings::Gui::ShowMenuBar>());
    auto* toggleMenubarCmd = m_actionManager->registerAction(toggleMenubar, Constants::Actions::ToggleMenubar);
    toggleMenubarCmd->setCategories({tr("View")});
    toggleMenubarCmd->setDefaultShortcut(QKeySequence{Qt::CTRL | Qt::Key_M});
    QObject::connect(toggleMenubar, &QAction::triggered, m_mainWindow.get(),
                     [this](bool visible) { m_settings->set<Settings::Gui::ShowMenuBar>(visible); });
    m_settings->subscribe<Settings::Gui::ShowMenuBar>(
        toggleMenubar, [toggleMenubar](bool visible) { toggleMenubar->setChecked(visible); });
}

void GuiApplication::rescanTracks(const TrackList& tracks, bool onlyModified) const
{
    auto* scanDialog = new ElapsedProgressDialog(tr("Reading tracks…"), tr("Abort"), 0, 100, Utils::getMainWindow());
    scanDialog->setAttribute(Qt::WA_DeleteOnClose);
    scanDialog->setModal(true);
    scanDialog->setMinimumDuration(500ms);

    auto* library = m_core->library();

    const ScanRequest request = onlyModified ? library->scanModifiedTracks(tracks) : library->scanTracks(tracks);
    scanDialog->startTimer();

    QObject::connect(library, &MusicLibrary::scanProgress, scanDialog,
                     [scanDialog, request](const ScanProgress& progress) {
                         if(progress.id != request.id) {
                             return;
                         }

                         if(scanDialog->wasCancelled()) {
                             request.cancel();
                             scanDialog->close();
                         }

                         const bool isIndeterminate = progress.total <= 0;
                         scanDialog->setBusy(isIndeterminate);

                         if(!isIndeterminate) {
                             scanDialog->setValue(progress.percentage());
                         }
                         if(!progress.file.isEmpty()) {
                             scanDialog->setText(tr("Current file") + ":\n"_L1 + progress.file);
                         }

                         if(progress.phase == ScanProgress::Phase::Finished) {
                             scanDialog->close();
                         }
                     });
}

void GuiApplication::setupScanMenu()
{
    m_selectionController->registerTrackContextSubmenu(
        this, TrackContextMenuArea::Track, Constants::Menus::Context::TrackSelection,
        Constants::Menus::Context::Tagging, tr("Tagging"), Constants::Menus::Context::TrackFinalSeparator);

    auto* rescanAction        = new QAction(tr("Reload tags from files"), m_mainWindow.get());
    auto* rescanChangedAction = new QAction(tr("Reload tags from modified files"), m_mainWindow.get());

    rescanAction->setStatusTip(tr("Replace tags in selected tracks with tags from the files"));
    rescanChangedAction->setStatusTip(tr("Replace tags in selected tracks with tags from the files if modified"));

    auto rescan = [this](const bool onlyModified) {
        if(m_selectionController->hasTracks()) {
            const auto tracks = m_selectionController->selectedTracks();
            rescanTracks(tracks, onlyModified);
        }
    };

    QObject::connect(rescanAction, &QAction::triggered, m_mainWindow.get(), [rescan]() { rescan(false); });
    QObject::connect(rescanChangedAction, &QAction::triggered, m_mainWindow.get(), [rescan]() { rescan(true); });

    m_selectionController->registerTrackContextAction(
        this, TrackContextMenuArea::Track, Constants::Menus::Context::Tagging, "Tracks.ReloadTags",
        rescanAction->text(), [this, rescanAction](QMenu* menu, const TrackSelection&) {
            rescanAction->setEnabled(m_selectionController->hasTracks());
            menu->addAction(rescanAction);
        });
    m_selectionController->registerTrackContextAction(
        this, TrackContextMenuArea::Track, Constants::Menus::Context::Tagging, "Tracks.ReloadModifiedTags",
        rescanChangedAction->text(), [this, rescanChangedAction](QMenu* menu, const TrackSelection&) {
            rescanChangedAction->setEnabled(m_selectionController->hasTracks());
            menu->addAction(rescanChangedAction);
        });
}

void GuiApplication::setupRatingMenu()
{
    m_selectionController->registerTrackContextSubmenu(
        this, TrackContextMenuArea::Track, Constants::Menus::Context::TrackSelection,
        Constants::Menus::Context::Tagging, tr("Tagging"), Constants::Menus::Context::TrackFinalSeparator);
}

void GuiApplication::setupUtilitiesMenu()
{
    m_selectionController->registerTrackContextSubmenu(
        this, TrackContextMenuArea::Track, Constants::Menus::Context::TrackSelection,
        Constants::Menus::Context::Utilities, tr("Utilities"), Constants::Menus::Context::TrackFinalSeparator);
}

void GuiApplication::close()
{
    if(m_mainWindow->isVisible()) {
        m_mainWindow->close();
    }
    else {
        m_mainWindow->exit();
    }
}

void GuiApplication::changeVolume(double delta) const
{
    const double currentVolume = std::max(m_settings->value<Settings::Core::OutputVolume>(), 0.01);
    const double currentDb     = Audio::volumeToDb(currentVolume);
    const double newVolume     = Audio::dbToVolume(currentDb + delta);

    m_settings->set<Settings::Core::OutputVolume>(newVolume);
}

void GuiApplication::mute() const
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

void GuiApplication::setStyle() const
{
    const auto currStyle = m_settings->value<Settings::Gui::Style>();
    if(auto* style = QStyleFactory::create(currStyle)) {
        QApplication::setStyle(style);
    }
    else if(auto* systemStyle = QStyleFactory::create(m_settings->value<Settings::Gui::Internal::SystemStyle>())) {
        QApplication::setStyle(systemStyle);
    }
}

void GuiApplication::scheduleThemeUpdate(bool refreshSystemBaseline)
{
    m_refreshSystemBaseline = m_refreshSystemBaseline || refreshSystemBaseline;

    if(m_applyingTheme) {
        m_themeUpdatePending = true;
        return;
    }

    m_themeUpdateTimer.start(ThemeUpdateDelayMs, this);
}

void GuiApplication::applyTheme()
{
    if(std::exchange(m_applyingTheme, true)) {
        m_themeUpdatePending = true;
        return;
    }

    const bool refreshSystemBaseline = std::exchange(m_refreshSystemBaseline, false);
    if(refreshSystemBaseline) {
        QApplication::setFont(QFont{});
        QApplication::setPalette(QPalette{});
    }
    else {
        QApplication::setFont(m_settings->value<Settings::Gui::Internal::SystemFont>().value<QFont>());
        QApplication::setPalette(m_settings->value<Settings::Gui::Internal::SystemPalette>().value<QPalette>());
    }

    QTimer::singleShot(ThemeUpdateDelayMs, this, [this]() {
        const auto systemFont = QApplication::font();
        m_settings->set<Settings::Gui::Internal::SystemFont>(systemFont);

        const auto fontEntries = m_themeRegistry->fontEntries();
        for(const auto& className : fontEntries | std::views::keys) {
            if(!className.isEmpty()) {
                QApplication::setFont(systemFont, className.toUtf8().constData());
            }
        }

        const auto currTheme = m_settings->value<Settings::Gui::CustomTheme>().value<FyTheme>();
        for(const auto& [className, font] : Utils::asRange(currTheme.fonts)) {
            if(className.isEmpty()) {
                QApplication::setFont(font);
            }
            else {
                QApplication::setFont(font, className.toUtf8().constData());
            }
        }

        const auto systemPalette = QApplication::palette();
        m_settings->set<Settings::Gui::Internal::SystemPalette>(systemPalette);

        auto newPalette{systemPalette};
        for(const auto& [key, colour] : Utils::asRange(currTheme.colours)) {
            newPalette.setColor(key.group, key.role, colour);
        }

        QApplication::setPalette(newPalette);
        QPixmapCache::clear();

        QTimer::singleShot(0, this, [this, newPalette]() {
            ResolvedAppStyle resolvedStyle;
            resolvedStyle.palette     = newPalette;
            resolvedStyle.defaultFont = QApplication::font();
            resolvedStyle.revision    = ++m_resolvedAppStyleRevision;

            const auto styleFonts = m_themeRegistry->fontEntries();
            for(const auto& className : styleFonts | std::views::keys) {
                if(!className.isEmpty()) {
                    resolvedStyle.classFonts.insert(className, QApplication::font(className.toUtf8().constData()));
                }
            }

            m_settings->set<Settings::Gui::ResolvedAppStyle>(QVariant::fromValue(resolvedStyle));
            m_applyingTheme = false;

            if(std::exchange(m_themeUpdatePending, false)) {
                scheduleThemeUpdate();
            }
        });
    });
}

void GuiApplication::handleSystemThemeChanged()
{
    if(m_applyingTheme) {
        return;
    }

    scheduleThemeUpdate(true);
}

bool GuiApplication::setIconTheme() const
{
    const auto [primaryTheme, fallbackTheme] = resolveIconThemes(m_settings);
    return Gui::setThemeIconOverrides(primaryTheme, fallbackTheme);
}

void GuiApplication::refreshThemeIcons()
{
    Gui::refreshThemeIcons(m_actionManager);

    for(const Command* command : m_actionManager->commands()) {
        Gui::refreshThemeIcon(command ? command->action() : nullptr);
    }

    Gui::refreshThemeIcons(this);

    const auto widgets = QApplication::topLevelWidgets();
    for(QWidget* widget : widgets) {
        Gui::refreshThemeIcons(widget);
    }
}

void GuiApplication::refreshAutoDetectedIconTheme() const
{
    if(static_cast<IconThemeOption>(m_settings->value<Settings::Gui::IconTheme>()) != IconThemeOption::AutoDetect) {
        return;
    }

    if(!setIconTheme()) {
        return;
    }

    QPixmapCache::clear();
    m_settings->refresh<Settings::Gui::IconTheme>();
}

void GuiApplication::registerLayouts()
{
    m_layoutProvider->registerLayout(
        R"({"Name":"Simple","Widgets":[{"SplitterVertical":{"State":"AAAA/wAAAAEAAAADAAAAHAAAAn0AAAAXAP////8BAAAAAgA=",
            "Widgets":[{"SplitterHorizontal":{"State":"AAAA/wAAAAEAAAAEAAAAggAABqEAAAA+AAAAHAD/////AQAAAAEA","Widgets":[
            {"PlayerControls":{}},{"SeekBar":{}},{"PlaylistControls":{}},{"VolumeControls":{}}]}},{"SplitterHorizontal":{
            "State":"AAAA/wAAAAEAAAACAAAA9QAABAoA/////wEAAAABAA==","Widgets":[{"SplitterVertical":{
            "State":"AAAA/wAAAAEAAAACAAABfQAAAPEA/////wEAAAACAA==","Widgets":[{"LibraryTree":{}},{"ArtworkPanel":{}}]}},
            {"PlaylistTabs":{"Widgets":[{"Playlist":{}}]}}]}},{"StatusBar":{}}]}}]})");

    m_layoutProvider->registerLayout(
        R"({"Name":"Vision","Widgets":[{"SplitterVertical":{"State":"AAAA/wAAAAEAAAADAAAAHAAAA6EAAAAWAP////8BAAAAAgA=",
            "Widgets":[{"SplitterHorizontal":{"State":"AAAA/wAAAAEAAAAEAAAAiQAABk8AAABHAAAAIwD/////AQAAAAEA",
            "Widgets":[{"PlayerControls":{}},{"SeekBar":{}},{"PlaylistControls":{}},{"VolumeControls":{}}]}},
            {"SplitterHorizontal":{"State":"AAAA/wAAAAEAAAACAAADuwAAA48A/////wEAAAABAA==","Widgets":[
            {"TabStack":{"Position":"West","State":"Artwork\u001fInfo\u001fLibrary Tree\u001fPlaylist Organiser",
            "Widgets":[{"ArtworkPanel":{}},{"SelectionInfo":{}},{"LibraryTree":{"Grouping":"Artist/Album"}},
            {"PlaylistOrganiser":{}}]}},{"Playlist":{}}]}},{"StatusBar":{}}]}}]})");

    m_layoutProvider->registerLayout(
        R"({"Name":"Browser","Widgets":[{"SplitterVertical":{"State":"AAAA/wAAAAEAAAADAAAAFwAAA6YAAAAWAP////8BAAAAAgA=",
            "Widgets":[{"SplitterHorizontal":{"State":"AAAA/wAAAAEAAAAEAAAAcgAABRoAAAA2AAAAGAD/////AQAAAAEA",
            "Widgets":[{"PlayerControls":{}},{"SeekBar":{}},{"PlaylistControls":{}},{"VolumeControls":{}}]}},
            {"SplitterHorizontal":{"State":"AAAA/wAAAAEAAAACAAACeAAAAnoA/////wEAAAABAA==x","Widgets":[{"DirectoryBrowser":{}},
            {"ArtworkPanel":{}}]}},{"StatusBar":{}}]}}]})");
}

void GuiApplication::checkTracksNeedUpdate() const
{
    const auto libraryOutOfDate = [this](int threshold) -> bool {
        const int prev = m_core->database()->previousRevision();
        const int curr = m_core->database()->currentRevision();
        return prev > 0 && prev < threshold && curr >= threshold;
    };

    if(m_library->hasLibrary()) {
        if(libraryOutOfDate(7)     /* changed codec storage type */
           || libraryOutOfDate(11) /* removed ReplayGain data in extra tags */
           || libraryOutOfDate(13) /* added more tech fields */
           || libraryOutOfDate(17) /* added file creation date */) {
            showNeedReloadMessage();
        }
    }
}

void GuiApplication::showNeedReloadMessage() const
{
    QMessageBox message;
    message.setIcon(QMessageBox::Warning);
    message.setText(tr("Reload Required"));
    message.setInformativeText(
        tr("Due to a database change, tracks should be reloaded from disk to update their saved metadata."));

    message.addButton(QMessageBox::Ok);
    if(auto* button = message.button(QMessageBox::Ok)) {
        button->setText(tr("Reload Now"));
    }

    const QPushButton* okButton = message.addButton(tr("OK"), QMessageBox::ActionRole);
    message.setDefaultButton(QMessageBox::Ok);

    message.exec();

    if(message.clickedButton() != okButton) {
        m_library->rescanAll();
    }
}

void GuiApplication::showScriptEditor()
{
    auto* scriptEditor = new ScriptEditor(m_core->libraryManager(), m_selectionController.get(), m_playerController,
                                          m_mainWindow.get());
    scriptEditor->setAttribute(Qt::WA_DeleteOnClose);
    scriptEditor->show();
}

void GuiApplication::showSearchPlaylistDialog()
{
    if(m_settings->value<Settings::Gui::PlaylistIntegratedSearch>() && focusIntegratedPlaylistSearch()) {
        return;
    }

    auto* coverProvider = new CoverProvider(m_coverRepository, this);
    auto* search = new SearchDialog(m_actionManager, &m_playlistInteractor, coverProvider, m_core, m_styleProvider,
                                    m_selectionController.get(), SearchDialog::Target::Playlist);
    search->setAttribute(Qt::WA_DeleteOnClose);
    coverProvider->setParent(search);

    search->show();
}

void GuiApplication::showSearchLibraryDialog()
{
    auto* coverProvider = new CoverProvider(m_coverRepository, this);
    auto* search = new SearchDialog(m_actionManager, &m_playlistInteractor, coverProvider, m_core, m_styleProvider,
                                    m_selectionController.get(), SearchDialog::Target::Library);
    search->setAttribute(Qt::WA_DeleteOnClose);
    coverProvider->setParent(search);

    search->show();
}

void GuiApplication::showPlaylistManager()
{
    if(m_playlistManagerWidget) {
        m_playlistManagerWidget->show();
        m_playlistManagerWidget->raise();
        m_playlistManagerWidget->activateWindow();
        return;
    }

    m_playlistManagerWidget = new PlaylistManagerWidget(m_actionManager, m_playlistController.get(),
                                                        &m_playlistInteractor, m_selectionController.get(), m_settings);
    m_playlistManagerWidget->setAttribute(Qt::WA_DeleteOnClose);
    m_playlistManagerWidget->finalise();

    m_playlistManagerWidget->show();
}

bool GuiApplication::focusIntegratedPlaylistSearch() const
{
    const auto playlists = m_editableLayout->findWidgetsByType<PlaylistWidget>();
    for(auto* playlist : playlists) {
        if(playlist && playlist->isVisibleTo(m_mainWindow.get()) && playlist->openIntegratedSearch()) {
            return true;
        }
    }

    return false;
}

void GuiApplication::showPlaybackQueue()
{
    if(m_playbackQueueWidget) {
        m_playbackQueueWidget->show();
        m_playbackQueueWidget->raise();
        m_playbackQueueWidget->activateWindow();
        return;
    }

    m_playbackQueueWidget = new QueueViewer(m_actionManager, &m_playlistInteractor, m_selectionController.get(),
                                            m_coverRepository, m_settings);
    m_playbackQueueWidget->setAttribute(Qt::WA_DeleteOnClose);
    m_playbackQueueWidget->finalise();

    m_playbackQueueWidget->show();
}

void GuiApplication::focusSearchBar() const
{
    const auto searchBars = m_editableLayout->findWidgetsByType<SearchWidget>();
    for(auto* searchBar : searchBars) {
        if(searchBar && searchBar->isVisibleTo(m_mainWindow.get())) {
            searchBar->setFocus(Qt::ShortcutFocusReason);
            return;
        }
    }
}

void GuiApplication::showQuickSearch() const
{
    auto* searchBar = new SearchWidget(m_searchController, m_playlistController.get(), m_library, m_settings);
    searchBar->setAttribute(Qt::WA_DeleteOnClose);
    searchBar->setWindowTitle(SearchWidget::tr("Quick Search"));
    searchBar->show();
}

void GuiApplication::showPropertiesDialog(const TrackList& tracks) const
{
    if(!tracks.empty()) {
        m_propertiesDialog->show(tracks);
    }
}

void GuiApplication::showEngineError(const QString& error) const
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

void GuiApplication::showMessage(const QString& title, const Track& track) const
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
        button->setText(tr("Continue"));
    }
    QPushButton* stopButton = message.addButton(tr("Stop"), QMessageBox::ActionRole);
    stopButton->setIcon(Gui::iconFromTheme(Constants::Icons::Stop));
    message.setDefaultButton(QMessageBox::Ok);

    auto* alwaysSkip = new QCheckBox(tr("Always continue playing if a track is unavailable"), &message);
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

void GuiApplication::showTrackNotFoundMessage(const Track& track) const
{
    showMessage(tr("Track Not Found"), track);
}

void GuiApplication::showTrackUnreableMessage(const Track& track) const
{
    showMessage(tr("No Decoder Available"), track);
}

void GuiApplication::createNewPlaylist() const
{
    if(auto* playlist = m_playlistHandler->createEmptyPlaylist()) {
        m_playlistController->changeCurrentPlaylist(playlist);
    }
}

void GuiApplication::createNewAutoPlaylist()
{
    auto* autoDialog = new AutoPlaylistDialog(m_mainWindow.get());
    autoDialog->setAttribute(Qt::WA_DeleteOnClose);
    QObject::connect(autoDialog, &AutoPlaylistDialog::playlistEdited, autoDialog,
                     [this](const QString& name, const QString& query, const QString& sortQuery, bool forceSorted) {
                         if(auto* playlist
                            = m_playlistHandler->createNewAutoPlaylist(name, query, sortQuery, forceSorted)) {
                             m_playlistController->changeCurrentPlaylist(playlist);
                         }
                     });
    autoDialog->show();
}

void GuiApplication::addFiles()
{
    const QString audioExtensions
        = Utils::extensionsToWildcards(m_core->audioLoader()->supportedFileExtensions()).join(" "_L1);
    const QString allExtensions = u"%1 %2"_s.arg(audioExtensions, u"*.cue"_s);

    const QString allFilter   = tr("All Supported Media Files (%1)").arg(allExtensions);
    const QString filesFilter = tr("Audio Files (%1)").arg(audioExtensions);
    const QStringList filters{allFilter, filesFilter};

    FyStateSettings stateSettings;

    QUrl dir = QUrl::fromLocalFile(QDir::homePath());
    if(const auto lastPath = stateSettings.value(Settings::Gui::Internal::LastFilePath).toString();
       !lastPath.isEmpty()) {
        dir = lastPath;
    }

    const auto files = QFileDialog::getOpenFileUrls(m_mainWindow.get(), tr("Add Files"), dir, filters.join(";;"_L1),
                                                    nullptr, QFileDialog::DontResolveSymlinks);

    if(files.empty()) {
        return;
    }

    stateSettings.setValue(Settings::Gui::Internal::LastFilePath, files.front());

    m_playlistInteractor.filesToCurrentPlaylist(files);
}

void GuiApplication::addFolders()
{
    const auto dirs = QFileDialog::getExistingDirectoryUrl(m_mainWindow.get(), tr("Add Folders"), QDir::homePath(),
                                                           QFileDialog::DontResolveSymlinks);

    if(dirs.isEmpty()) {
        return;
    }

    m_playlistInteractor.filesToCurrentPlaylist({dirs});
}

void GuiApplication::addStreamUrl()
{
    bool accepted{false};
    const QString input = QInputDialog::getText(m_mainWindow.get(), tr("Add Stream URL"), tr("Stream URL:"),
                                                QLineEdit::Normal, {}, &accepted)
                              .trimmed();

    if(!accepted || input.isEmpty()) {
        return;
    }

    const QUrl url{input, QUrl::StrictMode};
    if(!url.isValid() || !Track::isRemotePath(url.toString())) {
        QMessageBox::warning(m_mainWindow.get(), tr("Invalid Stream URL"),
                             tr("Enter a valid http:// or https:// stream URL."));
        return;
    }

    const QFileInfo urlInfo{url.path()};
    if(m_core->playlistLoader()->parserForExtension(urlInfo.suffix().toLower())) {
        const QString playlistName = !urlInfo.completeBaseName().isEmpty() ? urlInfo.completeBaseName() : url.host();
        m_playlistInteractor.loadPlaylist({{playlistName, url}});
        return;
    }

    m_playlistInteractor.tracksToCurrentPlaylist({Track{url.toString()}});
}

void GuiApplication::openFilesNow(const QList<QUrl>& urls)
{
    QList<QUrl> files{urls};
    QUrl fileToPlay;

    if(m_settings->value<Settings::Core::OpenFileAddDirectory>() && urls.size() == 1 && urls.front().isLocalFile()) {
        const QFileInfo fileInfo{urls.front().toLocalFile()};
        if(fileInfo.isFile()) {
            QStringList supportedExtensions
                = Utils::extensionsToWildcards(m_core->audioLoader()->supportedFileExtensions());
            supportedExtensions.append(u"*.cue"_s);

            const QList<QUrl> dirFiles = Utils::File::getUrlsInDir(QDir{fileInfo.absolutePath()}, supportedExtensions);
            if(!dirFiles.empty()) {
                files      = dirFiles;
                fileToPlay = urls.front();
            }
        }
    }

    const QString playlistName = m_settings->value<Settings::Core::OpenFilesPlaylist>();
    const bool replacePlaylist = m_settings->value<Settings::Core::OpenFilesSendTo>();
    if(!fileToPlay.isEmpty()) {
        m_playlistInteractor.filesToNewPlaylist(playlistName, files, fileToPlay, replacePlaylist, true);
    }
    else if(replacePlaylist) {
        m_playlistInteractor.filesToNewPlaylistReplace(playlistName, files, true);
    }
    else {
        m_playlistInteractor.filesToNewPlaylist(playlistName, files, true);
    }
}

void GuiApplication::loadPlaylist()
{
    const QString allExtensions
        = Utils::extensionsToWildcards(m_core->playlistLoader()->supportedExtensions()).join(" "_L1);
    const auto playlistFilter
        = Utils::extensionsToFilterList(m_core->playlistLoader()->supportedExtensions(), u"playlists"_s);

    const QString allFilter = tr("All Supported Playlists (%1)").arg(allExtensions);

    const QStringList filters{allFilter, playlistFilter};

    QUrl dir = QUrl::fromLocalFile(QDir::homePath());
    if(const auto lastPath = m_settings->fileValue(Settings::Gui::Internal::LastFilePath).toString();
       !lastPath.isEmpty()) {
        dir = lastPath;
    }

    const auto files = QFileDialog::getOpenFileUrls(m_mainWindow.get(), tr("Load Playlist"), dir, filters.join(";;"_L1),
                                                    nullptr, QFileDialog::DontResolveSymlinks);

    if(files.empty()) {
        return;
    }

    m_settings->fileSet(Settings::Gui::Internal::LastFilePath, files.front());
    const QList<QPair<QString, QUrl>> info{[&files]() {
        QList<QPair<QString, QUrl>> list;
        for(const QUrl& url : files) {
            list.append(qMakePair(QFileInfo(url.toLocalFile()).completeBaseName(), url));
        }
        return list;
    }()};

    m_playlistInteractor.loadPlaylist(info);
}

void GuiApplication::saveCurrentPlaylist() const
{
    auto* playlist = m_playlistController->currentPlaylist();
    savePlaylistToFile(playlist);
}

void GuiApplication::savePlaylistToFile(const Playlist* playlist) const
{
    if(!playlist || playlist->trackCount() == 0) {
        return;
    }

    const QString playlistFilter
        = Utils::extensionsToFilterList(m_core->playlistLoader()->supportedSaveExtensions(), u"files"_s);

    QDir dir{QDir::homePath()};
    if(const auto lastPath = m_settings->fileValue(Settings::Gui::Internal::LastFilePath).toString();
       !lastPath.isEmpty()) {
        dir = lastPath;
    }

    QFileDialog saveDialog{m_mainWindow.get(), tr("Save Playlist"), dir.absolutePath(), playlistFilter};
    saveDialog.setAcceptMode(QFileDialog::AcceptSave);
    saveDialog.setFileMode(QFileDialog::AnyFile);
    saveDialog.setOption(QFileDialog::DontResolveSymlinks);

    const QStringList filters = playlistFilter.split(";;"_L1, Qt::SkipEmptyParts);
    if(!filters.isEmpty()) {
        saveDialog.selectNameFilter(filters.constFirst());
        if(const QString defaultSuffix = Utils::extensionFromFilter(filters.constFirst()); !defaultSuffix.isEmpty()) {
            saveDialog.setDefaultSuffix(defaultSuffix);
            saveDialog.selectFile(playlist->name() + u'.' + defaultSuffix);
        }
        QObject::connect(&saveDialog, &QFileDialog::filterSelected, &saveDialog, [&saveDialog](const QString& filter) {
            if(const QString suffix = Utils::extensionFromFilter(filter); !suffix.isEmpty()) {
                saveDialog.setDefaultSuffix(suffix);
            }
        });
    }
    else {
        saveDialog.selectFile(playlist->name());
    }

    if(saveDialog.exec() == 0) {
        return;
    }

    const QStringList selectedFiles = saveDialog.selectedFiles();
    if(selectedFiles.isEmpty()) {
        return;
    }

    const QString& file = selectedFiles.constFirst();
    if(file.isEmpty()) {
        return;
    }

    FyStateSettings stateSettings;
    stateSettings.setValue(Settings::Gui::Internal::LastFilePath, file);

    const QString extension = Utils::extensionFromFilter(saveDialog.selectedNameFilter());
    if(extension.isEmpty()) {
        return;
    }

    const QFileInfo info{file};
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

void GuiApplication::savePlaylist(const UId& playlistId) const
{
    savePlaylistToFile(m_playlistHandler->playlistById(playlistId));
}

void GuiApplication::saveAllPlaylist() const
{
    auto* savePlaylists = new SavePlaylistsDialog(m_core, m_mainWindow.get());
    savePlaylists->setAttribute(Qt::WA_DeleteOnClose);
    savePlaylists->show();
}

bool GuiApplication::eventFilter(QObject* watched, QEvent* event)
{
    if(watched == qApp) {
        switch(event->type()) {
            case QEvent::ApplicationFontChange:
                handleSystemThemeChanged();
                break;
            case QEvent::ApplicationPaletteChange:
            case QEvent::ThemeChange:
                refreshAutoDetectedIconTheme();
                handleSystemThemeChanged();
                break;
            default:
                break;
        }
    }
    else if(watched == m_mainWindow.get()) {
        switch(event->type()) {
            case QEvent::FontChange:
                handleSystemThemeChanged();
                break;
            case QEvent::PaletteChange:
            case QEvent::StyleChange:
            case QEvent::ThemeChange:
                refreshAutoDetectedIconTheme();
                handleSystemThemeChanged();
                break;
            default:
                break;
        }
    }

    return QObject::eventFilter(watched, event);
}

void GuiApplication::timerEvent(QTimerEvent* event)
{
    if(event->timerId() == m_themeUpdateTimer.timerId()) {
        m_themeUpdateTimer.stop();
        m_themeUpdatePending = false;
        applyTheme();
        return;
    }

    QObject::timerEvent(event);
}

void GuiApplication::startup()
{
    initialise();
}

GuiApplication::~GuiApplication() = default;

void GuiApplication::shutdown()
{
    qApp->removeEventFilter(this);
    m_actionManager->saveSettings();
    m_editableLayout->saveLayout();
    m_editableLayout.reset();
    m_playlistController.reset();
    m_mainWindow.reset();
}

void GuiApplication::raise()
{
    m_mainWindow->raiseWindow();
}

void GuiApplication::openFiles(const QList<QUrl>& files)
{
    if(m_playlistController->playlistsHaveLoaded()) {
        openFilesNow(files);
        return;
    }

    QObject::connect(
        m_playlistController.get(), &PlaylistController::playlistsLoaded, this,
        [this, files]() { openFilesNow(files); }, Qt::SingleShotConnection);
}

void GuiApplication::searchForArtwork(const TrackList& tracks, Track::Cover type, bool quick)
{
    if(!quick) {
        auto* search = new ArtworkDialog(m_core->networkManager(), m_core->library(), m_settings, m_coverRepository,
                                         tracks, type, m_mainWindow.get());
        search->setAttribute(Qt::WA_DeleteOnClose);
        search->show();
        return;
    }

    StatusEvent::post(tr("Searching for artwork…"), 0);

    auto* artworkFinder = new ArtworkFinder(m_core->networkManager(), m_settings, this);

    const Track& track = tracks.front();
    ScriptParser parser;

    const QString artist = parser.evaluate(m_settings->value<Settings::Gui::Internal::ArtworkArtistField>(), track);
    const QString album  = parser.evaluate(m_settings->value<Settings::Gui::Internal::ArtworkAlbumField>(), track);
    const QString title{u"%title%"_s};

    const auto finishSearch = [artworkFinder]() {
        artworkFinder->disconnect();
        artworkFinder->deleteLater();
        StatusEvent::post(tr("Artwork search finished"));
    };

    QObject::connect(
        artworkFinder, &ArtworkFinder::coverLoaded, this,
        [this, finishSearch, tracks](const QUrl& /*url*/, const Fooyin::ArtworkResult& result) {
            finishSearch();

            const auto saveMethods
                = m_settings->value<Settings::Gui::Internal::ArtworkSaveMethods>().value<ArtworkSaveMethods>();
            const auto& frontMethod = saveMethods[Track::Cover::Front];
            const auto saveResult   = prepareArtworkForSave(result, frontMethod.format, frontMethod.quality);

            if(frontMethod.method == ArtworkSaveMethod::Embedded) {
                TrackCoverData coverData;
                coverData.tracks = tracks;
                coverData.coverData.emplace(Track::Cover::Front,
                                            CoverImage{.mimeType = saveResult.mimeType, .data = saveResult.image});
                m_library->writeTrackCovers(coverData).finished.then(
                    this, [this, tracks](const WriteResult& /*result*/) {
                        for(const Track& updatedTrack : tracks) {
                            m_coverRepository->removeFromCache(updatedTrack, *m_settings);
                        }
                    });
            }
            else {
                ScriptParser trackParser;
                const QString path = trackParser.evaluate(
                    u"%1/%2.%3"_s.arg(frontMethod.dir, frontMethod.filename, saveResult.suffix), tracks.front());
                const QString cleanPath = QDir::cleanPath(path);

                QFile file{cleanPath};
                if(file.open(QIODevice::WriteOnly) && file.write(saveResult.image) == saveResult.image.size()) {
                    for(const Track& updatedTrack : tracks) {
                        m_coverRepository->removeFromCache(updatedTrack, *m_settings);
                    }
                    m_widgets->refreshCoverWidgets();
                }
            }
        });

    QObject::connect(artworkFinder, &ArtworkFinder::searchFinished, this, [finishSearch]() { finishSearch(); });

    artworkFinder->findArtwork(Track::Cover::Front, artist, album, title);
}

void GuiApplication::removeArtwork(const TrackList& tracks, const std::set<Track::Cover>& types)
{
    TrackCoverData coverData;
    coverData.tracks = tracks;

    for(const auto& coverType : types) {
        coverData.coverData.emplace(coverType, CoverImage{});
    }

    m_library->writeTrackCovers(coverData).finished.then(this, [this, tracks](const WriteResult& /*result*/) {
        for(const Track& track : tracks) {
            m_coverRepository->removeFromCache(track, *m_settings);
        }
    });
}

void GuiApplication::attachArtwork(const TrackList& tracks, Track::Cover type, const QString& filepath)
{
    if(tracks.empty() || filepath.isEmpty()) {
        return;
    }

    QFile file{filepath};
    if(!file.open(QIODevice::ReadOnly)) {
        StatusEvent::post(tr("Failed to open artwork file"));
        return;
    }

    const QByteArray image = file.readAll();
    if(image.isEmpty()) {
        StatusEvent::post(tr("Artwork file is empty"));
        return;
    }

    const QString mimeType = QMimeDatabase().mimeTypeForData(image).name();

    TrackCoverData coverData;
    coverData.tracks = tracks;
    coverData.coverData.emplace(type, CoverImage{.mimeType = mimeType, .data = image});

    m_library->writeTrackCovers(coverData).finished.then(this, [this, tracks](const WriteResult& /*result*/) {
        for(const Track& track : tracks) {
            m_coverRepository->removeFromCache(track, *m_settings);
        }
    });
}

void GuiApplication::removeAllArtwork(const TrackList& tracks)
{
    removeArtwork(tracks, {Track::Cover::Artist, Track::Cover::Back, Track::Cover::Front, Track::Cover::Other});
}

ActionManager* GuiApplication::actionManager() const
{
    return m_actionManager;
}

LayoutProvider* GuiApplication::layoutProvider() const
{
    return m_layoutProvider.get();
}

PlaylistController* GuiApplication::playlistController() const
{
    return m_playlistController.get();
}

TrackSelectionController* GuiApplication::trackSelection() const
{
    return m_selectionController.get();
}

SearchController* GuiApplication::searchController() const
{
    return m_searchController;
}

PropertiesDialog* GuiApplication::propertiesDialog() const
{
    return m_propertiesDialog;
}

ScriptCommandHandler* GuiApplication::scriptCommandHandler() const
{
    return m_scriptCommandHandler.get();
}

EditableLayout* GuiApplication::editableLayout() const
{
    return m_editableLayout.get();
}

WidgetProvider* GuiApplication::widgetProvider() const
{
    return m_widgetProvider.get();
}

ThemeRegistry* GuiApplication::themeRegistry() const
{
    return m_themeRegistry;
}

GuiStyleProvider* GuiApplication::styleProvider() const
{
    return m_styleProvider;
}

AdvancedSettingsRegistry* GuiApplication::advancedSettingsRegistry() const
{
    return m_advancedSettingsRegistry.get();
}

CoverRepository* GuiApplication::coverRepository() const
{
    return m_coverRepository;
}
} // namespace Fooyin

#include "moc_guiapplication.cpp"
