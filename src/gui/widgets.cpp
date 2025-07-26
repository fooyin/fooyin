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

#include "artwork/artworkdialog.h"
#include "artwork/artworkproperties.h"
#include "controls/playercontrol.h"
#include "controls/playlistcontrol.h"
#include "controls/seekbar.h"
#include "controls/volumecontrol.h"
#include "dirbrowser/dirbrowser.h"
#include "guiapplication.h"
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
#include "replaygain/replaygainwidget.h"
#include "search/searchwidget.h"
#include "selectioninfo/infowidget.h"
#include "settings/artwork/artworkdownloadpage.h"
#include "settings/artwork/artworkgeneralpage.h"
#include "settings/dirbrowser/dirbrowserpage.h"
#include "settings/generalpage.h"
#include "settings/guigeneralpage.h"
#include "settings/guithemespage.h"
#include "settings/library/librarygeneralpage.h"
#include "settings/library/librarysortingpage.h"
#include "settings/librarytree/librarytreegrouppage.h"
#include "settings/librarytree/librarytreepage.h"
#include "settings/networkpage.h"
#include "settings/playback/decoderpage.h"
#include "settings/playback/outputpage.h"
#include "settings/playback/playbackpage.h"
#include "settings/playback/replaygainpage.h"
#include "settings/playlist/playlistcolumnpage.h"
#include "settings/playlist/playlistgeneralpage.h"
#include "settings/playlist/playlistpresetspage.h"
#include "settings/plugins/pluginspage.h"
#include "settings/searchpage.h"
#include "settings/shellintegrationpage.h"
#include "settings/shortcuts/shortcutspage.h"
#include "settings/widgets/playbackqueuepage.h"
#include "settings/widgets/statuswidgetpage.h"
#include "splitters/splitterwidget.h"
#include "splitters/tabstackwidget.h"
#include "statusevent.h"
#include "widgets/coverwidget.h"
#include "widgets/dummy.h"
#include "widgets/spacer.h"
#include "widgets/statuswidget.h"

#include <core/application.h>
#include <core/library/musiclibrary.h>
#include <core/player/playercontroller.h>
#include <core/playlist/playlisthandler.h>
#include <core/plugins/coreplugincontext.h>
#include <gui/coverprovider.h>
#include <gui/theme/themeregistry.h>
#include <gui/widgetprovider.h>

using namespace Qt::StringLiterals;

namespace Fooyin {
Widgets::Widgets(Application* core, MainWindow* window, GuiApplication* gui, PlaylistInteractor* playlistInteractor,
                 QObject* parent)
    : QObject{parent}
    , m_core{core}
    , m_gui{gui}
    , m_window{window}
    , m_settings{m_core->settingsManager()}
    , m_coverProvider{new CoverProvider(m_core->audioLoader(), m_settings, this)}
    , m_playlistInteractor{playlistInteractor}
    , m_playlistController{playlistInteractor->playlistController()}
    , m_libraryTreeController{new LibraryTreeController(m_settings, this)}
{
    QObject::connect(m_core->library(), &MusicLibrary::scanProgress, this, &Widgets::showScanProgress);
}

void Widgets::registerWidgets()
{
    auto* provider = m_gui->widgetProvider();

    provider->registerWidget(u"Dummy"_s, [this]() { return new Dummy(m_settings, m_window); }, tr("Dummy"));
    provider->setIsHidden(u"Dummy"_s, true);

    provider->registerWidget(
        u"SplitterVertical"_s,
        [this, provider]() { return new VerticalSplitterWidget(provider, m_settings, m_window); },
        tr("Splitter (Top/Bottom)"));
    provider->setSubMenus(u"SplitterVertical"_s, {tr("Splitters")});

    provider->registerWidget(
        u"SplitterHorizontal"_s,
        [this]() { return new HorizontalSplitterWidget(m_gui->widgetProvider(), m_settings, m_window); },
        tr("Splitter (Left/Right)"));
    provider->setSubMenus(u"SplitterHorizontal"_s, {tr("Splitters")});

    provider->registerWidget(
        u"PlaylistSwitcher"_s, [this]() { return new PlaylistBox(m_playlistController, m_window); },
        tr("Playlist Switcher"));

    provider->registerWidget(
        u"PlaylistTabs"_s,
        [this]() {
            auto* playlistTabs = new PlaylistTabs(m_gui->actionManager(), m_gui->widgetProvider(), m_playlistController,
                                                  m_settings, m_window);
            QObject::connect(playlistTabs, &PlaylistTabs::filesDropped, m_playlistInteractor,
                             &PlaylistInteractor::filesToPlaylist);
            QObject::connect(playlistTabs, &PlaylistTabs::tracksDropped, m_playlistInteractor,
                             &PlaylistInteractor::trackMimeToPlaylist);
            return playlistTabs;
        },
        tr("Playlist Tabs"));
    provider->setSubMenus(u"PlaylistTabs"_s, {tr("Splitters")});

    provider->registerWidget(
        u"PlaylistOrganiser"_s,
        [this]() { return new PlaylistOrganiser(m_gui->actionManager(), m_playlistInteractor, m_settings, m_window); },
        tr("Playlist Organiser"));

    provider->registerWidget(
        u"PlaybackQueue"_s,
        [this]() {
            return new QueueViewer(m_gui->actionManager(), m_playlistInteractor, m_core->audioLoader(), m_settings,
                                   m_window);
        },
        tr("Playback Queue"));
    provider->setLimit(u"PlaybackQueue"_s, 1);

    provider->registerWidget(
        u"TabStack"_s, [this, provider]() { return new TabStackWidget(provider, m_settings, m_window); },
        tr("Tab Stack"));
    provider->setSubMenus(u"TabStack"_s, {tr("Splitters")});

    provider->registerWidget(
        u"LibraryTree"_s,
        [this]() {
            return new LibraryTreeWidget(m_gui->actionManager(), m_playlistController, m_libraryTreeController, m_core,
                                         m_window);
        },
        tr("Library Tree"));

    provider->registerWidget(
        u"PlayerControls"_s,
        [this]() {
            return new PlayerControl(m_gui->actionManager(), m_core->playerController(), m_settings, m_window);
        },
        tr("Player Controls"));
    provider->setSubMenus(u"PlayerControls"_s, {tr("Controls")});

    provider->registerWidget(
        u"PlaylistControls"_s,
        [this]() { return new PlaylistControl(m_core->playerController(), m_settings, m_window); },
        tr("Playlist Controls"));
    provider->setSubMenus(u"PlaylistControls"_s, {tr("Controls")});

    provider->registerWidget(
        u"VolumeControls"_s, [this]() { return new VolumeControl(m_gui->actionManager(), m_settings, m_window); },
        tr("Volume Controls"));
    provider->setSubMenus(u"VolumeControls"_s, {tr("Controls")});

    provider->registerWidget(
        u"SeekBar"_s, [this]() { return new SeekBar(m_core->playerController(), m_settings, m_window); },
        tr("Seekbar"));
    provider->setSubMenus(u"SeekBar"_s, {tr("Controls")});

    provider->registerWidget(
        u"SelectionInfo"_s, [this]() { return new InfoWidget(m_core, m_gui->trackSelection(), m_window); },
        tr("Selection Info"));

    provider->registerWidget(
        u"ArtworkPanel"_s,
        [this]() {
            auto* coverWidget = new CoverWidget(m_core->playerController(), m_gui->trackSelection(),
                                                m_core->audioLoader(), m_settings, m_window);
            QObject::connect(coverWidget, &CoverWidget::requestArtworkSearch, this, &Widgets::showArtworkDialog);
            return coverWidget;
        },
        tr("Artwork Panel"));

    provider->registerWidget(
        u"Playlist"_s,
        [this]() {
            return new PlaylistWidget(m_gui->actionManager(), m_playlistInteractor, m_coverProvider, m_core,
                                      PlaylistWidget::Mode::Playlist, m_window);
        },
        tr("Playlist"));
    provider->setLimit(u"Playlist"_s, 1);

    provider->registerWidget(u"Spacer"_s, [this]() { return new Spacer(m_window); }, tr("Spacer"));

    provider->registerWidget(
        u"StatusBar"_s,
        [this]() {
            auto* statusWidget = new StatusWidget(m_core->playerController(), m_core->playlistHandler(),
                                                  m_gui->trackSelection(), m_settings, m_window);
            m_window->installStatusWidget(statusWidget);
            return statusWidget;
        },
        tr("Status Bar"));
    provider->setLimit(u"StatusBar"_s, 1);

    provider->registerWidget(
        u"SearchBar"_s,
        [this]() {
            return new SearchWidget(m_gui->searchController(), m_gui->playlistController(), m_core->library(),
                                    m_settings, m_window);
        },
        tr("Search Bar"));

    provider->registerWidget(u"DirectoryBrowser"_s, [this]() { return createDirBrowser(); }, tr("Directory Browser"));
    provider->setLimit(u"DirectoryBrowser"_s, 1);
}

void Widgets::registerPages()
{
    new GeneralPage(m_settings, this);
    new GuiGeneralPage(m_gui->layoutProvider(), m_gui->editableLayout(), m_settings, this);
    new GuiThemesPage(m_gui->themeRegistry(), m_settings, this);
    new ArtworkGeneralPage(m_settings, this);
    new ArtworkDownloadPage(m_settings, this);
    new LibraryGeneralPage(m_gui->actionManager(), m_core->libraryManager(), m_core->library(), m_settings, this);
    new LibrarySortingPage(m_gui->actionManager(), m_core->sortingRegistry(), m_settings, this);
    new PlaybackPage(m_settings, this);
    new PlaylistGeneralPage(m_core->playlistLoader()->supportedSaveExtensions(), m_settings, this);
    new PlaylistColumnPage(m_gui->actionManager(), m_playlistController->columnRegistry(), m_settings, this);
    new PlaylistPresetsPage(m_playlistController->presetRegistry(), m_settings, this);
    new PluginPage(m_core->pluginManager(), m_settings, this);
    new ShellIntegrationPage(m_settings, this);
    new ShortcutsPage(m_gui->actionManager(), m_settings, this);
    new OutputPage(m_core->engine(), m_settings, this);
    new DecoderPage(m_core->audioLoader().get(), m_settings, this);
    new ReplayGainPage(m_settings, this);
    new NetworkPage(m_settings, this);
    new SearchPage(m_settings, this);
    new DirBrowserPage(m_settings, this);
    new LibraryTreePage(m_settings, this);
    new LibraryTreeGroupPage(m_gui->actionManager(), m_libraryTreeController->groupRegistry(), m_settings, this);
    new PlaybackQueuePage(m_settings, this);
    new StatusWidgetPage(m_settings, this);
}

void Widgets::registerPropertiesTabs()
{
    m_gui->propertiesDialog()->addTab(tr("Details"), [this](const TrackList& tracks) {
        return new InfoWidget(tracks, m_core->libraryManager(), m_window);
    });
    m_gui->propertiesDialog()->addTab(tr("ReplayGain"), [this](const TrackList& tracks) {
        const bool canWrite = std::ranges::all_of(tracks, [this](const Track& track) {
            return !track.hasCue() && !track.isInArchive() && m_core->audioLoader()->canWriteMetadata(track);
        });
        return new ReplayGainWidget(m_core->library(), tracks, !canWrite, m_window);
    });
    m_gui->propertiesDialog()->addTab(tr("Artwork"), [this](const TrackList& tracks) {
        const bool canWrite = std::ranges::all_of(tracks, [this](const Track& track) {
            return !track.hasCue() && !track.isInArchive() && m_core->audioLoader()->canWriteMetadata(track);
        });
        return new ArtworkProperties(m_core->audioLoader().get(), m_core->library(), tracks, !canWrite, m_window);
    });
}

void Widgets::registerFontEntries() const
{
    auto* themeReg = m_gui->themeRegistry();

    themeReg->registerFontEntry(tr("Default"), {});
    themeReg->registerFontEntry(tr("Lists"), u"QAbstractItemView"_s);
    themeReg->registerFontEntry(tr("Library Tree"), u"Fooyin::LibraryTreeView"_s);
    themeReg->registerFontEntry(tr("Playlist"), u"Fooyin::PlaylistView"_s);
    themeReg->registerFontEntry(tr("Status bar"), u"Fooyin::StatusLabel"_s);
    themeReg->registerFontEntry(tr("Tabs"), u"Fooyin::EditableTabBar"_s);
}

void Widgets::showArtworkDialog(const TrackList& tracks, Track::Cover type, bool quick)
{
    m_gui->searchForArtwork(tracks, type, quick);
}

FyWidget* Widgets::createDirBrowser()
{
    auto* browser = new DirBrowser(m_core->audioLoader()->supportedFileExtensions(), m_gui->actionManager(),
                                   m_playlistInteractor, m_settings, m_window);

    browser->playstateChanged(m_core->playerController()->playState());
    browser->activePlaylistChanged(m_core->playlistHandler()->activePlaylist());
    browser->playlistTrackChanged(m_core->playerController()->currentPlaylistTrack());

    QObject::connect(m_core->playerController(), &PlayerController::playStateChanged, browser,
                     &DirBrowser::playstateChanged);
    QObject::connect(m_core->playerController(), &PlayerController::playlistTrackChanged, browser,
                     &DirBrowser::playlistTrackChanged);
    QObject::connect(m_core->playlistHandler(), &PlaylistHandler::activePlaylistChanged, browser,
                     &DirBrowser::activePlaylistChanged);

    return browser;
}

void Widgets::showScanProgress(const ScanProgress& progress)
{
    if(progress.id < 0) {
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

    scanText = u"%1: %2%"_s.arg(scanText).arg(progress.percentage());
    StatusEvent::post(scanText);
}
} // namespace Fooyin
