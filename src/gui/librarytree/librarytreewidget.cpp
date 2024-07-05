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
#include "librarytreedelegate.h"
#include "librarytreegroupregistry.h"
#include "librarytreemodel.h"
#include "librarytreeview.h"
#include "playlist/playlistcontroller.h"

#include <core/library/musiclibrary.h>
#include <core/library/trackfilter.h>
#include <core/player/playercontroller.h>
#include <core/player/playerdefs.h>
#include <core/playlist/playlisthandler.h>
#include <gui/guiconstants.h>
#include <gui/trackselectioncontroller.h>
#include <utils/actions/actionmanager.h>
#include <utils/actions/command.h>
#include <utils/actions/widgetcontext.h>
#include <utils/async.h>
#include <utils/tooltipfilter.h>

#include <QActionGroup>
#include <QContextMenuEvent>
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

struct LibraryTreeWidget::Private
{
    LibraryTreeWidget* m_self;

    ActionManager* m_actionManager;
    MusicLibrary* m_library;
    PlaylistHandler* m_playlistHandler;
    PlayerController* m_playerController;
    LibraryTreeGroupRegistry m_groupsRegistry;
    TrackSelectionController* m_trackSelection;
    SettingsManager* m_settings;

    LibraryTreeGrouping m_grouping;

    QVBoxLayout* m_layout;
    LibraryTreeView* m_libraryTree;
    LibraryTreeModel* m_model;
    LibraryTreeSortModel* m_sortProxy;

    WidgetContext* m_widgetContext;

    QAction* m_addToQueueAction;
    QAction* m_removeFromQueueAction;

    TrackAction m_doubleClickAction;
    TrackAction m_middleClickAction;

    QString m_prevSearch;
    TrackList m_prevSearchTracks;

    bool m_updating{false};
    QByteArray m_pendingState;

    Playlist* m_playlist{nullptr};
    std::map<int, QString> m_playlistGroups;

    Private(LibraryTreeWidget* self, ActionManager* actionManager, MusicLibrary* library,
            PlaylistController* playlistController, SettingsManager* settings)
        : m_self{self}
        , m_actionManager{actionManager}
        , m_library{library}
        , m_playlistHandler{playlistController->playlistHandler()}
        , m_playerController{playlistController->playerController()}
        , m_groupsRegistry{settings}
        , m_trackSelection{playlistController->selectionController()}
        , m_settings{settings}
        , m_layout{new QVBoxLayout(m_self)}
        , m_libraryTree{new LibraryTreeView(m_self)}
        , m_model{new LibraryTreeModel(m_self)}
        , m_sortProxy{new LibraryTreeSortModel(m_self)}
        , m_widgetContext{new WidgetContext(m_self, Context{Id{"Fooyin.Context.LibraryTree."}.append(m_self->id())},
                                            m_self)}
        , m_addToQueueAction{new QAction(tr("Add to Playback Queue"), m_self)}
        , m_removeFromQueueAction{new QAction(tr("Remove from Playback Queue"), m_self)}
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

        if(const auto initialGroup = m_groupsRegistry.itemByIndex(0)) {
            changeGrouping(initialGroup.value());
        }

        m_actionManager->addContextObject(m_widgetContext);
        m_trackSelection->changePlaybackOnSend(m_widgetContext, m_settings->value<LibTreeSendPlayback>());

        m_model->setFont(m_settings->value<LibTreeFont>());
        m_model->setColour(m_settings->value<LibTreeColour>());
        m_model->setRowHeight(m_settings->value<LibTreeRowHeight>());
        m_model->setPlayState(playlistController->playerController()->playState());

        m_addToQueueAction->setEnabled(false);
        m_actionManager->registerAction(m_addToQueueAction, Constants::Actions::AddToQueue, m_widgetContext->context());
        QObject::connect(m_addToQueueAction, &QAction::triggered, m_self, [this]() { queueSelectedTracks(); });

        m_removeFromQueueAction->setVisible(false);
        m_actionManager->registerAction(m_removeFromQueueAction, Constants::Actions::RemoveFromQueue,
                                        m_widgetContext->context());
        QObject::connect(m_removeFromQueueAction, &QAction::triggered, m_self, [this]() { dequeueSelectedTracks(); });

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
        m_settings->subscribe<LibTreeAltColours>(
            m_self, [this](bool enable) { m_libraryTree->setAlternatingRowColors(enable); });
        m_settings->subscribe<LibTreeFont>(m_self, [this](const QString& font) { m_model->setFont(font); });
        m_settings->subscribe<LibTreeColour>(m_self, [this](const QString& colour) { m_model->setColour(colour); });
        m_settings->subscribe<LibTreeRowHeight>(m_self, [this](const int height) {
            m_model->setRowHeight(height);
            QMetaObject::invokeMethod(m_libraryTree->itemDelegate(), "sizeHintChanged", Q_ARG(QModelIndex, {}));
        });
    }

    void reset() const
    {
        m_model->reset(m_library->tracks());
    }

    void changeGrouping(const LibraryTreeGrouping& newGrouping)
    {
        if(std::exchange(m_grouping, newGrouping) != newGrouping) {
            m_model->changeGrouping(m_grouping);
            reset();
        }
    }

    void activePlaylistChanged(Playlist* playlist) const
    {
        if(!playlist || !m_playlist) {
            return;
        }

        if(playlist->id() != m_playlist->id()) {
            m_model->setPlayingPath({}, {});
        }
    }

    void playlistTrackChanged(const PlaylistTrack& track)
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

    void addGroupMenu(QMenu* parent)
    {
        auto* groupMenu = new QMenu(QStringLiteral("Grouping"), parent);

        auto* treeGroups = new QActionGroup(groupMenu);

        const auto groups = m_groupsRegistry.items();
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

    void addPlayMenu(QMenu* menu)
    {
        auto* playAction = new QAction(tr("Play"), menu);
        QObject::connect(playAction, &QAction::triggered, m_self,
                         [this]() { handlePlayback(m_libraryTree->selectionModel()->selectedRows()); });
        menu->addAction(playAction);
    }

    void setScrollbarEnabled(bool enabled) const
    {
        m_libraryTree->setVerticalScrollBarPolicy(enabled ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
    }

    void setupHeaderContextMenu(const QPoint& pos)
    {
        auto* menu = new QMenu(m_self);
        menu->setAttribute(Qt::WA_DeleteOnClose);

        addGroupMenu(menu);
        menu->popup(m_self->mapToGlobal(pos));
    }

    void selectionChanged(const QItemSelection& selected, const QItemSelection& deselected) const
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
            PlaylistAction::ActionOptions options{None};

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

    void queueSelectedTracks() const
    {
        const QModelIndexList selectedIndexes = m_libraryTree->selectionModel()->selectedIndexes();
        if(selectedIndexes.empty()) {
            return;
        }

        m_trackSelection->executeAction(TrackAction::AddToQueue);
        m_removeFromQueueAction->setVisible(true);
    }

    void dequeueSelectedTracks() const
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

    void searchChanged(const QString& search)
    {
        const bool reset = m_prevSearch.length() > search.length();
        m_prevSearch     = search;

        if(search.isEmpty()) {
            m_prevSearchTracks.clear();
            m_model->reset(m_library->tracks());
            return;
        }

        const TrackList tracksToFilter
            = !reset && !m_prevSearchTracks.empty() ? m_prevSearchTracks : m_library->tracks();

        const auto tracks = Filter::filterTracks(tracksToFilter, search);

        m_prevSearchTracks = tracks;
        m_model->reset(tracks);
    }

    void handlePlayback(const QModelIndexList& indexes, int row = 0)
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

    void handlePlayTrack()
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

    void handleDoubleClick(const QModelIndex& index)
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

    void handleMiddleClick(const QModelIndex& index) const
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

    void handleTracksAdded(const TrackList& tracks) const
    {
        if(tracks.empty()) {
            return;
        }

        if(!m_prevSearch.isEmpty()) {
            const auto filteredTracks = Filter::filterTracks(tracks, m_prevSearch);
            m_model->addTracks(filteredTracks);
        }
        else {
            m_model->addTracks(tracks);
        }
    }

    void handleTracksUpdated(const TrackList& tracks)
    {
        if(tracks.empty()) {
            return;
        }

        m_updating = true;
        m_libraryTree->setUpdatesEnabled(false);

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

    void restoreSelection(const QStringList& expandedTitles, const QStringList& selectedTitles)
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
        m_libraryTree->setUpdatesEnabled(true);
        m_updating = false;
    }

    [[nodiscard]] QByteArray saveState() const
    {
        QByteArray data;
        QDataStream stream{&data, QIODeviceBase::WriteOnly};
        stream.setVersion(QDataStream::Qt_6_0);

        const QModelIndex topIndex = m_libraryTree->indexAt({0, 0});
        stream << topIndex.data(LibraryTreeItem::Key).toString();

        QStringList keysToSave;
        std::stack<QModelIndex> indexes;
        indexes.emplace();

        while(!indexes.empty()) {
            const QModelIndex index = indexes.top();
            indexes.pop();

            if(m_libraryTree->isExpanded(index) || !index.isValid()) {
                if(index.isValid()) {
                    keysToSave.emplace_back(index.data(LibraryTreeItem::Key).toString());
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

    void restoreIndexState(const QString& topKey, const QStringList& keys, int currentIndex = 0)
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

        const QString& key = keys.at(currentIndex);
        if(const auto index = m_model->indexForKey(key); index.isValid()) {
            m_libraryTree->expand(m_sortProxy->mapFromSource(index));
            QMetaObject::invokeMethod(
                m_libraryTree,
                [this, topKey, keys, currentIndex]() { restoreIndexState(topKey, keys, currentIndex + 1); },
                Qt::QueuedConnection);
        }
        else {
            restoreIndexState(topKey, keys, currentIndex + 1);
        }
    }

    void restoreState(const QByteArray& state)
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

        QString topKey;
        stream >> topKey;

        QStringList keysToRestore;
        stream >> keysToRestore;

        restoreIndexState(topKey, keysToRestore);
    }
};

LibraryTreeWidget::LibraryTreeWidget(ActionManager* actionManager, MusicLibrary* library,
                                     PlaylistController* playlistController, SettingsManager* settings, QWidget* parent)
    : FyWidget{parent}
    , p{std::make_unique<Private>(this, actionManager, library, playlistController, settings)}
{
    setObjectName(LibraryTreeWidget::name());

    setFeature(FyWidget::Search);

    QObject::connect(p->m_model, &LibraryTreeModel::dataUpdated, p->m_libraryTree, &QTreeView::dataChanged);
    QObject::connect(p->m_model, &LibraryTreeModel::modelLoaded, this,
                     [this]() { p->restoreState(p->m_pendingState); });
    QObject::connect(p->m_libraryTree, &LibraryTreeView::doubleClicked, this,
                     [this](const QModelIndex& index) { p->handleDoubleClick(index); });
    QObject::connect(p->m_libraryTree, &LibraryTreeView::middleClicked, this,
                     [this](const QModelIndex& index) { p->handleMiddleClick(index); });
    QObject::connect(p->m_libraryTree->selectionModel(), &QItemSelectionModel::selectionChanged, this,
                     [this](const QItemSelection& selected, const QItemSelection& deselected) {
                         p->selectionChanged(selected, deselected);
                     });
    QObject::connect(p->m_libraryTree->header(), &QHeaderView::customContextMenuRequested, this,
                     [this](const QPoint& pos) { p->setupHeaderContextMenu(pos); });
    QObject::connect(&p->m_groupsRegistry, &LibraryTreeGroupRegistry::groupingChanged, this,
                     [this](const LibraryTreeGrouping& changedGrouping) {
                         if(p->m_grouping.id == changedGrouping.id) {
                             p->changeGrouping(changedGrouping);
                         }
                     });

    QObject::connect(library, &MusicLibrary::tracksLoaded, this, [this]() { p->reset(); });
    QObject::connect(library, &MusicLibrary::tracksAdded, this,
                     [this](const TrackList& tracks) { p->handleTracksAdded(tracks); });
    QObject::connect(library, &MusicLibrary::tracksScanned, p->m_model,
                     [this](int /*id*/, const TrackList& tracks) { p->handleTracksAdded(tracks); });
    QObject::connect(library, &MusicLibrary::tracksUpdated, this,
                     [this](const TrackList& tracks) { p->handleTracksUpdated(tracks); });
    QObject::connect(library, &MusicLibrary::tracksPlayed, this,
                     [this](const TrackList& tracks) { p->m_model->refreshTracks(tracks); });
    QObject::connect(library, &MusicLibrary::tracksDeleted, p->m_model, &LibraryTreeModel::removeTracks);
    QObject::connect(library, &MusicLibrary::tracksSorted, this, [this]() { p->reset(); });

    QObject::connect(p->m_playerController, &PlayerController::playStateChanged, this,
                     [this](PlayState state) { p->m_model->setPlayState(state); });
    QObject::connect(p->m_playerController, &PlayerController::playlistTrackChanged, this,
                     [this](const auto& track) { p->playlistTrackChanged(track); });
    QObject::connect(p->m_playlistHandler, &PlaylistHandler::activePlaylistChanged, this,
                     [this](auto* playlist) { p->activePlaylistChanged(playlist); });
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
    layout[QStringLiteral("Grouping")] = p->m_grouping.id;
    layout[QStringLiteral("State")]    = QString::fromUtf8(p->saveState().toBase64());
}

void LibraryTreeWidget::loadLayoutData(const QJsonObject& layout)
{
    if(layout.contains(QStringLiteral("Grouping"))) {
        if(const auto grouping = p->m_groupsRegistry.itemById(layout.value(QStringLiteral("Grouping")).toInt())) {
            if(grouping->isValid()) {
                p->changeGrouping(grouping.value());
            }
        }
    }

    if(layout.contains(QStringLiteral("State"))) {
        p->m_pendingState = QByteArray::fromBase64(layout.value(QStringLiteral("State")).toString().toUtf8());
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
        p->addPlayMenu(menu);
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
