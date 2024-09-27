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

#include "librarytreewidget.h"

#include "internalguisettings.h"
#include "librarytree/librarytreecontroller.h"
#include "librarytreedelegate.h"
#include "librarytreegroupregistry.h"
#include "librarytreemodel.h"
#include "librarytreeview.h"
#include "playlist/playlistcontroller.h"

#include <core/application.h>
#include <core/coresettings.h>
#include <core/library/libraryinfo.h>
#include <core/library/musiclibrary.h>
#include <core/player/playercontroller.h>
#include <core/player/playerdefs.h>
#include <core/playlist/playlisthandler.h>
#include <core/plugins/coreplugincontext.h>
#include <gui/guiconstants.h>
#include <gui/guisettings.h>
#include <gui/trackselectioncontroller.h>
#include <utils/actions/actionmanager.h>
#include <utils/actions/command.h>
#include <utils/actions/widgetcontext.h>
#include <utils/async.h>
#include <utils/datastream.h>
#include <utils/fileutils.h>
#include <utils/signalthrottler.h>
#include <utils/tooltipfilter.h>

#include <QActionGroup>
#include <QFileInfo>
#include <QHeaderView>
#include <QJsonObject>
#include <QKeyEvent>
#include <QMenu>
#include <QTreeView>
#include <QVBoxLayout>

#include <stack>

constexpr auto LibTreePlaylist = "␟LibTreePlaylist␟";

namespace {
QModelIndexList filterAncestors(const QModelIndexList& indexes)
{
    QModelIndexList filteredIndexes;
    QModelIndexList indexesToRemove;

    for(const QModelIndex& index : indexes) {
        const QModelIndex parentIndex = index.parent();
        if(indexes.contains(parentIndex) || filteredIndexes.contains(parentIndex)) {
            for(const QModelIndex& childIndex : filteredIndexes) {
                if(childIndex.parent() == parentIndex) {
                    indexesToRemove.emplace_back(childIndex);
                }
            }
        }
        else {
            filteredIndexes.emplace_back(index);
        }
    }

    for(const QModelIndex& index : std::as_const(indexesToRemove)) {
        filteredIndexes.removeAll(index);
    }

    return filteredIndexes;
}

Fooyin::TrackList getSelectedTracks(const QTreeView* treeView, Fooyin::MusicLibrary* library)
{
    const QModelIndexList selectedIndexes = treeView->selectionModel()->selectedRows();
    if(selectedIndexes.empty()) {
        return {};
    }

    const QModelIndexList filteredIndexes = filterAncestors(selectedIndexes);

    Fooyin::TrackList tracks;
    QModelIndexList trackIndexes;

    for(const QModelIndex& index : filteredIndexes) {
        const int level = index.data(Fooyin::LibraryTreeItem::Level).toInt();
        if(level < 0) {
            trackIndexes.clear();
            tracks = library->tracks();
            break;
        }
        const auto indexTracks = index.data(Fooyin::LibraryTreeItem::Tracks).value<Fooyin::TrackList>();
        tracks.insert(tracks.end(), indexTracks.cbegin(), indexTracks.cend());
    }

    return tracks;
}

QModelIndexList getAllChildren(QAbstractItemModel* model, const QModelIndex& parent)
{
    QModelIndexList children;

    if(model->canFetchMore(parent)) {
        model->fetchMore(parent);
    }

    const int childCount = model->rowCount(parent);
    for(int row{0}; row < childCount; ++row) {
        const QModelIndex child = model->index(row, 0, parent);
        children.append(child);
        children.append(getAllChildren(model, child));
    }

    return children;
}

QModelIndexList filterLeafNodes(QAbstractItemModel* model, const QModelIndexList& indexes)
{
    QModelIndexList leafNodes;
    QSet<QModelIndex> processedIndexes;

    for(const QModelIndex& index : indexes) {
        const QModelIndexList children = getAllChildren(model, index);

        if(children.isEmpty()) {
            leafNodes.append(index);
        }
        else {
            for(const QModelIndex& child : children) {
                if(model->rowCount(child) == 0 && !processedIndexes.contains(child)) {
                    leafNodes.append(child);
                    processedIndexes.insert(child);
                }
            }
        }
    }

    return leafNodes;
}
} // namespace

namespace Fooyin {
using namespace Settings::Gui::Internal;

class LibraryTreeWidgetPrivate
{
public:
    LibraryTreeWidgetPrivate(LibraryTreeWidget* self, ActionManager* actionManager,
                             PlaylistController* playlistController, LibraryTreeController* controller,
                             Application* core);

    void setupConnections();
    void reset() const;

    void changeGrouping(const LibraryTreeGrouping& newGrouping);

    void activePlaylistChanged(Playlist* playlist) const;
    void playlistTrackChanged(const PlaylistTrack& track);

    void addGroupMenu(QMenu* parent);
    void addOpenMenu(QMenu* menu) const;

    void setScrollbarEnabled(bool enabled) const;

    void setupHeaderContextMenu(const QPoint& pos);

    void selectionChanged(const QItemSelection& selected, const QItemSelection& deselected) const;

    void queueSelectedTracks() const;
    void dequeueSelectedTracks() const;

    void searchChanged(const QString& search);

    void handlePlayback(const QModelIndexList& indexes, int row = 0);
    void handlePlayTrack();
    void handleDoubleClick(const QModelIndex& index);
    void handleMiddleClick(const QModelIndex& index) const;

    void handleTracksAdded(const TrackList& tracks) const;
    void handleTracksUpdated(const TrackList& tracks);

    void restoreSelection(const QStringList& expandedTitles, const QStringList& selectedTitles);
    [[nodiscard]] QByteArray saveState() const;
    void restoreIndexState(const Md5Hash& topKey, const std::vector<Md5Hash>& keys, int currentIndex = 0);
    void restoreState(const QByteArray& state);

    LibraryTreeWidget* m_self;

    ActionManager* m_actionManager;
    MusicLibrary* m_library;
    PlaylistHandler* m_playlistHandler;
    PlayerController* m_playerController;
    LibraryTreeGroupRegistry* m_groupsRegistry;
    TrackSelectionController* m_trackSelection;
    SettingsManager* m_settings;

    SignalThrottler* m_resetThrottler;
    LibraryTreeGrouping m_grouping;

    QVBoxLayout* m_layout;
    LibraryTreeView* m_libraryTree;
    LibraryTreeModel* m_model;
    LibraryTreeSortModel* m_sortProxy;

    WidgetContext* m_widgetContext;

    QAction* m_addToQueueAction;
    QAction* m_removeFromQueueAction;
    QAction* m_playAction;

    TrackAction m_doubleClickAction;
    TrackAction m_middleClickAction;

    QString m_prevSearch;
    TrackList m_prevSearchTracks;

    bool m_updating{false};
    QByteArray m_pendingState;

    Playlist* m_playlist{nullptr};
    std::map<int, QString> m_playlistGroups;
};

LibraryTreeWidgetPrivate::LibraryTreeWidgetPrivate(LibraryTreeWidget* self, ActionManager* actionManager,
                                                   PlaylistController* playlistController,
                                                   LibraryTreeController* controller, Application* core)
    : m_self{self}
    , m_actionManager{actionManager}
    , m_library{core->library()}
    , m_playlistHandler{playlistController->playlistHandler()}
    , m_playerController{playlistController->playerController()}
    , m_groupsRegistry{controller->groupRegistry()}
    , m_trackSelection{playlistController->selectionController()}
    , m_settings{core->settingsManager()}
    , m_resetThrottler{new SignalThrottler(m_self)}
    , m_layout{new QVBoxLayout(m_self)}
    , m_libraryTree{new LibraryTreeView(m_self)}
    , m_model{new LibraryTreeModel(core->libraryManager(), m_settings, m_self)}
    , m_sortProxy{new LibraryTreeSortModel(m_self)}
    , m_widgetContext{new WidgetContext(m_self, Context{Id{"Fooyin.Context.LibraryTree."}.append(m_self->id())},
                                        m_self)}
    , m_addToQueueAction{new QAction(LibraryTreeWidget::tr("&Add to playback queue"), m_self)}
    , m_removeFromQueueAction{new QAction(LibraryTreeWidget::tr("&Remove from playback queue"), m_self)}
    , m_playAction{new QAction(LibraryTreeWidget::tr("&Play"), m_self)}
    , m_doubleClickAction{static_cast<TrackAction>(m_settings->value<LibTreeDoubleClick>())}
    , m_middleClickAction{static_cast<TrackAction>(m_settings->value<LibTreeMiddleClick>())}
{
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->addWidget(m_libraryTree);

    m_sortProxy->setSourceModel(m_model);
    m_libraryTree->setModel(m_sortProxy);
    m_libraryTree->setItemDelegate(new LibraryTreeDelegate(m_self));
    m_libraryTree->viewport()->installEventFilter(new ToolTipFilter(m_self));

    m_libraryTree->setExpandsOnDoubleClick(m_doubleClickAction == TrackAction::None
                                           || m_doubleClickAction == TrackAction::Play);
    m_libraryTree->setAnimated(m_settings->value<Settings::Gui::Internal::LibTreeAnimated>());
    m_libraryTree->setHeaderHidden(!m_settings->value<Settings::Gui::Internal::LibTreeHeader>());

    setScrollbarEnabled(m_settings->value<LibTreeScrollBar>());
    m_libraryTree->setAlternatingRowColors(m_settings->value<LibTreeAltColours>());

    if(const auto initialGroup = m_groupsRegistry->itemByIndex(0)) {
        changeGrouping(initialGroup.value());
    }

    m_actionManager->addContextObject(m_widgetContext);
    m_trackSelection->changePlaybackOnSend(m_widgetContext, m_settings->value<LibTreeSendPlayback>());

    m_model->setRowHeight(m_settings->value<LibTreeRowHeight>());
    m_model->setPlayState(playlistController->playerController()->playState());

    m_playAction->setStatusTip(LibraryTreeWidget::tr("Start playback of the selected tracks"));
    QObject::connect(m_playAction, &QAction::triggered, m_self,
                     [this]() { handlePlayback(m_libraryTree->selectionModel()->selectedRows()); });

    m_addToQueueAction->setEnabled(false);
    m_actionManager->registerAction(m_addToQueueAction, Constants::Actions::AddToQueue, m_widgetContext->context());
    QObject::connect(m_addToQueueAction, &QAction::triggered, m_self, [this]() { queueSelectedTracks(); });

    m_removeFromQueueAction->setVisible(false);
    m_actionManager->registerAction(m_removeFromQueueAction, Constants::Actions::RemoveFromQueue,
                                    m_widgetContext->context());
    QObject::connect(m_removeFromQueueAction, &QAction::triggered, m_self, [this]() { dequeueSelectedTracks(); });

    setupConnections();
}

void LibraryTreeWidgetPrivate::setupConnections()
{
    QObject::connect(m_resetThrottler, &SignalThrottler::triggered, m_self,
                     [this]() { m_model->reset(m_library->tracks()); });

    QObject::connect(m_model, &LibraryTreeModel::dataUpdated, m_libraryTree, &QTreeView::dataChanged);
    QObject::connect(m_model, &LibraryTreeModel::modelLoaded, m_self, [this]() { restoreState(m_pendingState); });

    QObject::connect(m_libraryTree, &LibraryTreeView::doubleClicked, m_self,
                     [this](const QModelIndex& index) { handleDoubleClick(index); });
    QObject::connect(m_libraryTree, &LibraryTreeView::middleClicked, m_self,
                     [this](const QModelIndex& index) { handleMiddleClick(index); });
    QObject::connect(m_libraryTree->selectionModel(), &QItemSelectionModel::selectionChanged, m_self,
                     [this](const QItemSelection& selected, const QItemSelection& deselected) {
                         selectionChanged(selected, deselected);
                     });
    QObject::connect(m_libraryTree->header(), &QHeaderView::customContextMenuRequested, m_self,
                     [this](const QPoint& pos) { setupHeaderContextMenu(pos); });
    QObject::connect(m_groupsRegistry, &LibraryTreeGroupRegistry::groupingChanged, m_self,
                     [this](const LibraryTreeGrouping& changedGrouping) {
                         if(m_grouping.id == changedGrouping.id) {
                             changeGrouping(changedGrouping);
                         }
                     });

    QObject::connect(m_library, &MusicLibrary::tracksLoaded, m_self, [this]() { reset(); });
    QObject::connect(m_library, &MusicLibrary::tracksAdded, m_self,
                     [this](const TrackList& tracks) { handleTracksAdded(tracks); });
    QObject::connect(m_library, &MusicLibrary::tracksScanned, m_model,
                     [this](int /*id*/, const TrackList& tracks) { handleTracksAdded(tracks); });
    QObject::connect(m_library, &MusicLibrary::tracksMetadataChanged, m_self,
                     [this](const TrackList& tracks) { handleTracksUpdated(tracks); });
    QObject::connect(m_library, &MusicLibrary::tracksUpdated, m_self,
                     [this](const TrackList& tracks) { m_model->refreshTracks(tracks); });
    QObject::connect(m_library, &MusicLibrary::tracksDeleted, m_model, &LibraryTreeModel::removeTracks);
    QObject::connect(m_library, &MusicLibrary::tracksSorted, m_self, [this]() { reset(); });

    QObject::connect(m_playerController, &PlayerController::playStateChanged, m_self,
                     [this](Player::PlayState state) { m_model->setPlayState(state); });
    QObject::connect(m_playerController, &PlayerController::playlistTrackChanged, m_self,
                     [this](const auto& track) { playlistTrackChanged(track); });
    QObject::connect(m_playlistHandler, &PlaylistHandler::activePlaylistChanged, m_self,
                     [this](auto* playlist) { activePlaylistChanged(playlist); });

    m_settings->subscribe<LibTreeDoubleClick>(m_self, [this](int action) {
        m_doubleClickAction = static_cast<TrackAction>(action);
        m_libraryTree->setExpandsOnDoubleClick(m_doubleClickAction == TrackAction::None
                                               || m_doubleClickAction == TrackAction::Play);
    });
    m_settings->subscribe<LibTreeMiddleClick>(
        m_self, [this](int action) { m_middleClickAction = static_cast<TrackAction>(action); });
    m_settings->subscribe<LibTreeSendPlayback>(
        m_self, [this](bool enabled) { m_trackSelection->changePlaybackOnSend(m_widgetContext, enabled); });
    m_settings->subscribe<LibTreeAnimated>(m_self, [this](bool enable) { m_libraryTree->setAnimated(enable); });
    m_settings->subscribe<LibTreeHeader>(m_self, [this](bool enable) { m_libraryTree->setHeaderHidden(!enable); });
    m_settings->subscribe<LibTreeScrollBar>(m_self, [this](bool show) { setScrollbarEnabled(show); });
    m_settings->subscribe<LibTreeAltColours>(m_self,
                                             [this](bool enable) { m_libraryTree->setAlternatingRowColors(enable); });
    m_settings->subscribe<LibTreeRowHeight>(m_self, [this](const int height) {
        m_model->setRowHeight(height);
        QMetaObject::invokeMethod(m_libraryTree->itemDelegate(), "sizeHintChanged", Q_ARG(QModelIndex, {}));
    });
    m_settings->subscribe<Settings::Gui::Theme>(m_model, &LibraryTreeModel::resetPalette);
    m_settings->subscribe<Settings::Gui::Style>(m_model, &LibraryTreeModel::resetPalette);

    m_settings->subscribe<Settings::Core::UseVariousForCompilations>(m_self, [this]() { reset(); });
}

void LibraryTreeWidgetPrivate::reset() const
{
    m_resetThrottler->throttle();
}

void LibraryTreeWidgetPrivate::changeGrouping(const LibraryTreeGrouping& newGrouping)
{
    if(std::exchange(m_grouping, newGrouping) != newGrouping) {
        m_model->changeGrouping(m_grouping);
        reset();
    }
}

void LibraryTreeWidgetPrivate::activePlaylistChanged(Playlist* playlist) const
{
    if(!playlist || !m_playlist) {
        return;
    }

    if(playlist->id() != m_playlist->id()) {
        m_model->setPlayingPath({}, {});
    }
}

void LibraryTreeWidgetPrivate::playlistTrackChanged(const PlaylistTrack& track)
{
    if(!m_playlist || m_playlist->id() != track.playlistId) {
        return;
    }

    auto groupIt = m_playlistGroups.upper_bound(track.indexInPlaylist);
    if(groupIt != m_playlistGroups.cbegin()) {
        --groupIt;
    }
    m_model->setPlayingPath(groupIt->second, track.track.uniqueFilepath());
}

void LibraryTreeWidgetPrivate::addGroupMenu(QMenu* parent)
{
    auto* groupMenu = new QMenu(LibraryTreeWidget::tr("Grouping"), parent);

    auto* treeGroups = new QActionGroup(groupMenu);

    const auto groups = m_groupsRegistry->items();
    for(const auto& group : groups) {
        auto* switchGroup = new QAction(group.name, groupMenu);
        QObject::connect(switchGroup, &QAction::triggered, m_self, [this, group]() { changeGrouping(group); });
        switchGroup->setCheckable(true);
        switchGroup->setChecked(m_grouping.id == group.id);
        groupMenu->addAction(switchGroup);
        treeGroups->addAction(switchGroup);
    }

    parent->addMenu(groupMenu);
}

void LibraryTreeWidgetPrivate::addOpenMenu(QMenu* menu) const
{
    const auto selected = m_libraryTree->selectionModel()->selectedRows();
    if(selected.size() != 1) {
        return;
    }

    if(m_grouping.id != 2) {
        // Only add if in folder structure view
        return;
    }

    const auto tracks = selected.front().data(LibraryTreeItem::Tracks).value<TrackList>();
    if(tracks.empty()) {
        return;
    }

    const auto libId   = tracks.front().libraryId();
    const auto library = m_library->libraryInfo(libId);
    if(!library) {
        return;
    }

    QString dir{library->path};
    QStringList parentDirs;

    QModelIndex index{selected.front()};
    while(index.isValid()) {
        parentDirs.prepend(index.data().toString());
        index = index.parent();
    }

    dir += u"/" + parentDirs.join(u'/');
    const QFileInfo info{dir};
    if(info.exists() && info.isDir()) {
        auto* openFolder = new QAction(LibraryTreeWidget::tr("Open folder"), m_self);
        QObject::connect(openFolder, &QAction::triggered, m_self, [dir]() { Utils::File::openDirectory(dir); });
        menu->addAction(openFolder);
    }
}

void LibraryTreeWidgetPrivate::setScrollbarEnabled(bool enabled) const
{
    m_libraryTree->setVerticalScrollBarPolicy(enabled ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
}

void LibraryTreeWidgetPrivate::setupHeaderContextMenu(const QPoint& pos)
{
    auto* menu = new QMenu(m_self);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    addGroupMenu(menu);
    menu->popup(m_self->mapToGlobal(pos));
}

void LibraryTreeWidgetPrivate::selectionChanged(const QItemSelection& selected, const QItemSelection& deselected) const
{
    if(m_updating) {
        return;
    }

    if(selected.indexes().empty() && deselected.indexes().empty()) {
        return;
    }

    std::set<Track> trackIndexes;
    const TrackList tracks = getSelectedTracks(m_libraryTree, m_library);
    m_trackSelection->changeSelectedTracks(m_widgetContext, tracks);

    for(const Track& track : tracks) {
        trackIndexes.emplace(track);
    }

    m_addToQueueAction->setEnabled(true);
    const auto queuedTracks = m_playerController->playbackQueue().tracks();
    const bool canDeque     = std::ranges::any_of(
        queuedTracks, [&trackIndexes](const PlaylistTrack& track) { return trackIndexes.contains(track.track); });
    m_removeFromQueueAction->setVisible(canDeque);

    if(m_settings->value<LibTreePlaylistEnabled>()) {
        PlaylistAction::ActionOptions options{PlaylistAction::None};

        if(m_settings->value<LibTreeKeepAlive>()) {
            options |= PlaylistAction::KeepActive;
        }
        if(m_settings->value<LibTreeAutoSwitch>()) {
            options |= PlaylistAction::Switch;
        }

        const QString autoPlaylist = m_settings->value<LibTreeAutoPlaylist>();
        m_trackSelection->executeAction(TrackAction::SendNewPlaylist, options, autoPlaylist);
    }
}

void LibraryTreeWidgetPrivate::queueSelectedTracks() const
{
    const QModelIndexList selectedIndexes = m_libraryTree->selectionModel()->selectedIndexes();
    if(selectedIndexes.empty()) {
        return;
    }

    m_trackSelection->executeAction(TrackAction::AddToQueue);
    m_removeFromQueueAction->setVisible(true);
}

void LibraryTreeWidgetPrivate::dequeueSelectedTracks() const
{
    const QModelIndexList selectedIndexes = m_libraryTree->selectionModel()->selectedIndexes();
    if(selectedIndexes.empty()) {
        return;
    }

    const QModelIndexList leafNodes = filterLeafNodes(m_sortProxy, selectedIndexes);

    if(leafNodes.empty()) {
        return;
    }

    TrackList tracks;

    for(const QModelIndex& index : leafNodes) {
        const auto indexTracks = index.data(LibraryTreeItem::Tracks).value<TrackList>();
        tracks.insert(tracks.end(), indexTracks.cbegin(), indexTracks.cend());
    }

    if(tracks.empty()) {
        return;
    }

    m_playerController->dequeueTracks(tracks);
    m_removeFromQueueAction->setVisible(false);
}

void LibraryTreeWidgetPrivate::searchChanged(const QString& search)
{
    const bool reset   = m_prevSearch.length() > search.length();
    const QString prev = std::exchange(m_prevSearch, search);

    if(prev.length() >= 2 && search.length() < 2) {
        m_prevSearchTracks.clear();
        m_model->reset(m_library->tracks());
        return;
    }

    if(search.length() < 2) {
        return;
    }

    const TrackList tracksToFilter = !reset && !m_prevSearchTracks.empty() ? m_prevSearchTracks : m_library->tracks();

    Utils::asyncExec([search, tracksToFilter]() {
        ScriptParser parser;
        return parser.filter(search, tracksToFilter);
    }).then(m_self, [this](const TrackList& filteredTracks) {
        m_prevSearchTracks = filteredTracks;
        m_model->reset(filteredTracks);
    });
}

void LibraryTreeWidgetPrivate::handlePlayback(const QModelIndexList& indexes, int row)
{
    m_playlistGroups.clear();

    const QModelIndexList leafNodes = filterLeafNodes(m_sortProxy, indexes);

    if(leafNodes.empty()) {
        return;
    }

    TrackList tracks;

    QModelIndex parent;
    for(const QModelIndex& index : leafNodes) {
        if(index.data(LibraryTreeItem::TrackCount).toInt() != 1) {
            continue;
        }

        if(!parent.isValid()) {
            parent = index.parent();
        }
        else if(index.parent() != parent) {
            if(m_playlistGroups.empty()) {
                m_playlistGroups[0] = parent.data(Qt::DisplayRole).toString();
            }
            parent = index.parent();

            m_playlistGroups[static_cast<int>(tracks.size())] = parent.data(Qt::DisplayRole).toString();
        }
        const auto indexTracks = index.data(LibraryTreeItem::Tracks).value<TrackList>();
        tracks.insert(tracks.end(), indexTracks.cbegin(), indexTracks.cend());
    }

    if(tracks.empty()) {
        return;
    }

    if(parent.isValid() && m_playlistGroups.empty()) {
        m_playlistGroups[0] = parent.data(Qt::DisplayRole).toString();
    }

    m_playlist = m_playlistHandler->createTempPlaylist(QString::fromLatin1(LibTreePlaylist), tracks);
    if(m_playlist) {
        m_playlist->changeCurrentIndex(row);
        m_playlistHandler->startPlayback(m_playlist);
    }
}

void LibraryTreeWidgetPrivate::handlePlayTrack()
{
    const QModelIndexList selectedIndexes = m_libraryTree->selectionModel()->selectedIndexes();
    if(selectedIndexes.empty()) {
        return;
    }

    const QModelIndex index = selectedIndexes.front();

    if(m_sortProxy->hasChildren(index)) {
        return;
    }

    const int row            = index.row();
    const QModelIndex parent = index.parent();
    const int count          = m_sortProxy->rowCount(parent);

    QModelIndexList trackIndexes;

    for(int i{0}; i < count; ++i) {
        trackIndexes.emplace_back(m_sortProxy->index(i, 0, parent));
    }

    handlePlayback(trackIndexes, row);
}

void LibraryTreeWidgetPrivate::handleDoubleClick(const QModelIndex& index)
{
    if(!index.isValid()) {
        return;
    }

    if(m_doubleClickAction == TrackAction::None) {
        return;
    }

    if(m_doubleClickAction == TrackAction::Play) {
        handlePlayTrack();
        return;
    }

    PlaylistAction::ActionOptions options;

    if(m_settings->value<LibTreeAutoSwitch>()) {
        options |= PlaylistAction::Switch;
    }
    if(m_settings->value<LibTreeSendPlayback>()) {
        options |= PlaylistAction::StartPlayback;
    }

    m_trackSelection->executeAction(m_doubleClickAction, options);
    m_removeFromQueueAction->setVisible(m_doubleClickAction == TrackAction::AddToQueue
                                        || m_doubleClickAction == TrackAction::SendToQueue);
}

void LibraryTreeWidgetPrivate::handleMiddleClick(const QModelIndex& index) const
{
    if(!index.isValid()) {
        return;
    }

    PlaylistAction::ActionOptions options;

    if(m_settings->value<LibTreeAutoSwitch>()) {
        options |= PlaylistAction::Switch;
    }
    if(m_settings->value<LibTreeSendPlayback>()) {
        options |= PlaylistAction::StartPlayback;
    }

    m_trackSelection->executeAction(m_middleClickAction, options);
    m_removeFromQueueAction->setVisible(m_middleClickAction == TrackAction::AddToQueue
                                        || m_middleClickAction == TrackAction::SendToQueue);
}

void LibraryTreeWidgetPrivate::handleTracksAdded(const TrackList& tracks) const
{
    if(tracks.empty()) {
        return;
    }

    if(!m_prevSearch.isEmpty()) {
        Utils::asyncExec([search = m_prevSearch, tracks]() {
            ScriptParser parser;
            return parser.filter(search, tracks);
        }).then(m_self, [this](const TrackList& filteredTracks) { m_model->addTracks(filteredTracks); });
    }
    else {
        m_model->addTracks(tracks);
    }
}

void LibraryTreeWidgetPrivate::handleTracksUpdated(const TrackList& tracks)
{
    if(tracks.empty()) {
        return;
    }

    m_updating = true;

    const QModelIndexList selectedRows = m_libraryTree->selectionModel()->selectedRows();
    const QModelIndexList trackParents = m_model->indexesForTracks(tracks);

    QStringList selectedTitles;
    for(const QModelIndex& index : selectedRows) {
        selectedTitles.emplace_back(index.data(Qt::DisplayRole).toString());
    }

    std::stack<QModelIndex> checkExpanded;
    for(const QModelIndex& index : trackParents) {
        if(index.isValid()) {
            checkExpanded.emplace(index);
        }
    }

    QStringList expandedTitles;
    while(!checkExpanded.empty()) {
        const QModelIndex index = checkExpanded.top();
        checkExpanded.pop();
        if(!index.isValid()) {
            continue;
        }

        if(m_libraryTree->isExpanded(index)) {
            const QString title = index.data(Qt::DisplayRole).toString();
            if(!expandedTitles.contains(title)) {
                expandedTitles.emplace_back(title);
            }
            const int rowCount = m_sortProxy->rowCount(index);
            for(int row{0}; row < rowCount; ++row) {
                checkExpanded.emplace(m_sortProxy->index(row, 0, index));
            }
        }
    }

    m_model->updateTracks(tracks);

    QObject::connect(
        m_model, &LibraryTreeModel::modelUpdated, m_self,
        [this, expandedTitles, selectedTitles]() { restoreSelection(expandedTitles, selectedTitles); },
        Qt::SingleShotConnection);
}

void LibraryTreeWidgetPrivate::restoreSelection(const QStringList& expandedTitles, const QStringList& selectedTitles)
{
    const QModelIndexList expandedIndexes = m_model->findIndexes(expandedTitles);
    const QModelIndexList selectedIndexes = m_model->findIndexes(selectedTitles);

    for(const QModelIndex& index : expandedIndexes) {
        if(index.isValid()) {
            m_libraryTree->setExpanded(m_sortProxy->mapFromSource(index), true);
        }
    }

    QItemSelection indexesToSelect;
    indexesToSelect.reserve(selectedIndexes.size());

    for(const QModelIndex& index : selectedIndexes) {
        if(index.isValid()) {
            const QModelIndex proxyIndex = m_sortProxy->mapFromSource(index);
            indexesToSelect.append({proxyIndex, proxyIndex});
        }
    }

    m_libraryTree->selectionModel()->select(indexesToSelect, QItemSelectionModel::ClearAndSelect);
    m_updating = false;
}

QByteArray LibraryTreeWidgetPrivate::saveState() const
{
    QByteArray data;
    QDataStream stream{&data, QIODeviceBase::WriteOnly};
    stream.setVersion(QDataStream::Qt_6_0);

    const QModelIndex topIndex = m_libraryTree->indexAt({0, 0});
    const auto topKey          = topIndex.data(LibraryTreeItem::Key).toByteArray();
    stream << topKey;

    std::vector<Md5Hash> keysToSave;
    std::stack<QModelIndex> indexes;
    indexes.emplace();

    while(!indexes.empty()) {
        const QModelIndex index = indexes.top();
        indexes.pop();

        if(m_libraryTree->isExpanded(index) || !index.isValid()) {
            if(index.isValid()) {
                keysToSave.emplace_back(index.data(LibraryTreeItem::Key).toByteArray());
            }

            const int childCount = m_sortProxy->rowCount(index);
            for(int row{0}; row < childCount; ++row) {
                indexes.push(m_sortProxy->index(row, 0, index));
            }
        }
    }

    if(!keysToSave.empty()) {
        stream << keysToSave;
    }

    data = qCompress(data, 9);
    return data;
}

void LibraryTreeWidgetPrivate::restoreIndexState(const Md5Hash& topKey, const std::vector<Md5Hash>& keys,
                                                 int currentIndex)
{
    // We use queued connections here so any expand calls have a chance to load their children in fetchMore
    if(std::cmp_greater_equal(currentIndex, keys.size())) {
        QMetaObject::invokeMethod(
            m_libraryTree,
            [this, topKey]() {
                if(const QModelIndex topIndex = m_model->indexForKey(topKey); topIndex.isValid()) {
                    m_libraryTree->scrollTo(m_sortProxy->mapFromSource(topIndex), QAbstractItemView::PositionAtTop);
                }
                m_libraryTree->setLoading(false);
                m_libraryTree->setAnimated(m_settings->value<Settings::Gui::Internal::LibTreeAnimated>());
            },
            Qt::QueuedConnection);

        return;
    }

    const auto& key = keys.at(currentIndex);
    if(const auto index = m_model->indexForKey(key); index.isValid()) {
        m_libraryTree->expand(m_sortProxy->mapFromSource(index));
        QMetaObject::invokeMethod(
            m_libraryTree, [this, topKey, keys, currentIndex]() { restoreIndexState(topKey, keys, currentIndex + 1); },
            Qt::QueuedConnection);
    }
    else {
        restoreIndexState(topKey, keys, currentIndex + 1);
    }
}

void LibraryTreeWidgetPrivate::restoreState(const QByteArray& state)
{
    if(state.isEmpty() || !m_settings->value<LibTreeRestoreState>()) {
        return;
    }

    // Disable animation during state restore to prevent visual artifacts
    m_libraryTree->setAnimated(false);

    QByteArray data{state};
    QDataStream stream{&data, QIODeviceBase::ReadOnly};
    stream.setVersion(QDataStream::Qt_6_0);

    data = qUncompress(data);

    Md5Hash topKey;
    stream >> topKey;

    std::vector<Md5Hash> keysToRestore;
    stream >> keysToRestore;

    restoreIndexState(topKey, keysToRestore);
}

LibraryTreeWidget::LibraryTreeWidget(ActionManager* actionManager, PlaylistController* playlistController,
                                     LibraryTreeController* controller, Application* core, QWidget* parent)
    : FyWidget{parent}
    , p{std::make_unique<LibraryTreeWidgetPrivate>(this, actionManager, playlistController, controller, core)}
{
    setObjectName(LibraryTreeWidget::name());

    setFeature(FyWidget::Search);
}

LibraryTreeWidget::~LibraryTreeWidget()
{
    QObject::disconnect(p->m_resetThrottler, nullptr, this, nullptr);
}

QString LibraryTreeWidget::name() const
{
    return tr("Library Tree");
}

QString LibraryTreeWidget::layoutName() const
{
    return QStringLiteral("LibraryTree");
}

void LibraryTreeWidget::saveLayoutData(QJsonObject& layout)
{
    layout[u"Grouping"] = p->m_grouping.id;
    layout[u"State"]    = QString::fromUtf8(p->saveState().toBase64());
}

void LibraryTreeWidget::loadLayoutData(const QJsonObject& layout)
{
    if(layout.contains(u"Grouping")) {
        if(const auto grouping = p->m_groupsRegistry->itemById(layout.value(u"Grouping").toInt())) {
            if(grouping->isValid()) {
                p->changeGrouping(grouping.value());
            }
        }
    }

    if(layout.contains(u"State")) {
        p->m_pendingState = QByteArray::fromBase64(layout.value(u"State").toString().toUtf8());
        if(!p->m_pendingState.isEmpty() && p->m_settings->value<LibTreeRestoreState>()) {
            p->m_libraryTree->setLoading(true);
        }
    }
}

void LibraryTreeWidget::searchEvent(const QString& search)
{
    p->searchChanged(search);
}

void LibraryTreeWidget::contextMenuEvent(QContextMenuEvent* event)
{
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    const bool hasSelection = p->m_libraryTree->selectionModel()->hasSelection();

    if(hasSelection) {
        menu->addAction(p->m_playAction);
        p->m_trackSelection->addTrackPlaylistContextMenu(menu);
    }

    p->addGroupMenu(menu);
    menu->addSeparator();

    if(hasSelection) {
        if(auto* addQueueCmd = p->m_actionManager->command(Constants::Actions::AddToQueue)) {
            menu->addAction(addQueueCmd->action());
        }

        if(auto* removeQueueCmd = p->m_actionManager->command(Constants::Actions::RemoveFromQueue)) {
            menu->addAction(removeQueueCmd->action());
        }

        menu->addSeparator();
        p->addOpenMenu(menu);
        p->m_trackSelection->addTrackContextMenu(menu);
    }

    menu->popup(event->globalPos());
}

void LibraryTreeWidget::keyPressEvent(QKeyEvent* event)
{
    const auto key = event->key();
    if(key == Qt::Key_Enter || key == Qt::Key_Return) {
        p->handleDoubleClick(p->m_libraryTree->currentIndex());
    }

    FyWidget::keyPressEvent(event);
}
} // namespace Fooyin

#include "moc_librarytreewidget.cpp"
