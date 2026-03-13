/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <LukeT1@proton.me>
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

#include "playlistwidgetsession.h"

#include "playlist/playlistinteractor.h"
#include "playlistcontroller.h"
#include "playlistview.h"

#include <core/library/musiclibrary.h>
#include <core/player/playercontroller.h>
#include <core/playlist/playlisthandler.h>
#include <core/scripting/scriptparser.h>
#include <gui/guiconstants.h>
#include <gui/guisettings.h>
#include <utils/actions/actioncontainer.h>
#include <utils/actions/actionmanager.h>
#include <utils/actions/command.h>
#include <utils/async.h>
#include <utils/modelutils.h>
#include <utils/signalthrottler.h>

#include <QAction>
#include <QItemSelectionModel>
#include <QScrollBar>

#include <ranges>
#include <stack>

namespace Fooyin {
void PlaylistWidgetSession::setupConnections(PlaylistWidgetSessionHost& /*host*/) { }

void PlaylistWidgetSession::setupActions(PlaylistWidgetSessionHost& /*host*/, ActionContainer* /*editMenu*/,
                                         const QStringList& /*editCategory*/)
{ }

void PlaylistWidgetSession::selectionChanged(PlaylistWidgetSessionHost& host)
{
    const auto modeCaps = capabilities();

    if(modeCaps.editablePlaylist && !host.playlistController()->currentPlaylist()) {
        return;
    }

    TrackList tracks;
    std::set<int> trackIndexes;
    std::set<Track> selectedTracks;
    PlaylistTrack firstTrack;

    QModelIndexList indexes = filterSelectedIndexes(host.playlistView());
    std::ranges::sort(indexes, Utils::sortModelIndexes);

    for(const QModelIndex& index : std::as_const(indexes)) {
        if(index.data(PlaylistItem::Type).toInt() != PlaylistItem::Track) {
            continue;
        }

        const auto track = index.data(PlaylistItem::Role::ItemData).value<PlaylistTrack>();
        tracks.push_back(track.track);
        selectedTracks.emplace(track.track);
        const int trackIndex = index.data(PlaylistItem::Role::Index).toInt();
        trackIndexes.emplace(trackIndex);

        if(modeCaps.playlistBackedSelection) {
            if(!firstTrack.isValid() || trackIndex < firstTrack.indexInPlaylist) {
                firstTrack = track;
            }
        }
    }

    const auto selection
        = this->selection(host.playlistController()->currentPlaylist(), tracks, trackIndexes, firstTrack);
    host.selectionController()->changeSelectedTracks(host.playlistContext(), selection);

    if(tracks.empty()) {
        if(auto* action = removeTrackAction()) {
            action->setEnabled(false);
        }
        if(auto* action = addToQueueAction()) {
            action->setEnabled(false);
        }
        if(auto* action = queueNextAction()) {
            action->setEnabled(false);
        }
        if(auto* action = removeFromQueueAction()) {
            action->setVisible(false);
        }
        return;
    }

    updateSelectionState(host, trackIndexes, selectedTracks, firstTrack);
}

void PlaylistWidgetSession::requestPlaylistFocus(PlaylistWidgetSessionHost& /*host*/) { }

void PlaylistWidgetSession::changePlaylist(PlaylistWidgetSessionHost& /*host*/, Playlist* /*prevPlaylist*/,
                                           Playlist* /*playlist*/)
{ }

void PlaylistWidgetSession::destroy(PlaylistWidgetSessionHost& /*host*/) { }

void PlaylistWidgetSession::handleAboutToBeReset(PlaylistWidgetSessionHost& /*host*/) { }

void PlaylistWidgetSession::resetTree(PlaylistWidgetSessionHost& /*host*/) { }

void PlaylistWidgetSession::handleTracksChanged(PlaylistWidgetSessionHost& /*host*/,
                                                const std::vector<int>& /*indexes*/, bool /*allNew*/)
{ }

void PlaylistWidgetSession::searchEvent(PlaylistWidgetSessionHost& host, const QString& search)
{
    handleSearchChanged(host, search);

    if(search.isEmpty()) {
        clearSearch();
        host.resetModelThrottled();
        return;
    }

    setSearch(search);

    const PlaylistTrackList sourceTracks = searchSourceTracks(host.playlistController(), host.musicLibrary());

    if(sourceTracks.empty()) {
        setFilteredTracks({});
        host.resetModelThrottled();
        return;
    }

    auto* receiver = host.sessionWidget();
    Utils::asyncExec([search, sourceTracks]() {
        ScriptParser parser;
        return parser.filter(search, sourceTracks);
    }).then(receiver, [this, search, hostPtr = &host](const PlaylistTrackList& filteredTracks) {
        if(this->search() != search) {
            return;
        }

        setFilteredTracks(filteredTracks);
        hostPtr->resetModelThrottled();
    });
}

void PlaylistWidgetSession::handleSearchChanged(PlaylistWidgetSessionHost& /*host*/, const QString& /*search*/) { }

void PlaylistWidgetSession::finalise(PlaylistWidgetSessionHost& /*host*/) { }

void PlaylistWidgetSession::applyReadOnlyState(PlaylistWidgetSessionHost& /*host*/, bool /*readOnly*/) { }

void PlaylistWidgetSession::deferFollowCurrentTrack(PlaylistWidgetSessionHost& /*host*/) { }

void PlaylistWidgetSession::handleRestoredState(PlaylistWidgetSessionHost& /*host*/) { }

void PlaylistWidgetSession::updateContextMenuState(PlaylistWidgetSessionHost& /*host*/,
                                                   const QModelIndexList& /*selected*/,
                                                   PlaylistWidget::ContextMenuState& /*state*/)
{ }

void PlaylistWidgetSession::updateSelectionState(PlaylistWidgetSessionHost& host, const std::set<int>& trackIndexes,
                                                 const std::set<Track>& selectedTracks,
                                                 const PlaylistTrack& /*firstTrack*/)
{
    if(auto* action = removeFromQueueAction()) {
        action->setVisible(canDequeue(host.playerController(), host.playlistController()->currentPlaylist(),
                                      trackIndexes, selectedTracks));
    }
}

void PlaylistWidgetSession::queueSelectedTracks(PlaylistWidgetSessionHost& /*host*/, bool /*next*/, bool /*send*/) { }

void PlaylistWidgetSession::dequeueSelectedTracks(PlaylistWidgetSessionHost& /*host*/) { }

void PlaylistWidgetSession::removeDuplicates(PlaylistWidgetSessionHost& /*host*/) { }

void PlaylistWidgetSession::removeDeadTracks(PlaylistWidgetSessionHost& /*host*/) { }

void PlaylistWidgetSession::scanDroppedTracks(PlaylistWidgetSessionHost& /*host*/, const QList<QUrl>& /*urls*/,
                                              int /*index*/)
{ }

void PlaylistWidgetSession::tracksInserted(PlaylistWidgetSessionHost& /*host*/, const TrackGroups& /*tracks*/) { }

void PlaylistWidgetSession::tracksRemoved(PlaylistWidgetSessionHost& /*host*/, const std::vector<int>& /*indexes*/) { }

void PlaylistWidgetSession::tracksMoved(PlaylistWidgetSessionHost& /*host*/, const MoveOperation& /*operation*/) { }

void PlaylistWidgetSession::clearTracks(PlaylistWidgetSessionHost& /*host*/) { }

void PlaylistWidgetSession::cutTracks(PlaylistWidgetSessionHost& /*host*/) { }

void PlaylistWidgetSession::copyTracks(PlaylistWidgetSessionHost& /*host*/) { }

void PlaylistWidgetSession::pasteTracks(PlaylistWidgetSessionHost& /*host*/) { }

void PlaylistWidgetSession::playlistTracksAdded(PlaylistWidgetSessionHost& /*host*/, const TrackList& /*tracks*/,
                                                int /*index*/)
{ }

void PlaylistWidgetSession::cropSelection(PlaylistWidgetSessionHost& /*host*/) { }

void PlaylistWidgetSession::sortTracks(PlaylistWidgetSessionHost& /*host*/, const QString& /*script*/) { }

void PlaylistWidgetSession::sortColumn(PlaylistWidgetSessionHost& /*host*/, int /*column*/, Qt::SortOrder /*order*/) { }

QAction* PlaylistWidgetSession::cropAction() const
{
    return nullptr;
}

QAction* PlaylistWidgetSession::stopAfterAction() const
{
    return nullptr;
}

QAction* PlaylistWidgetSession::cutAction() const
{
    return nullptr;
}

QAction* PlaylistWidgetSession::copyAction() const
{
    return nullptr;
}

QAction* PlaylistWidgetSession::pasteAction() const
{
    return nullptr;
}

QAction* PlaylistWidgetSession::removeTrackAction() const
{
    return nullptr;
}

QAction* PlaylistWidgetSession::addToQueueAction() const
{
    return nullptr;
}

QAction* PlaylistWidgetSession::queueNextAction() const
{
    return nullptr;
}

QAction* PlaylistWidgetSession::removeFromQueueAction() const
{
    return nullptr;
}

bool PlaylistWidgetSession::isSorting() const
{
    return m_state.sorting;
}

bool PlaylistWidgetSession::isSortingColumn() const
{
    return m_state.sortingColumn;
}

bool PlaylistWidgetSession::hasSearch() const
{
    return !m_state.search.isEmpty();
}

const QString& PlaylistWidgetSession::search() const
{
    return m_state.search;
}

const PlaylistTrackList& PlaylistWidgetSession::filteredTracks() const
{
    return m_state.filteredTracks;
}

void PlaylistWidgetSession::setSorting(bool sorting)
{
    m_state.sorting = sorting;
}

void PlaylistWidgetSession::setSortingColumn(bool sortingColumn)
{
    m_state.sortingColumn = sortingColumn;
}

void PlaylistWidgetSession::resetSortState(bool force)
{
    if(force) {
        m_state.sorting = false;
    }
}

void PlaylistWidgetSession::setSearch(QString search)
{
    m_state.search = std::move(search);
}

void PlaylistWidgetSession::clearSearch()
{
    m_state.search.clear();
    m_state.filteredTracks.clear();
}

void PlaylistWidgetSession::setFilteredTracks(PlaylistTrackList filteredTracks)
{
    m_state.filteredTracks = std::move(filteredTracks);
}

TrackList PlaylistWidgetSession::getTracks(const QModelIndexList& indexes)
{
    TrackList tracks;

    for(const QModelIndex& index : indexes) {
        if(index.isValid() && index.data(PlaylistItem::Type).toInt() == PlaylistItem::Track) {
            auto track = index.data(PlaylistItem::ItemData).value<PlaylistTrack>();
            if(track.isValid()) {
                tracks.push_back(track.track);
            }
        }
    }

    return tracks;
}

void PlaylistWidgetSession::getAllTrackIndexes(QAbstractItemModel* model, const QModelIndex& parent,
                                               QModelIndexList& indexList)
{
    const int rowCount = model->rowCount(parent);
    for(int row{0}; row < rowCount; ++row) {
        const QModelIndex index = model->index(row, 0, parent);
        if(index.data(PlaylistItem::Role::Type).toInt() == PlaylistItem::Track) {
            indexList.append(index);
        }
        else if(model->hasChildren(index)) {
            getAllTrackIndexes(model, index, indexList);
        }
    }
}

QModelIndexList PlaylistWidgetSession::filterSelectedIndexes(const QAbstractItemView* view)
{
    if(!view || !view->selectionModel()) {
        return {};
    }

    return view->selectionModel()->selectedRows();
}

TrackSelection PlaylistWidgetSession::makeTrackSelection(const TrackList& tracks,
                                                         const std::vector<int>& playlistIndexes,
                                                         const Playlist* playlist)
{
    TrackSelection selection;
    selection.tracks          = tracks;
    selection.playlistIndexes = playlistIndexes;
    selection.playlistBacked  = playlist && !playlistIndexes.empty();

    if(playlist) {
        selection.playlistId = playlist->id();
    }

    if(!playlistIndexes.empty()) {
        selection.primaryPlaylistIndex = playlistIndexes.front();
    }

    return selection;
}

TrackList PlaylistWidgetSession::getAllTracks(QAbstractItemModel* model, const QModelIndexList& indexes)
{
    TrackList tracks;

    std::stack<QModelIndex> indexStack;
    for(const QModelIndex& index : indexes | std::views::reverse) {
        indexStack.push(index);
    }

    while(!indexStack.empty()) {
        const QModelIndex currentIndex = indexStack.top();
        indexStack.pop();

        const int rowCount = model->rowCount(currentIndex);
        if(rowCount == 0 && currentIndex.data(PlaylistItem::Role::Type).toInt() == PlaylistItem::Track) {
            auto track = currentIndex.data(PlaylistItem::ItemData).value<PlaylistTrack>();
            if(track.isValid()) {
                tracks.push_back(track.track);
            }
        }
        else {
            for(int row{rowCount - 1}; row >= 0; --row) {
                const QModelIndex childIndex = model->index(row, 0, currentIndex);
                indexStack.push(childIndex);
            }
        }
    }
    return tracks;
}

PlaylistViewState PlaylistWidgetSession::playlistViewState(const PlaylistWidgetSessionHost& host, Playlist* playlist)
{
    if(!playlist) {
        return {};
    }

    PlaylistViewState state;

    if(playlist->trackCount() > 0) {
        QModelIndex topTrackIndex = host.playlistView()->findIndexAt({0, 0}, true);

        if(topTrackIndex.isValid()) {
            topTrackIndex = topTrackIndex.siblingAtColumn(0);
            while(host.playlistModel()->hasChildren(topTrackIndex)) {
                topTrackIndex = host.playlistView()->indexBelow(topTrackIndex);
            }

            state.topIndex  = topTrackIndex.data(PlaylistItem::Index).toInt();
            state.scrollPos = host.playlistView()->verticalScrollBar()->value();
        }
    }

    return state;
}

void PlaylistWidgetSession::savePlaylistViewState(const PlaylistWidgetSessionHost& host, Playlist* playlist,
                                                  bool requireVisible)
{
    if(!playlist || (requireVisible && !host.sessionVisible())) {
        return;
    }

    const auto state = playlistViewState(host, playlist);
    // Don't save state if still populating model
    if(playlist->trackCount() == 0 || (playlist->trackCount() > 0 && state.topIndex >= 0)) {
        host.playlistController()->savePlaylistState(playlist, state);
    }
}

void PlaylistWidgetSession::restorePlaylistViewState(PlaylistWidgetSessionHost& host, Playlist* playlist)
{
    if(!playlist) {
        return;
    }

    auto restoreDefaultState = [&host]() {
        host.playlistView()->playlistReset();
        host.playlistView()->scrollToTop();
        host.sessionHandleRestoredState();
    };

    if(playlist->trackCount() == 0) {
        restoreDefaultState();
        return;
    }

    const auto state = host.playlistController()->playlistState(playlist);

    if(!state) {
        restoreDefaultState();
        return;
    }

    auto loadState = [hostPtr = &host, state]() {
        const auto index = hostPtr->playlistModel()->indexAtPlaylistIndex(state->topIndex, false);

        if(!index.isValid()) {
            return false;
        }

        hostPtr->playlistView()->scrollTo(index, PlaylistView::PositionAtTop);
        hostPtr->playlistView()->verticalScrollBar()->setValue(state->scrollPos);
        hostPtr->playlistView()->playlistReset();
        hostPtr->sessionHandleRestoredState();
        return true;
    };

    if(!loadState()) {
        host.setDelayedStateLoad(QObject::connect(
            host.playlistModel(), &PlaylistModel::playlistLoaded, host.sessionWidget(), [loadState]() { loadState(); },
            Qt::SingleShotConnection));
    }
}

std::vector<int> PlaylistWidgetSession::selectedPlaylistIndexes(const PlaylistWidgetSessionHost& host)
{
    std::vector<int> indexes;

    const auto selected = filterSelectedIndexes(host.playlistView());
    for(const QModelIndex& index : selected) {
        indexes.emplace_back(index.data(PlaylistItem::Index).toInt());
    }

    return indexes;
}

void PlaylistWidgetSession::restoreSelectedPlaylistIndexes(const PlaylistWidgetSessionHost& host,
                                                           const std::vector<int>& indexes)
{
    if(indexes.empty()) {
        return;
    }

    const int columnCount = static_cast<int>(host.layoutState().columns.size());

    QItemSelection indexesToSelect;
    indexesToSelect.reserve(static_cast<qsizetype>(indexes.size()));

    for(const int selectedIndex : indexes) {
        const QModelIndex index = host.playlistModel()->indexAtPlaylistIndex(selectedIndex, true);
        if(index.isValid()) {
            const QModelIndex last = index.siblingAtColumn(columnCount - 1);
            indexesToSelect.append({index, last.isValid() ? last : index});
        }
    }

    host.playlistView()->selectionModel()->select(indexesToSelect, QItemSelectionModel::ClearAndSelect);
}
} // namespace Fooyin
