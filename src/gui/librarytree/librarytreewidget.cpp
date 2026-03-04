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
#include "librarytreeconfigwidget.h"
#include "librarytreedelegate.h"
#include "librarytreegroupeditordialog.h"
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
#include <gui/configdialog.h>
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
#include <QComboBox>
#include <QDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QHeaderView>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenu>
#include <QTreeView>
#include <QVBoxLayout>

#include <algorithm>
#include <stack>

using namespace Qt::StringLiterals;

constexpr auto LibTreePlaylist = "␟LibTreePlaylist␟";

// Settings
constexpr auto LibTreeDoubleClickKey     = u"LibraryTree/DoubleClickBehaviour";
constexpr auto LibTreeMiddleClickKey     = u"LibraryTree/MiddleClickBehaviour";
constexpr auto LibTreePlaylistEnabledKey = u"LibraryTree/SelectionPlaylistEnabled";
constexpr auto LibTreeAutoSwitchKey      = u"LibraryTree/SelectionPlaylistAutoSwitch";
constexpr auto LibTreeAutoPlaylistKey    = u"LibraryTree/SelectionPlaylistName";
constexpr auto LibTreeScrollBarKey       = u"LibraryTree/Scrollbar";
constexpr auto LibTreeAltColoursKey      = u"LibraryTree/AlternatingColours";
constexpr auto LibTreeRowHeightKey       = u"LibraryTree/RowHeight";
constexpr auto LibTreeSendPlaybackKey    = u"LibraryTree/StartPlaybackOnSend";
constexpr auto LibTreeRestoreStateKey    = u"LibraryTree/RestoreState";
constexpr auto LibTreeKeepAliveKey       = u"LibraryTree/KeepAlive";
constexpr auto LibTreeAnimatedKey        = u"LibraryTree/Animated";
constexpr auto LibTreeHeaderKey          = u"LibraryTree/Header";

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
LibraryTreeWidget::LibraryTreeWidget(ActionManager* actionManager, PlaylistController* playlistController,
                                     LibraryTreeController* controller, Application* core, QWidget* parent)
    : FyWidget{parent}
    , m_actionManager{actionManager}
    , m_library{core->library()}
    , m_playlistHandler{playlistController->playlistHandler()}
    , m_playerController{playlistController->playerController()}
    , m_groupsRegistry{controller->groupRegistry()}
    , m_trackSelection{playlistController->selectionController()}
    , m_settings{core->settingsManager()}
    , m_resetThrottler{new SignalThrottler(this)}
    , m_layout{new QVBoxLayout(this)}
    , m_libraryTree{new LibraryTreeView(this)}
    , m_model{new LibraryTreeModel(core->libraryManager(), core->audioLoader(), m_settings, this)}
    , m_sortProxy{new LibraryTreeSortModel(this)}
    , m_widgetContext{new WidgetContext(this, Context{Id{"Fooyin.Context.LibraryTree."}.append(this->id())}, this)}
    , m_addToQueueAction{new QAction(tr("&Add to playback queue"), this)}
    , m_queueNextAction{new QAction(tr("&Queue to play next"), this)}
    , m_removeFromQueueAction{new QAction(tr("&Remove from playback queue"), this)}
    , m_playAction{new QAction(tr("&Play"), this)}
    , m_doubleClickAction{TrackAction::None}
    , m_middleClickAction{TrackAction::None}
    , m_updating{false}
    , m_playlist{nullptr}
{
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->addWidget(m_libraryTree);

    m_sortProxy->setSourceModel(m_model);
    m_libraryTree->setModel(m_sortProxy);
    m_libraryTree->setItemDelegate(new LibraryTreeDelegate(this));
    m_libraryTree->viewport()->installEventFilter(new ToolTipFilter(this));

    if(const auto initialGroup = m_groupsRegistry->itemByIndex(0)) {
        changeGrouping(initialGroup.value());
    }

    m_actionManager->addContextObject(m_widgetContext);

    m_model->setPlayState(playlistController->playerController()->playState());

    m_playAction->setStatusTip(tr("Start playback of the selected tracks"));
    QObject::connect(m_playAction, &QAction::triggered, this,
                     [this]() { handlePlayback(m_libraryTree->selectionModel()->selectedRows()); });

    m_addToQueueAction->setEnabled(false);
    m_actionManager->registerAction(m_addToQueueAction, Constants::Actions::AddToQueue, m_widgetContext->context());
    QObject::connect(m_addToQueueAction, &QAction::triggered, this, [this]() { queueSelectedTracks(false); });

    m_queueNextAction->setEnabled(false);
    m_actionManager->registerAction(m_queueNextAction, Constants::Actions::QueueNext, m_widgetContext->context());
    QObject::connect(m_queueNextAction, &QAction::triggered, this, [this]() { queueSelectedTracks(true); });

    m_removeFromQueueAction->setVisible(false);
    m_actionManager->registerAction(m_removeFromQueueAction, Constants::Actions::RemoveFromQueue,
                                    m_widgetContext->context());
    QObject::connect(m_removeFromQueueAction, &QAction::triggered, this, [this]() { dequeueSelectedTracks(); });

    setObjectName(LibraryTreeWidget::name());
    setFeature(FyWidget::Search);

    m_config = defaultConfig();
    applyConfig(m_config);

    setupConnections();
}

void LibraryTreeWidget::setupConnections()
{
    QObject::connect(m_resetThrottler, &SignalThrottler::triggered, this,
                     [this]() { m_model->reset(m_library->tracks()); });

    QObject::connect(m_model, &LibraryTreeModel::dataUpdated, m_libraryTree, &QTreeView::dataChanged);
    QObject::connect(m_model, &LibraryTreeModel::modelLoaded, this, [this]() { restoreState(m_pendingState); });

    QObject::connect(m_libraryTree, &LibraryTreeView::doubleClicked, this,
                     [this](const QModelIndex& index) { handleDoubleClick(index); });
    QObject::connect(m_libraryTree, &LibraryTreeView::middleClicked, this,
                     [this](const QModelIndex& index) { handleMiddleClick(index); });
    QObject::connect(m_libraryTree->selectionModel(), &QItemSelectionModel::selectionChanged, this,
                     [this](const QItemSelection& selected, const QItemSelection& deselected) {
                         selectionChanged(selected, deselected);
                     });
    QObject::connect(m_libraryTree->header(), &QHeaderView::customContextMenuRequested, this,
                     [this](const QPoint& pos) { setupHeaderContextMenu(pos); });
    QObject::connect(m_libraryTree, &QAbstractItemView::iconSizeChanged, this, [this](const QSize& size) {
        if(size.isValid() && m_config.iconSize != size) {
            m_config.iconSize = size;
            QMetaObject::invokeMethod(m_libraryTree->itemDelegate(), "sizeHintChanged", Q_ARG(QModelIndex, {}));
        }
    });
    QObject::connect(m_groupsRegistry, &LibraryTreeGroupRegistry::groupingChanged, this,
                     [this](const LibraryTreeGrouping& changedGrouping) {
                         if(m_grouping.id == changedGrouping.id) {
                             changeGrouping(changedGrouping);
                         }
                     });

    QObject::connect(m_library, &MusicLibrary::tracksLoaded, this, [this]() { reset(); });
    QObject::connect(m_library, &MusicLibrary::tracksAdded, this,
                     [this](const TrackList& tracks) { handleTracksAdded(tracks); });
    QObject::connect(m_library, &MusicLibrary::tracksScanned, m_model,
                     [this](int /*id*/, const TrackList& tracks) { handleTracksAdded(tracks); });
    QObject::connect(m_library, &MusicLibrary::tracksMetadataChanged, this,
                     [this](const TrackList& tracks) { handleTracksUpdated(tracks); });
    QObject::connect(m_library, &MusicLibrary::tracksUpdated, this,
                     [this](const TrackList& tracks) { m_model->refreshTracks(tracks); });
    QObject::connect(m_library, &MusicLibrary::tracksDeleted, m_model, &LibraryTreeModel::removeTracks);
    QObject::connect(m_library, &MusicLibrary::tracksSorted, this, [this]() { reset(); });

    QObject::connect(m_playerController, &PlayerController::playStateChanged, this,
                     [this](Player::PlayState state) { m_model->setPlayState(state); });
    QObject::connect(m_playerController, &PlayerController::playlistTrackChanged, this,
                     [this](const auto& track) { playlistTrackChanged(track); });
    QObject::connect(m_playlistHandler, &PlaylistHandler::activePlaylistChanged, this,
                     [this](auto* playlist) { activePlaylistChanged(playlist); });

    m_settings->subscribe<Settings::Gui::Theme>(m_model, &LibraryTreeModel::resetPalette);
    m_settings->subscribe<Settings::Gui::Style>(m_model, &LibraryTreeModel::resetPalette);

    m_settings->subscribe<Settings::Core::UseVariousForCompilations>(this, [this]() { reset(); });
}

void LibraryTreeWidget::reset() const
{
    m_resetThrottler->throttle();
}

void LibraryTreeWidget::changeGrouping(const LibraryTreeGrouping& newGrouping)
{
    if(std::exchange(m_grouping, newGrouping) != newGrouping) {
        m_model->changeGrouping(m_grouping);
        reset();
    }
}

void LibraryTreeWidget::activePlaylistChanged(Playlist* playlist) const
{
    if(!playlist || !m_playlist) {
        return;
    }

    if(playlist->id() != m_playlist->id()) {
        m_model->setPlayingPath({}, {});
    }
}

void LibraryTreeWidget::playlistTrackChanged(const PlaylistTrack& track)
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

void LibraryTreeWidget::addGroupMenu(QMenu* parent)
{
    auto* groupMenu = new QMenu(tr("Grouping"), parent);

    auto* treeGroups = new QActionGroup(groupMenu);

    const auto groups = m_groupsRegistry->items();
    for(const auto& group : groups) {
        auto* switchGroup = new QAction(group.name, groupMenu);
        QObject::connect(switchGroup, &QAction::triggered, this, [this, group]() { changeGrouping(group); });
        switchGroup->setCheckable(true);
        switchGroup->setChecked(m_grouping.id == group.id);
        groupMenu->addAction(switchGroup);
        treeGroups->addAction(switchGroup);
    }

    groupMenu->addSeparator();

    auto* manageGroupings = new QAction(tr("Manage groupings..."), groupMenu);
    QObject::connect(manageGroupings, &QAction::triggered, this, [this]() {
        auto* dialog = new LibraryTreeGroupEditorDialog(m_actionManager, m_groupsRegistry, this);
        dialog->open();
    });
    groupMenu->addAction(manageGroupings);

    parent->addMenu(groupMenu);
}

void LibraryTreeWidget::addOpenMenu(QMenu* menu)
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

    dir += "/"_L1 + parentDirs.join(u'/');
    const QFileInfo info{dir};
    if(info.exists() && info.isDir()) {
        auto* openFolder = new QAction(tr("Open folder"), this);
        QObject::connect(openFolder, &QAction::triggered, this, [dir]() { Utils::File::openDirectory(dir); });
        menu->addAction(openFolder);
    }
}

void LibraryTreeWidget::setScrollbarEnabled(bool enabled) const
{
    m_libraryTree->setVerticalScrollBarPolicy(enabled ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
}

void LibraryTreeWidget::setupHeaderContextMenu(const QPoint& pos)
{
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    addGroupMenu(menu);
    menu->popup(this->mapToGlobal(pos));
}

void LibraryTreeWidget::selectionChanged(const QItemSelection& selected, const QItemSelection& deselected) const
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
    m_queueNextAction->setEnabled(true);

    const auto queuedTracks = m_playerController->playbackQueue().tracks();
    const bool canDeque     = std::ranges::any_of(
        queuedTracks, [&trackIndexes](const PlaylistTrack& track) { return trackIndexes.contains(track.track); });
    m_removeFromQueueAction->setVisible(canDeque);

    if(m_config.playlistEnabled) {
        PlaylistAction::ActionOptions options{PlaylistAction::None};

        if(m_config.keepAlive) {
            options |= PlaylistAction::KeepActive;
        }
        if(m_config.autoSwitch) {
            options |= PlaylistAction::Switch;
        }

        m_trackSelection->executeAction(TrackAction::SendNewPlaylist, options, m_config.playlistName);
    }
}

void LibraryTreeWidget::queueSelectedTracks(bool next) const
{
    const QModelIndexList selectedIndexes = m_libraryTree->selectionModel()->selectedIndexes();
    if(selectedIndexes.empty()) {
        return;
    }

    m_trackSelection->executeAction(next ? TrackAction::QueueNext : TrackAction::AddToQueue);
    m_removeFromQueueAction->setVisible(true);
}

void LibraryTreeWidget::dequeueSelectedTracks() const
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

void LibraryTreeWidget::searchChanged(const QString& search)
{
    m_currentSearch = search;

    if(search.length() < 1) {
        m_filteredTracks.clear();
        m_model->reset(m_library->tracks());
        return;
    }

    Utils::asyncExec([search, tracks = m_library->tracks()]() {
        ScriptParser parser;
        return parser.filter(search, tracks);
    }).then(this, [this](const TrackList& filteredTracks) {
        m_filteredTracks = filteredTracks;
        m_model->reset(m_filteredTracks);
    });
}

void LibraryTreeWidget::handlePlayback(const QModelIndexList& indexes, int row)
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
        m_playerController->startPlayback(m_playlist);
    }
}

void LibraryTreeWidget::handlePlayTrack()
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

void LibraryTreeWidget::handleDoubleClick(const QModelIndex& index)
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

    if(m_config.autoSwitch) {
        options |= PlaylistAction::Switch;
    }
    if(m_config.sendPlayback) {
        options |= PlaylistAction::StartPlayback;
    }

    m_trackSelection->executeAction(m_doubleClickAction, options);
    m_removeFromQueueAction->setVisible(m_doubleClickAction == TrackAction::AddToQueue
                                        || m_doubleClickAction == TrackAction::SendToQueue);
}

void LibraryTreeWidget::handleMiddleClick(const QModelIndex& index) const
{
    if(!index.isValid()) {
        return;
    }

    PlaylistAction::ActionOptions options;

    if(m_config.autoSwitch) {
        options |= PlaylistAction::Switch;
    }
    if(m_config.sendPlayback) {
        options |= PlaylistAction::StartPlayback;
    }

    m_trackSelection->executeAction(m_middleClickAction, options);
    m_removeFromQueueAction->setVisible(m_middleClickAction == TrackAction::AddToQueue
                                        || m_middleClickAction == TrackAction::SendToQueue);
}

void LibraryTreeWidget::handleTracksAdded(const TrackList& tracks)
{
    if(tracks.empty()) {
        return;
    }

    if(!m_currentSearch.isEmpty()) {
        Utils::asyncExec([search = m_currentSearch, tracks]() {
            ScriptParser parser;
            return parser.filter(search, tracks);
        }).then(this, [this](const TrackList& filteredTracks) { m_model->addTracks(filteredTracks); });
    }
    else {
        m_model->addTracks(tracks);
    }
}

void LibraryTreeWidget::handleTracksUpdated(const TrackList& tracks)
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

    QObject::connect(
        m_model, &LibraryTreeModel::modelUpdated, this,
        [this, expandedTitles, selectedTitles]() { restoreSelection(expandedTitles, selectedTitles); },
        Qt::SingleShotConnection);

    m_model->updateTracks(tracks);
}

void LibraryTreeWidget::restoreSelection(const QStringList& expandedTitles, const QStringList& selectedTitles)
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

QByteArray LibraryTreeWidget::saveState() const
{
    QByteArray stateData;
    QDataStream stream{&stateData, QIODeviceBase::WriteOnly};
    stream.setVersion(QDataStream::Qt_6_0);

    const QModelIndex topIndex = m_libraryTree->indexAt({0, 0});
    const auto topKey          = topIndex.data(LibraryTreeItem::Key).toByteArray();
    stream << topKey;

    std::vector<QByteArray> keysToSave;
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

    stateData = qCompress(stateData, 9);
    return stateData;
}

void LibraryTreeWidget::restoreIndexState(const QByteArray& topKey, const std::vector<QByteArray>& keys,
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
                m_libraryTree->setAnimated(m_config.animated);
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

void LibraryTreeWidget::restoreState(const QByteArray& state)
{
    if(state.isEmpty() || !m_config.restoreState) {
        return;
    }

    // Disable animation during state restore to prevent visual artifacts
    m_libraryTree->setAnimated(false);

    QByteArray stateData = qUncompress(state);
    QDataStream stream{&stateData, QIODeviceBase::ReadOnly};
    stream.setVersion(QDataStream::Qt_6_0);

    QByteArray topKey;
    stream >> topKey;

    std::vector<QByteArray> keysToRestore;
    stream >> keysToRestore;

    restoreIndexState(topKey, keysToRestore);
}

LibraryTreeWidget::ConfigData LibraryTreeWidget::defaultConfig() const
{
    const auto* settings = m_settings;
    return {
        .doubleClickAction = settings->fileValue(LibTreeDoubleClickKey, 0).toInt(),
        .middleClickAction = settings->fileValue(LibTreeMiddleClickKey, 0).toInt(),
        .sendPlayback      = settings->fileValue(LibTreeSendPlaybackKey, true).toBool(),
        .playlistEnabled   = settings->fileValue(LibTreePlaylistEnabledKey, false).toBool(),
        .autoSwitch        = settings->fileValue(LibTreeAutoSwitchKey, true).toBool(),
        .keepAlive         = settings->fileValue(LibTreeKeepAliveKey, false).toBool(),
        .playlistName
        = settings->fileValue(LibTreeAutoPlaylistKey, LibraryTreeController::defaultPlaylistName()).toString(),
        .restoreState    = settings->fileValue(LibTreeRestoreStateKey, true).toBool(),
        .animated        = settings->fileValue(LibTreeAnimatedKey, true).toBool(),
        .showHeader      = settings->fileValue(LibTreeHeaderKey, true).toBool(),
        .showScrollbar   = settings->fileValue(LibTreeScrollBarKey, true).toBool(),
        .alternatingRows = settings->fileValue(LibTreeAltColoursKey, false).toBool(),
        .rowHeight       = settings->fileValue(LibTreeRowHeightKey, 0).toInt(),
        .iconSize        = settings->value<LibTreeIconSize>().toSize(),
    };
}

const LibraryTreeWidget::ConfigData& LibraryTreeWidget::currentConfig() const
{
    return m_config;
}

void LibraryTreeWidget::saveDefaults(const ConfigData& config) const
{
    auto* settings = m_settings;
    settings->fileSet(LibTreeDoubleClickKey, config.doubleClickAction);
    settings->fileSet(LibTreeMiddleClickKey, config.middleClickAction);
    settings->fileSet(LibTreeSendPlaybackKey, config.sendPlayback);
    settings->fileSet(LibTreePlaylistEnabledKey, config.playlistEnabled);
    settings->fileSet(LibTreeAutoSwitchKey, config.autoSwitch);
    settings->fileSet(LibTreeKeepAliveKey, config.keepAlive);
    settings->fileSet(LibTreeAutoPlaylistKey, config.playlistName);
    settings->fileSet(LibTreeRestoreStateKey, config.restoreState);
    settings->fileSet(LibTreeAnimatedKey, config.animated);
    settings->fileSet(LibTreeHeaderKey, config.showHeader);
    settings->fileSet(LibTreeScrollBarKey, config.showScrollbar);
    settings->fileSet(LibTreeAltColoursKey, config.alternatingRows);
    settings->fileSet(LibTreeRowHeightKey, config.rowHeight);
    settings->set<LibTreeIconSize>(config.iconSize);
}

void LibraryTreeWidget::applyConfig(const ConfigData& config)
{
    m_config = config;

    if(!m_config.iconSize.isValid()) {
        m_config.iconSize = defaultConfig().iconSize;
    }

    m_config.rowHeight = std::max(m_config.rowHeight, 0);

    m_doubleClickAction = static_cast<TrackAction>(m_config.doubleClickAction);
    m_middleClickAction = static_cast<TrackAction>(m_config.middleClickAction);

    m_libraryTree->setExpandsOnDoubleClick(m_doubleClickAction == TrackAction::None
                                           || m_doubleClickAction == TrackAction::Play);
    m_trackSelection->changePlaybackOnSend(m_widgetContext, m_config.sendPlayback);
    m_libraryTree->setAnimated(m_config.animated);
    m_libraryTree->setHeaderHidden(!m_config.showHeader);
    setScrollbarEnabled(m_config.showScrollbar);
    m_libraryTree->setAlternatingRowColors(m_config.alternatingRows);
    m_model->setRowHeight(m_config.rowHeight);
    m_libraryTree->setIconSize(m_config.iconSize);

    QMetaObject::invokeMethod(m_libraryTree->itemDelegate(), "sizeHintChanged", Q_ARG(QModelIndex, {}));
}

LibraryTreeWidget::ConfigData LibraryTreeWidget::configFromLayout(const QJsonObject& layout) const
{
    ConfigData config{defaultConfig()};

    if(layout.contains("DoubleClickAction"_L1)) {
        config.doubleClickAction = layout.value("DoubleClickAction"_L1).toInt();
    }
    if(layout.contains("MiddleClickAction"_L1)) {
        config.middleClickAction = layout.value("MiddleClickAction"_L1).toInt();
    }
    if(layout.contains("SendPlayback"_L1)) {
        config.sendPlayback = layout.value("SendPlayback"_L1).toBool();
    }
    if(layout.contains("PlaylistEnabled"_L1)) {
        config.playlistEnabled = layout.value("PlaylistEnabled"_L1).toBool();
    }
    if(layout.contains("AutoSwitch"_L1)) {
        config.autoSwitch = layout.value("AutoSwitch"_L1).toBool();
    }
    if(layout.contains("KeepAlive"_L1)) {
        config.keepAlive = layout.value("KeepAlive"_L1).toBool();
    }
    if(layout.contains("PlaylistName"_L1)) {
        config.playlistName = layout.value("PlaylistName"_L1).toString();
    }
    if(layout.contains("RestoreState"_L1)) {
        config.restoreState = layout.value("RestoreState"_L1).toBool();
    }
    if(layout.contains("Animated"_L1)) {
        config.animated = layout.value("Animated"_L1).toBool();
    }
    if(layout.contains("ShowHeader"_L1)) {
        config.showHeader = layout.value("ShowHeader"_L1).toBool();
    }
    if(layout.contains("ShowScrollbar"_L1)) {
        config.showScrollbar = layout.value("ShowScrollbar"_L1).toBool();
    }
    if(layout.contains("AlternatingRows"_L1)) {
        config.alternatingRows = layout.value("AlternatingRows"_L1).toBool();
    }
    if(layout.contains("RowHeight"_L1)) {
        config.rowHeight = layout.value("RowHeight"_L1).toInt();
    }
    if(layout.contains("IconWidth"_L1) && layout.contains("IconHeight"_L1)) {
        config.iconSize = {layout.value("IconWidth"_L1).toInt(), layout.value("IconHeight"_L1).toInt()};
    }

    if(!config.iconSize.isValid()) {
        config.iconSize = defaultConfig().iconSize;
    }

    config.rowHeight = std::max(config.rowHeight, 0);

    return config;
}

void LibraryTreeWidget::saveConfigToLayout(const ConfigData& config, QJsonObject& layout)
{
    layout["DoubleClickAction"_L1] = config.doubleClickAction;
    layout["MiddleClickAction"_L1] = config.middleClickAction;
    layout["SendPlayback"_L1]      = config.sendPlayback;
    layout["PlaylistEnabled"_L1]   = config.playlistEnabled;
    layout["AutoSwitch"_L1]        = config.autoSwitch;
    layout["KeepAlive"_L1]         = config.keepAlive;
    layout["PlaylistName"_L1]      = config.playlistName;
    layout["RestoreState"_L1]      = config.restoreState;
    layout["Animated"_L1]          = config.animated;
    layout["ShowHeader"_L1]        = config.showHeader;
    layout["ShowScrollbar"_L1]     = config.showScrollbar;
    layout["AlternatingRows"_L1]   = config.alternatingRows;
    layout["RowHeight"_L1]         = config.rowHeight;
    layout["IconWidth"_L1]         = config.iconSize.width();
    layout["IconHeight"_L1]        = config.iconSize.height();
}

void LibraryTreeWidget::openConfigDialog()
{
    showConfigDialog(new LibraryTreeConfigDialog(this, m_actionManager, m_groupsRegistry, this));
}

LibraryTreeWidget::~LibraryTreeWidget()
{
    QObject::disconnect(m_resetThrottler, nullptr, this, nullptr);
}

QString LibraryTreeWidget::name() const
{
    return tr("Library Tree");
}

QString LibraryTreeWidget::layoutName() const
{
    return u"LibraryTree"_s;
}

void LibraryTreeWidget::saveLayoutData(QJsonObject& layout)
{
    saveConfigToLayout(m_config, layout);
    layout["Grouping"_L1] = m_grouping.id;
    layout["State"_L1]    = QString::fromUtf8(saveState().toBase64());
}

void LibraryTreeWidget::loadLayoutData(const QJsonObject& layout)
{
    applyConfig(configFromLayout(layout));

    if(layout.contains("Grouping"_L1)) {
        if(const auto grouping = m_groupsRegistry->itemById(layout.value("Grouping"_L1).toInt())) {
            if(grouping->isValid()) {
                changeGrouping(grouping.value());
            }
        }
    }

    if(layout.contains("State"_L1)) {
        m_pendingState = QByteArray::fromBase64(layout.value("State"_L1).toString().toUtf8());
        if(!m_pendingState.isEmpty() && m_config.restoreState) {
            m_libraryTree->setLoading(true);
        }
    }
}

void LibraryTreeWidget::searchEvent(const QString& search)
{
    searchChanged(search);
}

void LibraryTreeWidget::contextMenuEvent(QContextMenuEvent* event)
{
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    const bool hasSelection = m_libraryTree->selectionModel()->hasSelection();

    if(hasSelection) {
        menu->addAction(m_playAction);
        m_trackSelection->addTrackPlaylistContextMenu(menu);
    }

    addGroupMenu(menu);
    addConfigureAction(menu, false);

    menu->addSeparator();

    if(hasSelection) {
        if(auto* addQueueCmd = m_actionManager->command(Constants::Actions::AddToQueue)) {
            menu->addAction(addQueueCmd->action());
        }
        if(auto* queueNextCmd = m_actionManager->command(Constants::Actions::QueueNext)) {
            menu->addAction(queueNextCmd->action());
        }
        if(auto* removeQueueCmd = m_actionManager->command(Constants::Actions::RemoveFromQueue)) {
            menu->addAction(removeQueueCmd->action());
        }

        menu->addSeparator();
        addOpenMenu(menu);
        m_trackSelection->addTrackContextMenu(menu);
    }

    menu->popup(event->globalPos());
}

void LibraryTreeWidget::keyPressEvent(QKeyEvent* event)
{
    const auto key = event->key();
    if(key == Qt::Key_Enter || key == Qt::Key_Return) {
        handleDoubleClick(m_libraryTree->currentIndex());
    }

    FyWidget::keyPressEvent(event);
}
} // namespace Fooyin

#include "moc_librarytreewidget.cpp"
