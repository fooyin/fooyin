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

#pragma once

#include "fygui_export.h"

#include "internalguisettings.h"
#include "playlist/playlistinteractor.h"
#include "widgets.h"

#include <core/engine/enginedefs.h>
#include <core/scripting/scriptparser.h>
#include <core/track.h>
#include <gui/coverprovider.h>
#include <gui/layoutprovider.h>
#include <gui/plugins/guiplugincontext.h>
#include <gui/trackselectioncontroller.h>
#include <gui/widgetprovider.h>

#include <QBasicTimer>
#include <QObject>
#include <QPointer>

#include <memory>
#include <set>

class QTimerEvent;

namespace Fooyin {
class ActionManager;
class AdvancedSettingsRegistry;
class Application;
class CoverRepository;
class EditableLayout;
class GuiStyleProvider;
class EditMenu;
class FileMenu;
class HelpMenu;
class LibraryMenu;
class LogWidget;
class MainMenuBar;
class MainWindow;
class Playlist;
class PlaylistController;
class PlaylistManagerWidget;
class PlaybackMenu;
class PropertiesDialog;
class QueueViewer;
class SearchController;
class ScriptCommandHandler;
class SystemTrayIcon;
class ThemeRegistry;
class UId;
class ViewMenu;
class WindowController;
class WidgetContext;
class LayoutMenu;
class SettingsManager;
class MusicLibrary;
class PlayerController;
class PlaylistHandler;

class FYGUI_EXPORT GuiApplication : public QObject
{
    Q_OBJECT

public:
    explicit GuiApplication(Application* core);
    ~GuiApplication() override;

    void startup();
    void shutdown();

    void raise();
    void openFiles(const QList<QUrl>& files);
    void savePlaylist(const UId& playlistId) const;

    void createNewLayout();

    void searchForArtwork(const TrackList& tracks, Track::Cover type, bool quick);
    void attachArtwork(const TrackList& tracks, Track::Cover type, const QString& filepath);
    void removeArtwork(const TrackList& tracks, const std::set<Track::Cover>& types);
    void removeAllArtwork(const TrackList& tracks);

    [[nodiscard]] ActionManager* actionManager() const;
    [[nodiscard]] LayoutProvider* layoutProvider() const;
    [[nodiscard]] PlaylistController* playlistController() const;
    [[nodiscard]] TrackSelectionController* trackSelection() const;
    [[nodiscard]] SearchController* searchController() const;
    [[nodiscard]] PropertiesDialog* propertiesDialog() const;
    [[nodiscard]] ScriptCommandHandler* scriptCommandHandler() const;
    [[nodiscard]] EditableLayout* editableLayout() const;
    [[nodiscard]] WidgetProvider* widgetProvider() const;
    [[nodiscard]] ThemeRegistry* themeRegistry() const;
    [[nodiscard]] GuiStyleProvider* styleProvider() const;
    [[nodiscard]] AdvancedSettingsRegistry* advancedSettingsRegistry() const;
    [[nodiscard]] CoverRepository* coverRepository() const;

    bool eventFilter(QObject* watched, QEvent* event) override;

protected:
    void timerEvent(QTimerEvent* event) override;

private:
    void initialise();
    void setupConnections();
    void initialisePlugins();
    void showQuickSetup();

    void showPluginsNotFoundMessage();
    void initialiseTray();
    void updateWindowTitle();
    void checkArtwork();
    void handleTrackStatus(const Engine::TrackStatusContext& context);
    void handleTracksDeleted(const TrackList& tracks);

    void removeExpiredCovers(const TrackList& tracks);

    void registerActions();
    void rescanTracks(const TrackList& tracks, bool onlyModified) const;

    void setupScanMenu();
    void setupRatingMenu();
    void setupUtilitiesMenu();

    void close();
    void changeVolume(double delta) const;
    void mute() const;
    void setStyle() const;
    void scheduleThemeUpdate(bool refreshSystemBaseline = false);
    void applyTheme();
    void handleSystemThemeChanged();
    bool setIconTheme() const;
    void refreshThemeIcons();
    void refreshAutoDetectedIconTheme() const;
    void registerLayouts();

    void checkTracksNeedUpdate() const;
    void showNeedReloadMessage() const;

    void showScriptEditor();
    void showSearchPlaylistDialog();
    void showSearchLibraryDialog(const QString& search = {});
    void showPlaybackQueue();
    void showPlaylistManager();
    bool focusIntegratedPlaylistSearch() const;
    void focusSearchBar() const;
    void showQuickSearch() const;
    void showPropertiesDialog(const TrackList& tracks) const;
    void showEngineError(const QString& error) const;
    void showMessage(const QString& title, const Track& track) const;
    void showTrackNotFoundMessage(const Track& track) const;
    void showTrackUnreableMessage(const Track& track) const;

    void createNewPlaylist() const;
    void createNewAutoPlaylist();

    void addFiles();
    void addFolders();
    void addStreamUrl();
    void openFilesNow(const QList<QUrl>& urls);

    void loadPlaylist();
    void saveCurrentPlaylist() const;
    void savePlaylistToFile(const Playlist* playlist) const;
    void saveAllPlaylist() const;

    Application* m_core;

    SettingsManager* m_settings;
    ActionManager* m_actionManager;
    MusicLibrary* m_library;
    PlayerController* m_playerController;
    PlaylistHandler* m_playlistHandler;

    std::unique_ptr<WidgetProvider> m_widgetProvider;
    GuiSettings m_guiSettings;
    std::unique_ptr<LayoutProvider> m_layoutProvider;
    std::unique_ptr<EditableLayout> m_editableLayout;
    std::unique_ptr<MainMenuBar> m_menubar;
    std::unique_ptr<MainWindow> m_mainWindow;
    std::unique_ptr<SystemTrayIcon> m_trayIcon;
    WidgetContext* m_mainContext;
    std::unique_ptr<PlaylistController> m_playlistController;
    PlaylistInteractor m_playlistInteractor;
    std::unique_ptr<TrackSelectionController> m_selectionController;
    SearchController* m_searchController;

    FileMenu* m_fileMenu;
    EditMenu* m_editMenu;
    ViewMenu* m_viewMenu;
    LayoutMenu* m_layoutMenu;
    PlaybackMenu* m_playbackMenu;
    LibraryMenu* m_libraryMenu;
    HelpMenu* m_helpMenu;

    PropertiesDialog* m_propertiesDialog;
    std::unique_ptr<ScriptCommandHandler> m_scriptCommandHandler;
    WindowController* m_windowController;
    ThemeRegistry* m_themeRegistry;
    GuiStyleProvider* m_styleProvider;
    std::unique_ptr<AdvancedSettingsRegistry> m_advancedSettingsRegistry;
    CoverRepository* m_coverRepository;

    GuiPluginContext m_guiPluginContext;

    std::unique_ptr<LogWidget> m_logWidget;
    QPointer<QueueViewer> m_playbackQueueWidget;
    QPointer<PlaylistManagerWidget> m_playlistManagerWidget;
    Widgets* m_widgets;
    ScriptParser m_scriptParser;
    CoverProvider m_coverProvider;

    QBasicTimer m_themeUpdateTimer;
    bool m_themeUpdatePending;
    bool m_refreshSystemBaseline;
    bool m_applyingTheme;
    quint64 m_resolvedAppStyleRevision;
};
} // namespace Fooyin
