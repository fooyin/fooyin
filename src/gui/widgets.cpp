/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <LukeT1@proton.me>
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

#include "widgets.h"

#include "controls/playercontrol.h"
#include "controls/playlistcontrol.h"
#include "controls/seekbar.h"
#include "controls/volumecontrol.h"
#include "dirbrowser/dirbrowser.h"
#include "info/infowidget.h"
#include "librarytree/librarytreecontroller.h"
#include "librarytree/librarytreewidget.h"
#include "mainwindow.h"
#include "playlist/organiser/playlistorganiser.h"
#include "playlist/playlistbox.h"
#include "playlist/playlistcontroller.h"
#include "playlist/playlistinteractor.h"
#include "playlist/playlisttabs.h"
#include "playlist/playlistwidget.h"
#include "queueviewer/queueviewer.h"
#include "search/searchwidget.h"
#include "settings/artworkpage.h"
#include "settings/dirbrowser/dirbrowserpage.h"
#include "settings/generalpage.h"
#include "settings/guigeneralpage.h"
#include "settings/guithemespage.h"
#include "settings/library/librarygeneralpage.h"
#include "settings/library/librarysortingpage.h"
#include "settings/librarytree/librarytreegrouppage.h"
#include "settings/librarytree/librarytreepage.h"
#include "settings/playback/decoderpage.h"
#include "settings/playback/outputpage.h"
#include "settings/playback/playbackpage.h"
#include "settings/playback/replaygainpage.h"
#include "settings/playlist/playlistcolumnpage.h"
#include "settings/playlist/playlistgeneralpage.h"
#include "settings/playlist/playlistpresetspage.h"
#include "settings/plugins/pluginspage.h"
#include "settings/shellintegrationpage.h"
#include "settings/shortcuts/shortcutspage.h"
#include "settings/widgets/playbackqueuepage.h"
#include "settings/widgets/statuswidgetpage.h"
#include "widgets/coverwidget.h"
#include "widgets/dummy.h"
#include "widgets/lyricswidget.h"
#include "widgets/spacer.h"
#include "widgets/splitterwidget.h"
#include "widgets/statuswidget.h"
#include "widgets/tabstackwidget.h"

#include <core/application.h>
#include <core/library/musiclibrary.h>
#include <core/player/playercontroller.h>
#include <core/playlist/playlisthandler.h>
#include <core/playlist/playlistloader.h>
#include <core/plugins/coreplugincontext.h>
#include <gui/coverprovider.h>
#include <gui/plugins/guiplugincontext.h>
#include <gui/theme/themeregistry.h>
#include <gui/widgetprovider.h>
#include <utils/utils.h>

namespace Fooyin {
Widgets::Widgets(Application* core, MainWindow* window, const GuiPluginContext& gui,
                 PlaylistInteractor* playlistInteractor, QObject* parent)
    : QObject{parent}
    , m_core{core}
    , m_gui{gui}
    , m_window{window}
    , m_provider{m_gui.widgetProvider}
    , m_settings{m_core->settingsManager()}
    , m_coverProvider{new CoverProvider(m_core->audioLoader(), m_settings, this)}
    , m_playlistInteractor{playlistInteractor}
    , m_playlistController{playlistInteractor->playlistController()}
    , m_libraryTreeController{new LibraryTreeController(m_settings, this)}
    , m_statusWidget{nullptr}
{ }

void Widgets::registerWidgets()
{
    m_provider->registerWidget(
        QStringLiteral("Dummy"), [this]() { return new Dummy(m_settings, m_window); }, tr("Dummy"));
    m_provider->setIsHidden(QStringLiteral("Dummy"), true);

    m_provider->registerWidget(
        QStringLiteral("SplitterVertical"),
        [this]() { return new VerticalSplitterWidget(m_provider, m_settings, m_window); }, tr("Splitter (Top/Bottom)"));
    m_provider->setSubMenus(QStringLiteral("SplitterVertical"), {tr("Splitters")});

    m_provider->registerWidget(
        QStringLiteral("SplitterHorizontal"),
        [this]() { return new HorizontalSplitterWidget(m_gui.widgetProvider, m_settings, m_window); },
        tr("Splitter (Left/Right)"));
    m_provider->setSubMenus(QStringLiteral("SplitterHorizontal"), {tr("Splitters")});

    m_provider->registerWidget(
        QStringLiteral("PlaylistSwitcher"), [this]() { return new PlaylistBox(m_playlistController, m_window); },
        tr("Playlist Switcher"));

    m_provider->registerWidget(
        QStringLiteral("PlaylistTabs"),
        [this]() {
            auto* playlistTabs = new PlaylistTabs(m_gui.actionManager, m_gui.widgetProvider, m_playlistController,
                                                  m_settings, m_window);
            QObject::connect(playlistTabs, &PlaylistTabs::filesDropped, m_playlistInteractor,
                             &PlaylistInteractor::filesToPlaylist);
            QObject::connect(playlistTabs, &PlaylistTabs::tracksDropped, m_playlistInteractor,
                             &PlaylistInteractor::trackMimeToPlaylist);
            return playlistTabs;
        },
        tr("Playlist Tabs"));
    m_provider->setSubMenus(QStringLiteral("PlaylistTabs"), {tr("Splitters")});

    m_provider->registerWidget(
        QStringLiteral("PlaylistOrganiser"),
        [this]() { return new PlaylistOrganiser(m_gui.actionManager, m_playlistInteractor, m_settings, m_window); },
        tr("Playlist Organiser"));

    m_provider->registerWidget(
        QStringLiteral("PlaybackQueue"),
        [this]() {
            return new QueueViewer(m_gui.actionManager, m_playlistInteractor, m_core->audioLoader(), m_settings,
                                   m_window);
        },
        tr("Playback Queue"));
    m_provider->setLimit(QStringLiteral("PlaybackQueue"), 1);

    m_provider->registerWidget(
        QStringLiteral("TabStack"), [this]() { return new TabStackWidget(m_provider, m_settings, m_window); },
        tr("Tab Stack"));
    m_provider->setSubMenus(QStringLiteral("TabStack"), {tr("Splitters")});

    m_provider->registerWidget(
        QStringLiteral("LibraryTree"),
        [this]() {
            return new LibraryTreeWidget(m_gui.actionManager, m_playlistController, m_libraryTreeController, m_core,
                                         m_window);
        },
        tr("Library Tree"));

    m_provider->registerWidget(
        QStringLiteral("PlayerControls"),
        [this]() { return new PlayerControl(m_gui.actionManager, m_core->playerController(), m_settings, m_window); },
        tr("Player Controls"));
    m_provider->setSubMenus(QStringLiteral("PlayerControls"), {tr("Controls")});

    m_provider->registerWidget(
        QStringLiteral("PlaylistControls"),
        [this]() { return new PlaylistControl(m_core->playerController(), m_settings, m_window); },
        tr("Playlist Controls"));
    m_provider->setSubMenus(QStringLiteral("PlaylistControls"), {tr("Controls")});

    m_provider->registerWidget(
        QStringLiteral("VolumeControls"),
        [this]() { return new VolumeControl(m_gui.actionManager, m_settings, m_window); }, tr("Volume Controls"));
    m_provider->setSubMenus(QStringLiteral("VolumeControls"), {tr("Controls")});

    m_provider->registerWidget(
        QStringLiteral("SeekBar"), [this]() { return new SeekBar(m_core->playerController(), m_settings, m_window); },
        tr("Seekbar"));
    m_provider->setSubMenus(QStringLiteral("SeekBar"), {tr("Controls")});

    m_provider->registerWidget(
        QStringLiteral("SelectionInfo"),
        [this]() { return new InfoWidget(m_core->playerController(), m_gui.trackSelection, m_settings, m_window); },
        tr("Selection Info"));

    m_provider->registerWidget(
        QStringLiteral("ArtworkPanel"),
        [this]() {
            return new CoverWidget(m_core->playerController(), m_gui.trackSelection, m_core->audioLoader(), m_settings,
                                   m_window);
        },
        tr("Artwork Panel"));

    m_provider->registerWidget(
        QStringLiteral("Lyrics"), [this]() { return new LyricsWidget(m_core->playerController(), m_window); },
        tr("Lyrics"));
    m_provider->setLimit(QStringLiteral("Lyrics"), 1);

    m_provider->registerWidget(
        QStringLiteral("Playlist"),
        [this]() {
            return new PlaylistWidget(m_gui.actionManager, m_playlistInteractor, m_coverProvider, m_core, m_window);
        },
        tr("Playlist"));
    m_provider->setLimit(QStringLiteral("Playlist"), 1);

    m_provider->registerWidget(QStringLiteral("Spacer"), [this]() { return new Spacer(m_window); }, tr("Spacer"));

    m_provider->registerWidget(
        QStringLiteral("StatusBar"),
        [this]() {
            m_statusWidget = new StatusWidget(m_core->playerController(), m_gui.trackSelection, m_settings, m_window);
            m_window->installStatusWidget(m_statusWidget);
            QObject::connect(m_core->library(), &MusicLibrary::scanProgress, this, &Widgets::showScanProgress);
            return m_statusWidget;
        },
        tr("Status Bar"));
    m_provider->setLimit(QStringLiteral("StatusBar"), 1);

    m_provider->registerWidget(
        QStringLiteral("SearchBar"),
        [this]() { return new SearchWidget(m_gui.searchController, m_settings, m_window); }, tr("Search Bar"));

    m_provider->registerWidget(
        QStringLiteral("DirectoryBrowser"),
        [this]() {
            auto* browser = new DirBrowser(m_core->audioLoader()->supportedFileExtensions(), m_gui.actionManager,
                                           m_playlistInteractor, m_settings, m_window);
            QObject::connect(m_core->playerController(), &PlayerController::playStateChanged, browser,
                             &DirBrowser::playstateChanged);
            QObject::connect(m_core->playerController(), &PlayerController::playlistTrackChanged, browser,
                             &DirBrowser::playlistTrackChanged);
            QObject::connect(m_core->playlistHandler(), &PlaylistHandler::activePlaylistChanged, browser,
                             &DirBrowser::activePlaylistChanged);
            return browser;
        },
        tr("Directory Browser"));
    m_provider->setLimit(QStringLiteral("DirectoryBrowser"), 1);
}

void Widgets::registerPages()
{
    new GeneralPage(m_settings, this);
    new GuiGeneralPage(m_gui.layoutProvider, m_gui.editableLayout, m_settings, this);
    new GuiThemesPage(m_gui.themeRegistry, m_settings, this);
    new ArtworkPage(m_settings, this);
    new LibraryGeneralPage(m_gui.actionManager, m_core->libraryManager(), m_core->library(), m_settings, this);
    new LibrarySortingPage(m_gui.actionManager, m_core->sortingRegistry(), m_settings, this);
    new PlaybackPage(m_settings, this);
    new PlaylistGeneralPage(m_core->playlistLoader()->supportedSaveExtensions(), m_settings, this);
    new PlaylistColumnPage(m_gui.actionManager, m_playlistController->columnRegistry(), m_settings, this);
    new PlaylistPresetsPage(m_playlistController->presetRegistry(), m_settings, this);
    new PluginPage(m_core->pluginManager(), m_settings, this);
    new ShellIntegrationPage(m_settings, this);
    new ShortcutsPage(m_gui.actionManager, m_settings, this);
    new OutputPage(m_core->engine(), m_settings, this);
    new DecoderPage(m_core->audioLoader().get(), m_settings, this);
    new ReplayGainPage(m_settings, this);
    new DirBrowserPage(m_settings, this);
    new LibraryTreePage(m_settings, this);
    new LibraryTreeGroupPage(m_gui.actionManager, m_libraryTreeController->groupRegistry(), m_settings, this);
    new PlaybackQueuePage(m_settings, this);
    new StatusWidgetPage(m_settings, this);
}

void Widgets::registerPropertiesTabs()
{
    m_gui.propertiesDialog->addTab(tr("Details"),
                                   [this](const TrackList& tracks) { return new InfoWidget(tracks, m_window); });
}

void Widgets::registerFontEntries() const
{
    auto* themeReg = m_gui.themeRegistry;

    themeReg->registerFontEntry(tr("Default"), {});
    themeReg->registerFontEntry(tr("Lists"), QStringLiteral("QAbstractItemView"));
    themeReg->registerFontEntry(tr("Library Tree"), QStringLiteral("Fooyin::LibraryTreeView"));
    themeReg->registerFontEntry(tr("Playlist"), QStringLiteral("Fooyin::PlaylistView"));
    themeReg->registerFontEntry(tr("Status bar"), QStringLiteral("Fooyin::StatusLabel"));
    themeReg->registerFontEntry(tr("Tabs"), QStringLiteral("Fooyin::EditableTabBar"));
}

void Widgets::showScanProgress(const ScanProgress& progress) const
{
    if(!m_statusWidget || progress.id < 0) {
        return;
    }

    QString scanText;
    switch(progress.type) {
        case(ScanRequest::Files):
            scanText = tr("Scanning files");
            break;
        case(ScanRequest::Tracks):
            scanText = tr("Scanning tracks");
            break;
        case(ScanRequest::Library):
            scanText = tr("Scanning %1").arg(progress.info.name);
            break;
        case(ScanRequest::Playlist):
            scanText = tr("Loading playlist");
            break;
    }

    scanText += QStringLiteral(": %1%").arg(progress.percentage());
    m_statusWidget->showTempMessage(scanText);
}
} // namespace Fooyin
