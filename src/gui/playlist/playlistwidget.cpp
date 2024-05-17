/*
 * Fooyin
 * Copyright © 2022, Luke Taylor <LukeT1@proton.me>
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

#include "playlistwidget.h"

#include "internalguisettings.h"
#include "playlist/playlistinteractor.h"
#include "playlist/playlistscriptregistry.h"
#include "playlistcommands.h"
#include "playlistcontroller.h"
#include "playlistdelegate.h"
#include "playlistview.h"
#include "playlistwidget_p.h"

#include <core/library/musiclibrary.h>
#include <core/library/tracksort.h>
#include <core/player/playercontroller.h>
#include <core/playlist/playlisthandler.h>
#include <gui/guiconstants.h>
#include <gui/guisettings.h>
#include <gui/trackselectioncontroller.h>
#include <utils/actions/actioncontainer.h>
#include <utils/actions/actionmanager.h>
#include <utils/actions/command.h>
#include <utils/async.h>
#include <utils/settings/settingsdialogcontroller.h>
#include <utils/tooltipfilter.h>
#include <utils/utils.h>
#include <utils/widgets/autoheaderview.h>

#include <QActionGroup>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonObject>
#include <QKeyEvent>
#include <QMenu>
#include <QScrollBar>

#include <stack>

namespace {
Fooyin::TrackList getAllTracks(QAbstractItemModel* model, const QModelIndexList& indexes)
{
    Fooyin::TrackList tracks;

    std::stack<QModelIndex> indexStack;
    for(const QModelIndex& index : indexes | std::views::reverse) {
        indexStack.push(index);
    }

    while(!indexStack.empty()) {
        const QModelIndex currentIndex = indexStack.top();
        indexStack.pop();

        while(model->canFetchMore(currentIndex)) {
            model->fetchMore(currentIndex);
        }

        const int rowCount = model->rowCount(currentIndex);
        if(rowCount == 0
           && currentIndex.data(Fooyin::PlaylistItem::Role::Type).toInt() == Fooyin::PlaylistItem::Track) {
            tracks.push_back(currentIndex.data(Fooyin::PlaylistItem::Role::ItemData).value<Fooyin::Track>());
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
} // namespace

namespace Fooyin {
using namespace Settings::Gui::Internal;

PlaylistWidgetPrivate::PlaylistWidgetPrivate(PlaylistWidget* self_, ActionManager* actionManager_,
                                             PlaylistInteractor* playlistInteractor_, SettingsManager* settings_)
    : self{self_}
    , actionManager{actionManager_}
    , playlistInteractor{playlistInteractor_}
    , playlistController{playlistInteractor->controller()}
    , playerController{playlistController->playerController()}
    , selectionController{playlistController->selectionController()}
    , library{playlistInteractor_->library()}
    , settings{settings_}
    , settingsDialog{settings->settingsDialog()}
    , columnRegistry{settings}
    , presetRegistry{settings}
    , sortRegistry{settings}
    , layout{new QHBoxLayout(self)}
    , model{new PlaylistModel(library, playerController, settings, self)}
    , playlistView{new PlaylistView(self)}
    , header{playlistView->header()}
    , singleMode{false}
    , playlistContext{new WidgetContext(self, Context{Constants::Context::Playlist}, self)}
    , removeTrackAction{new QAction(tr("Remove"), self)}
    , addToQueueAction{new QAction(tr("Add to Playback Queue"), self)}
    , removeFromQueueAction{new QAction(tr("Remove from Playback Queue"), self)}
    , m_sorting{false}
    , m_sortingColumn{false}
{
    layout->setContentsMargins(0, 0, 0, 0);

    header->setStretchEnabled(true);
    header->setSectionsMovable(true);
    header->setFirstSectionMovable(true);
    header->setContextMenuPolicy(Qt::CustomContextMenu);

    playlistView->setModel(model);
    playlistView->setItemDelegate(new PlaylistDelegate(self));
    playlistView->viewport()->installEventFilter(new ToolTipFilter(self));

    layout->addWidget(playlistView);

    setHeaderHidden(!settings->value<PlaylistHeader>());
    setScrollbarHidden(settings->value<PlaylistScrollBar>());

    setupConnections();
    setupActions();

    model->playingTrackChanged(playlistController->currentTrack());
    model->playStateChanged(playlistController->playState());
}

void PlaylistWidgetPrivate::setupConnections()
{
    QObject::connect(header, &QHeaderView::sortIndicatorChanged, this, &PlaylistWidgetPrivate::sortColumn);
    QObject::connect(header, &QHeaderView::customContextMenuRequested, this,
                     &PlaylistWidgetPrivate::customHeaderMenuRequested);
    QObject::connect(playlistView->selectionModel(), &QItemSelectionModel::selectionChanged, this,
                     &PlaylistWidgetPrivate::selectionChanged);
    QObject::connect(playlistView, &QAbstractItemView::doubleClicked, this, &PlaylistWidgetPrivate::doubleClicked);

    QObject::connect(model, &QAbstractItemModel::modelAboutToBeReset, playlistView, &QAbstractItemView::clearSelection);
    QObject::connect(model, &PlaylistModel::playlistTracksChanged, this, &PlaylistWidgetPrivate::trackIndexesChanged);
    QObject::connect(model, &PlaylistModel::filesDropped, this, &PlaylistWidgetPrivate::scanDroppedTracks);
    QObject::connect(model, &PlaylistModel::tracksInserted, this, &PlaylistWidgetPrivate::tracksInserted);
    QObject::connect(model, &PlaylistModel::tracksMoved, this, &PlaylistWidgetPrivate::tracksMoved);
    QObject::connect(model, &QAbstractItemModel::modelReset, this, &PlaylistWidgetPrivate::resetTree);

    QObject::connect(playlistController->playlistHandler(), &PlaylistHandler::activePlaylistChanged, this,
                     [this]() { model->playingTrackChanged(playerController->currentPlaylistTrack()); });
    QObject::connect(playlistController, &PlaylistController::currentPlaylistTracksChanged, this,
                     &PlaylistWidgetPrivate::handleTracksChanged);
    QObject::connect(playlistController, &PlaylistController::currentPlaylistQueueChanged, this,
                     [this](const std::vector<int>& indexes) { handleTracksChanged(indexes, false); });
    QObject::connect(playlistController, &PlaylistController::currentPlaylistChanged, this,
                     [this](Playlist* prevPlaylist, Playlist* playlist) { changePlaylist(prevPlaylist, playlist); });
    QObject::connect(playlistController, &PlaylistController::playlistsLoaded, this,
                     [this]() { changePlaylist(nullptr, playlistController->currentPlaylist()); });
    QObject::connect(playlistController, &PlaylistController::playingTrackChanged, this,
                     &PlaylistWidgetPrivate::handlePlayingTrackChanged);
    QObject::connect(playlistController, &PlaylistController::playStateChanged, model,
                     &PlaylistModel::playStateChanged);
    QObject::connect(playlistController, &PlaylistController::currentPlaylistTracksAdded, this,
                     &PlaylistWidgetPrivate::playlistTracksAdded);
    QObject::connect(&presetRegistry, &PresetRegistry::presetChanged, this, &PlaylistWidgetPrivate::onPresetChanged);

    settings->subscribe<PlaylistHeader>(this, [this](bool show) { setHeaderHidden(!show); });
    settings->subscribe<PlaylistScrollBar>(this, &PlaylistWidgetPrivate::setScrollbarHidden);
    settings->subscribe<PlaylistCurrentPreset>(
        this, [this](int presetId) { changePreset(presetRegistry.itemById(presetId)); });
}

void PlaylistWidgetPrivate::setupActions()
{
    actionManager->addContextObject(playlistContext);

    auto* editMenu = actionManager->actionContainer(Constants::Menus::Edit);

    auto* undoAction = new QAction(tr("Undo"), this);
    auto* undoCmd    = actionManager->registerAction(undoAction, Constants::Actions::Undo, playlistContext->context());
    undoCmd->setDefaultShortcut(QKeySequence::Undo);
    editMenu->addAction(undoCmd);
    QObject::connect(undoAction, &QAction::triggered, this, [this]() { playlistController->undoPlaylistChanges(); });
    QObject::connect(playlistController, &PlaylistController::playlistHistoryChanged, this,
                     [this, undoAction]() { undoAction->setEnabled(playlistController->canUndo()); });
    undoAction->setEnabled(playlistController->canUndo());

    auto* redoAction = new QAction(tr("Redo"), this);
    auto* redoCmd    = actionManager->registerAction(redoAction, Constants::Actions::Redo, playlistContext->context());
    redoCmd->setDefaultShortcut(QKeySequence::Redo);
    editMenu->addAction(redoCmd);
    QObject::connect(redoAction, &QAction::triggered, this, [this]() { playlistController->redoPlaylistChanges(); });
    QObject::connect(playlistController, &PlaylistController::playlistHistoryChanged, this,
                     [this, redoAction]() { redoAction->setEnabled(playlistController->canRedo()); });
    redoAction->setEnabled(playlistController->canRedo());

    editMenu->addSeparator();

    auto* clearAction = new QAction(PlaylistWidget::tr("&Clear"), self);
    editMenu->addAction(
        actionManager->registerAction(clearAction, Constants::Actions::Clear, playlistContext->context()));
    QObject::connect(clearAction, &QAction::triggered, this, [this]() {
        playlistView->selectAll();
        tracksRemoved();
    });

    auto* selectAllAction = new QAction(PlaylistWidget::tr("&Select All"), self);
    auto* selectAllCmd
        = actionManager->registerAction(selectAllAction, Constants::Actions::SelectAll, playlistContext->context());
    selectAllCmd->setDefaultShortcut(QKeySequence::SelectAll);
    editMenu->addAction(selectAllCmd);
    QObject::connect(selectAllAction, &QAction::triggered, this, [this]() { selectAll(); });

    removeTrackAction->setEnabled(false);
    auto* removeCmd
        = actionManager->registerAction(removeTrackAction, Constants::Actions::Remove, playlistContext->context());
    removeCmd->setDefaultShortcut(QKeySequence::Delete);
    QObject::connect(removeTrackAction, &QAction::triggered, this, [this]() { tracksRemoved(); });

    addToQueueAction->setEnabled(false);
    actionManager->registerAction(addToQueueAction, Constants::Actions::AddToQueue, playlistContext->context());
    QObject::connect(addToQueueAction, &QAction::triggered, this, [this]() { queueSelectedTracks(); });

    removeFromQueueAction->setVisible(false);
    actionManager->registerAction(removeFromQueueAction, Constants::Actions::RemoveFromQueue,
                                  playlistContext->context());
    QObject::connect(removeFromQueueAction, &QAction::triggered, this, [this]() { dequeueSelectedTracks(); });
}

void PlaylistWidgetPrivate::onPresetChanged(const PlaylistPreset& preset)
{
    if(currentPreset.id == preset.id) {
        changePreset(preset);
    }
}

void PlaylistWidgetPrivate::changePreset(const PlaylistPreset& preset)
{
    currentPreset = preset;
    resetModel();
}

void PlaylistWidgetPrivate::changePlaylist(Playlist* prevPlaylist, Playlist* /*playlist*/)
{
    saveState(prevPlaylist);
    resetSort(true);
    resetModel();
}

void PlaylistWidgetPrivate::resetTree()
{
    resetSort();
    playlistView->playlistAboutToBeReset();
    restoreState(playlistController->currentPlaylist());
}

PlaylistViewState PlaylistWidgetPrivate::getState(Playlist* playlist) const
{
    if(!playlist) {
        return {};
    }

    PlaylistViewState state;

    if(playlist->trackCount() > 0) {
        QModelIndex topTrackIndex = playlistView->findIndexAt({0, 0}, true);

        if(topTrackIndex.isValid()) {
            topTrackIndex = topTrackIndex.siblingAtColumn(0);
            while(model->hasChildren(topTrackIndex)) {
                topTrackIndex = playlistView->indexBelow(topTrackIndex);
            }

            state.topIndex  = topTrackIndex.data(PlaylistItem::Index).toInt();
            state.scrollPos = playlistView->verticalScrollBar()->value();
        }
    }

    return state;
}

void PlaylistWidgetPrivate::saveState(Playlist* playlist) const
{
    if(!playlist) {
        return;
    }

    const auto state = getState(playlist);
    // Don't save state if still populating model
    if(playlist->trackCount() == 0 || (playlist->trackCount() > 0 && state.topIndex >= 0)) {
        playlistController->savePlaylistState(playlist, state);
    }
}

void PlaylistWidgetPrivate::restoreState(Playlist* playlist) const
{
    if(!playlist) {
        return;
    }

    auto restoreDefaultState = [this]() {
        playlistView->playlistReset();
        playlistView->scrollToTop();
    };

    if(playlist->trackCount() == 0) {
        restoreDefaultState();
        return;
    }

    const auto state = playlistController->playlistState(playlist);

    if(!state) {
        restoreDefaultState();
        return;
    }

    auto loadState = [this, state](bool fetch = false) -> bool {
        const auto& [index, end] = model->trackIndexAtPlaylistIndex(state->topIndex, fetch);

        if(!index.isValid() || (!fetch && end)) {
            return false;
        }

        playlistView->playlistReset();
        playlistView->scrollTo(index, PlaylistView::PositionAtTop);
        playlistView->verticalScrollBar()->setValue(state->scrollPos);

        return true;
    };

    if(!loadState()) {
        QObject::connect(
            model, &PlaylistModel::playlistLoaded, this, [loadState]() { loadState(true); }, Qt::SingleShotConnection);
    }
}

void PlaylistWidgetPrivate::resetModel() const
{
    if(playlistController->currentPlaylist()) {
        model->reset(currentPreset, singleMode ? PlaylistColumnList{} : columns, playlistController->currentPlaylist());
    }
}

std::vector<int> PlaylistWidgetPrivate::selectedPlaylistIndexes() const
{
    std::vector<int> selectedPlaylistIndexes;

    const auto selected = playlistView->selectionModel()->selectedRows();
    for(const QModelIndex& index : selected) {
        selectedPlaylistIndexes.emplace_back(index.data(PlaylistItem::Index).toInt());
    }

    return selectedPlaylistIndexes;
}

void PlaylistWidgetPrivate::restoreSelectedPlaylistIndexes(const std::vector<int>& indexes) const
{
    if(indexes.empty()) {
        return;
    }

    const int columnCount = static_cast<int>(columns.size());

    QItemSelection indexesToSelect;
    indexesToSelect.reserve(static_cast<qsizetype>(indexes.size()));

    for(const int selectedIndex : indexes) {
        const QModelIndex index = model->indexAtPlaylistIndex(selectedIndex);
        if(index.isValid()) {
            const QModelIndex last = index.siblingAtColumn(columnCount - 1);
            indexesToSelect.append({index, last.isValid() ? last : index});
        }
    }

    playlistView->selectionModel()->select(indexesToSelect, QItemSelectionModel::ClearAndSelect);
}

bool PlaylistWidgetPrivate::isHeaderHidden() const
{
    return header->isHidden();
}

bool PlaylistWidgetPrivate::isScrollbarHidden() const
{
    return playlistView->verticalScrollBarPolicy() == Qt::ScrollBarAlwaysOff;
}

void PlaylistWidgetPrivate::setHeaderHidden(bool hide) const
{
    header->setFixedHeight(hide ? 0 : QWIDGETSIZE_MAX);
    header->adjustSize();
}

void PlaylistWidgetPrivate::setScrollbarHidden(bool showScrollBar) const
{
    playlistView->setVerticalScrollBarPolicy(!showScrollBar ? Qt::ScrollBarAlwaysOff : Qt::ScrollBarAsNeeded);
}

void PlaylistWidgetPrivate::selectAll() const
{
    while(model->canFetchMore({})) {
        model->fetchMore({});
    }
    playlistView->selectAll();
}

void PlaylistWidgetPrivate::selectionChanged() const
{
    if(!playlistController->currentPlaylist()) {
        return;
    }

    TrackList tracks;
    std::set<int> trackIndexes;
    int firstIndex{-1};

    const QModelIndexList indexes = playlistView->selectionModel()->selectedRows();

    for(const QModelIndex& index : indexes) {
        const auto type = index.data(PlaylistItem::Type).toInt();
        if(type == PlaylistItem::Track) {
            tracks.push_back(index.data(PlaylistItem::Role::ItemData).value<Track>());
            const int trackIndex = index.data(PlaylistItem::Role::Index).toInt();
            trackIndexes.emplace(trackIndex);
            firstIndex = firstIndex >= 0 ? std::min(firstIndex, trackIndex) : trackIndex;
        }
    }

    selectionController->changeSelectedTracks(playlistContext, firstIndex, tracks);

    if(tracks.empty()) {
        removeTrackAction->setEnabled(false);
        addToQueueAction->setEnabled(false);
        removeFromQueueAction->setVisible(false);
        return;
    }

    if(settings->value<Settings::Gui::PlaybackFollowsCursor>()) {
        if(playlistController->currentPlaylist()->currentTrackIndex() != firstIndex) {
            if(playlistController->playState() != PlayState::Playing) {
                playlistController->currentPlaylist()->changeCurrentIndex(firstIndex);
            }
            else {
                playlistController->currentPlaylist()->scheduleNextIndex(firstIndex);
            }
            playlistController->playlistHandler()->schedulePlaylist(playlistController->currentPlaylist());
        }
    }

    removeTrackAction->setEnabled(true);
    addToQueueAction->setEnabled(true);

    const auto queuedTracks
        = playerController->playbackQueue().indexesForPlaylist(playlistController->currentPlaylist()->id());
    const bool canDeque = std::ranges::any_of(
        queuedTracks, [&trackIndexes](const auto& track) { return trackIndexes.contains(track.first); });
    removeFromQueueAction->setVisible(canDeque);
}

void PlaylistWidgetPrivate::trackIndexesChanged(int playingIndex)
{
    resetSort(true);

    if(!playlistController->currentPlaylist()) {
        return;
    }

    const TrackList tracks = getAllTracks(model, {QModelIndex{}});

    if(!tracks.empty()) {
        const QModelIndexList selected = playlistView->selectionModel()->selectedRows();
        if(!selected.empty()) {
            const int firstIndex           = selected.front().data(PlaylistItem::Role::Index).toInt();
            const TrackList selectedTracks = getAllTracks(model, selected);
            selectionController->changeSelectedTracks(playlistContext, firstIndex, selectedTracks);
        }
    }

    playlistController->playlistHandler()->clearSchedulePlaylist();

    playlistController->aboutToChangeTracks();
    playerController->updateCurrentTrackIndex(playingIndex);
    playlistController->playlistHandler()->replacePlaylistTracks(playlistController->currentPlaylist()->id(), tracks);
    playlistController->changedTracks();

    if(playingIndex >= 0 && !playerController->currentIsQueueTrack()) {
        playlistController->currentPlaylist()->changeCurrentIndex(playingIndex);
    }

    model->updateHeader(playlistController->currentPlaylist());

    m_sorting = false;
}

void PlaylistWidgetPrivate::playSelectedTracks() const
{
    if(!playlistController->currentPlaylist()) {
        return;
    }

    const auto selected = playlistView->selectionModel()->selectedRows();

    if(selected.empty()) {
        return;
    }

    for(const QModelIndex& index : selected) {
        if(index.isValid() && index.data(PlaylistItem::Type).toInt() == PlaylistItem::Track) {
            auto track = index.data(PlaylistItem::ItemData).value<Track>();
            if(track.isValid()) {
                const int playIndex = index.data(PlaylistItem::Index).toInt();
                playlistController->currentPlaylist()->changeCurrentIndex(playIndex);
                playlistController->startPlayback();
                return;
            }
        }
    }
}

void PlaylistWidgetPrivate::queueSelectedTracks() const
{
    if(!playlistController->currentPlaylist()) {
        return;
    }

    const auto selected = playlistView->selectionModel()->selectedRows();
    const Id playlistId = playlistController->currentPlaylist()->id();

    QueueTracks tracks;

    for(const QModelIndex& index : selected) {
        if(index.isValid() && index.data(PlaylistItem::Type).toInt() == PlaylistItem::Track) {
            auto track = index.data(PlaylistItem::ItemData).value<Track>();
            if(track.isValid()) {
                tracks.emplace_back(track, playlistId, index.data(PlaylistItem::Index).toInt());
            }
        }
    }

    playerController->queueTracks(tracks);
}

void PlaylistWidgetPrivate::dequeueSelectedTracks() const
{
    if(!playlistController->currentPlaylist()) {
        return;
    }

    const auto selected = playlistView->selectionModel()->selectedRows();
    const Id playlistId = playlistController->currentPlaylist()->id();

    QueueTracks tracks;

    for(const QModelIndex& index : selected) {
        if(index.isValid() && index.data(PlaylistItem::Type).toInt() == PlaylistItem::Track) {
            auto track = index.data(PlaylistItem::ItemData).value<Track>();
            if(track.isValid()) {
                tracks.emplace_back(track, playlistId, index.data(PlaylistItem::Index).toInt());
            }
        }
    }

    playerController->dequeueTracks(tracks);
}

void PlaylistWidgetPrivate::scanDroppedTracks(const QList<QUrl>& urls, int index)
{
    if(!playlistController->currentPlaylist()) {
        return;
    }

    playlistView->setFocus(Qt::ActiveWindowFocusReason);

    playlistInteractor->filesToTracks(urls, [this, index](const TrackList& tracks) {
        auto* insertCmd
            = new InsertTracks(playerController, model, playlistController->currentPlaylist()->id(), {{index, tracks}});
        playlistController->addToHistory(insertCmd);
    });
}

void PlaylistWidgetPrivate::tracksInserted(const TrackGroups& tracks) const
{
    if(!playlistController->currentPlaylist()) {
        return;
    }

    auto* insertCmd = new InsertTracks(playerController, model, playlistController->currentPlaylist()->id(), tracks);
    playlistController->addToHistory(insertCmd);

    playlistView->setFocus(Qt::ActiveWindowFocusReason);
}

void PlaylistWidgetPrivate::tracksRemoved() const
{
    if(!playlistController->currentPlaylist()) {
        return;
    }

    const auto selected = playlistView->selectionModel()->selectedRows();

    QModelIndexList trackSelection;
    std::vector<int> indexes;

    for(const QModelIndex& index : selected) {
        if(index.isValid() && index.data(PlaylistItem::Type).toInt() == PlaylistItem::Track) {
            trackSelection.push_back(index);
            indexes.emplace_back(index.data(PlaylistItem::Index).toInt());
        }
    }

    playlistController->playlistHandler()->clearSchedulePlaylist();
    playlistController->playlistHandler()->removePlaylistTracks(playlistController->currentPlaylist()->id(), indexes);

    auto* delCmd = new RemoveTracks(playerController, model, playlistController->currentPlaylist()->id(),
                                    PlaylistModel::saveTrackGroups(trackSelection));
    playlistController->addToHistory(delCmd);

    model->updateHeader(playlistController->currentPlaylist());
}

void PlaylistWidgetPrivate::tracksMoved(const MoveOperation& operation) const
{
    if(!playlistController->currentPlaylist()) {
        return;
    }

    auto* moveCmd = new MoveTracks(playerController, model, playlistController->currentPlaylist()->id(), operation);
    playlistController->addToHistory(moveCmd);

    playlistView->setFocus(Qt::ActiveWindowFocusReason);
}

void PlaylistWidgetPrivate::playlistTracksAdded(const TrackList& tracks, int index) const
{
    auto* insertCmd
        = new InsertTracks(playerController, model, playlistController->currentPlaylist()->id(), {{index, tracks}});
    playlistController->addToHistory(insertCmd);
}

void PlaylistWidgetPrivate::handleTracksChanged(const std::vector<int>& indexes, bool allNew)
{
    if(!m_sortingColumn) {
        resetSort(true);
    }

    saveState(playlistController->currentPlaylist());

    auto restoreSelection = [this](const int currentIndex, const std::vector<int>& selectedIndexes) {
        restoreState(playlistController->currentPlaylist());
        restoreSelectedPlaylistIndexes(selectedIndexes);

        if(currentIndex >= 0) {
            const QModelIndex index = model->indexAtPlaylistIndex(currentIndex);
            if(index.isValid()) {
                playlistView->selectionModel()->setCurrentIndex(index, QItemSelectionModel::NoUpdate);
            }
        }
    };

    const auto changedTrackCount = static_cast<int>(indexes.size());
    // It's faster to just reset if we're going to be updating more than half the playlist
    // or we're updating a large number of tracks
    if(changedTrackCount > 500 || changedTrackCount > (playlistController->currentPlaylist()->trackCount() / 2)) {
        std::vector<int> selectedIndexes;

        if(!allNew) {
            selectedIndexes = selectedPlaylistIndexes();

            QObject::connect(
                model, &PlaylistModel::playlistTracksChanged, this,
                [restoreSelection, selectedIndexes]() { restoreSelection(-1, selectedIndexes); },
                Qt::SingleShotConnection);
        }

        resetModel();
    }
    else {
        int currentIndex{-1};
        if(playlistView->currentIndex().isValid()) {
            currentIndex = playlistView->currentIndex().data(PlaylistItem::Index).toInt();
        }
        const std::vector<int> selectedIndexes = selectedPlaylistIndexes();

        QObject::connect(
            model, &PlaylistModel::playlistTracksChanged, this,
            [restoreSelection, currentIndex, selectedIndexes]() { restoreSelection(currentIndex, selectedIndexes); },
            Qt::SingleShotConnection);

        model->updateTracks(indexes);
    }
}

void PlaylistWidgetPrivate::handlePlayingTrackChanged(const PlaylistTrack& track) const
{
    model->playingTrackChanged(track);
    if(track.playlistId == playlistController->currentPlaylistId()) {
        followCurrentTrack(track.track, track.indexInPlaylist);
    }
}

void PlaylistWidgetPrivate::setSingleMode(bool enabled)
{
    if(std::exchange(singleMode, enabled) != singleMode && singleMode) {
        headerState = header->saveHeaderState();
        // Avoid display issues in case first section has been hidden
        header->showSection(header->logicalIndex(0));
    }

    header->setSectionsClickable(!singleMode);
    header->setSortIndicatorShown(!singleMode);

    if(!singleMode) {
        if(columns.empty()) {
            columns.push_back(columnRegistry.itemById(8));
            columns.push_back(columnRegistry.itemById(3));
            columns.push_back(columnRegistry.itemById(0));
            columns.push_back(columnRegistry.itemById(1));
            columns.push_back(columnRegistry.itemById(7));
        }

        auto resetColumns = [this]() {
            model->resetColumnAlignments();
            header->resetSectionPositions();
            header->setHeaderSectionWidths({{0, 0.06}, {1, 0.38}, {2, 0.08}, {3, 0.38}, {4, 0.10}});
            header->setHeaderSectionAlignment(0, Qt::AlignCenter);
            header->setHeaderSectionAlignment(2, Qt::AlignRight);
            header->setHeaderSectionAlignment(4, Qt::AlignRight);
        };

        if(std::cmp_equal(header->count(), columns.size())) {
            resetColumns();
            updateSpans();
        }
        else {
            QObject::connect(
                model, &QAbstractItemModel::modelReset, self,
                [this, resetColumns]() {
                    if(!headerState.isEmpty()) {
                        header->restoreHeaderState(headerState);
                    }
                    else {
                        resetColumns();
                    }
                    updateSpans();
                },
                Qt::SingleShotConnection);
        }
    }

    resetModel();
}

void PlaylistWidgetPrivate::updateSpans()
{
    auto isPixmap = [](const QString& field) {
        return field == QString::fromLatin1(FrontCover) || field == QString::fromLatin1(BackCover)
            || field == QString::fromLatin1(ArtistPicture);
    };

    for(auto i{0}; const auto& column : columns) {
        if(isPixmap(column.field)) {
            playlistView->setSpan(i, true);
        }
        else {
            playlistView->setSpan(i, false);
        }
        ++i;
    }
}

void PlaylistWidgetPrivate::customHeaderMenuRequested(const QPoint& pos)
{
    auto* menu = new QMenu(self);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    if(!singleMode) {
        auto* columnsMenu = new QMenu(tr("Columns"), menu);
        auto* columnGroup = new QActionGroup{menu};
        columnGroup->setExclusionPolicy(QActionGroup::ExclusionPolicy::None);

        auto hasColumn = [this](int id) {
            return std::ranges::any_of(columns, [id](const PlaylistColumn& column) { return column.id == id; });
        };

        for(const auto& column : columnRegistry.items()) {
            auto* columnAction = new QAction(column.name, columnsMenu);
            columnAction->setData(column.id);
            columnAction->setCheckable(true);
            columnAction->setChecked(hasColumn(column.id));
            columnsMenu->addAction(columnAction);
            columnGroup->addAction(columnAction);
        }

        QObject::connect(columnGroup, &QActionGroup::triggered, self, [this](QAction* action) {
            const int columnId = action->data().toInt();
            if(action->isChecked()) {
                const PlaylistColumn column = columnRegistry.itemById(action->data().toInt());
                if(column.isValid()) {
                    columns.push_back(column);
                    updateSpans();
                    changePreset(currentPreset);
                }
            }
            else {
                auto colIt = std::ranges::find_if(columns,
                                                  [columnId](const PlaylistColumn& col) { return col.id == columnId; });
                if(colIt != columns.end()) {
                    const int removedIndex = static_cast<int>(std::distance(columns.begin(), colIt));
                    if(model->removeColumn(removedIndex)) {
                        columns.erase(colIt);
                        updateSpans();
                    }
                }
            }
        });

        auto* moreSettings = new QAction(tr("More…"), columnsMenu);
        QObject::connect(moreSettings, &QAction::triggered, self,
                         [this]() { settingsDialog->openAtPage(Constants::Page::PlaylistColumns); });
        columnsMenu->addSeparator();
        columnsMenu->addAction(moreSettings);

        menu->addMenu(columnsMenu);
        menu->addSeparator();
        header->addHeaderContextMenu(menu, self->mapToGlobal(pos));
        menu->addSeparator();
        header->addHeaderAlignmentMenu(menu, self->mapToGlobal(pos));

        auto* resetAction = new QAction(tr("Reset columns to default"), menu);
        QObject::connect(resetAction, &QAction::triggered, self, [this]() {
            columns.clear();
            headerState.clear();
            setSingleMode(false);
        });
        menu->addAction(resetAction);
    }

    auto* columnModeAction = new QAction(tr("Single-column mode"), menu);
    columnModeAction->setCheckable(true);
    columnModeAction->setChecked(singleMode);
    QObject::connect(columnModeAction, &QAction::triggered, self, [this]() { setSingleMode(!singleMode); });
    menu->addAction(columnModeAction);

    menu->addSeparator();

    addPresetMenu(menu);

    menu->popup(self->mapToGlobal(pos));
}

void PlaylistWidgetPrivate::doubleClicked(const QModelIndex& /*index*/) const
{
    selectionController->executeAction(TrackAction::Play);
    playlistView->clearSelection();
}

void PlaylistWidgetPrivate::followCurrentTrack(const Track& track, int index) const
{
    if(!settings->value<Settings::Gui::CursorFollowsPlayback>()) {
        return;
    }

    if(playlistController->playState() != PlayState::Playing) {
        return;
    }

    if(!track.isValid()) {
        return;
    }

    QModelIndex modelIndex = model->indexAtPlaylistIndex(index);
    while(modelIndex.isValid() && header->isSectionHidden(modelIndex.column())) {
        modelIndex = modelIndex.siblingAtColumn(modelIndex.column() + 1);
    }

    if(!modelIndex.isValid()) {
        return;
    }

    const QRect indexRect    = playlistView->visualRect(modelIndex);
    const QRect viewportRect = playlistView->viewport()->rect();

    if((indexRect.top() < 0) || (viewportRect.bottom() - indexRect.bottom() < 0)) {
        playlistView->scrollTo(modelIndex, QAbstractItemView::PositionAtCenter);
    }

    playlistView->setCurrentIndex(modelIndex);
}

void PlaylistWidgetPrivate::sortTracks(const QString& script) const
{
    if(!playlistController->currentPlaylist()) {
        return;
    }

    auto* currentPlaylist    = playlistController->currentPlaylist();
    const auto currentTracks = currentPlaylist->tracks();

    auto handleSortedTracks = [this, currentPlaylist](const TrackList& sortedTracks) {
        playlistController->playlistHandler()->replacePlaylistTracks(currentPlaylist->id(), sortedTracks);
    };

    if(playlistView->selectionModel()->hasSelection()) {
        const auto selected = playlistView->selectionModel()->selectedRows();

        std::vector<int> indexesToSort;

        for(const QModelIndex& index : selected) {
            if(index.data(PlaylistItem::Type).toInt() == PlaylistItem::Track) {
                indexesToSort.push_back(index.data(PlaylistItem::Index).toInt());
            }
        }

        std::ranges::sort(indexesToSort);

        Utils::asyncExec([currentTracks, script, indexesToSort]() {
            return Sorting::calcSortTracks(script, currentTracks, indexesToSort);
        }).then(self, handleSortedTracks);
    }
    else {
        Utils::asyncExec([currentTracks, script]() {
            return Sorting::calcSortTracks(script, currentTracks);
        }).then(self, handleSortedTracks);
    }
}

void PlaylistWidgetPrivate::sortColumn(int column, Qt::SortOrder order)
{
    if(!playlistController->currentPlaylist() || column < 0 || std::cmp_greater_equal(column, columns.size())) {
        return;
    }

    m_sorting       = true;
    m_sortingColumn = true;

    auto* currentPlaylist    = playlistController->currentPlaylist();
    const auto currentTracks = currentPlaylist->tracks();
    const QString sortField  = columns.at(column).field;

    Utils::asyncExec([sortField, currentTracks, order]() {
        return Sorting::calcSortTracks(sortField, currentTracks, order);
    }).then(self, [this, currentPlaylist](const TrackList& sortedTracks) {
        playlistController->playlistHandler()->replacePlaylistTracks(currentPlaylist->id(), sortedTracks);
        m_sortingColumn = false;
    });
}

void PlaylistWidgetPrivate::resetSort(bool force)
{
    if(force) {
        m_sorting = false;
    }

    if(!m_sorting) {
        header->setSortIndicator(-1, Qt::AscendingOrder);
    }
}

void PlaylistWidgetPrivate::addSortMenu(QMenu* parent, bool disabled)
{
    auto* sortMenu = new QMenu(PlaylistWidget::tr("Sort"), parent);

    if(disabled) {
        sortMenu->setEnabled(false);
    }
    else {
        const auto& groups = sortRegistry.items();
        for(const auto& script : groups) {
            auto* switchSort = new QAction(script.name, sortMenu);
            QObject::connect(switchSort, &QAction::triggered, self, [this, script]() { sortTracks(script.script); });
            sortMenu->addAction(switchSort);
        }
    }

    parent->addMenu(sortMenu);
}

void PlaylistWidgetPrivate::addPresetMenu(QMenu* parent)
{
    auto* presetsMenu = new QMenu(PlaylistWidget::tr("Presets"), parent);

    const auto& presets = presetRegistry.items();

    for(const auto& preset : presets) {
        auto* switchPreset = new QAction(preset.name, presetsMenu);
        if(preset == currentPreset) {
            presetsMenu->setDefaultAction(switchPreset);
        }

        const int presetId = preset.id;
        QObject::connect(switchPreset, &QAction::triggered, self,
                         [this, presetId]() { settings->set<PlaylistCurrentPreset>(presetId); });

        presetsMenu->addAction(switchPreset);
    }

    parent->addMenu(presetsMenu);
}

PlaylistWidget::PlaylistWidget(ActionManager* actionManager, PlaylistInteractor* playlistInteractor,
                               SettingsManager* settings, QWidget* parent)
    : FyWidget{parent}
    , p{std::make_unique<PlaylistWidgetPrivate>(this, actionManager, playlistInteractor, settings)}
{
    setObjectName(PlaylistWidget::name());
}

PlaylistWidget::~PlaylistWidget()
{
    if(!p->playlistController->currentPlaylist()) {
        return;
    }

    p->playlistController->clearHistory();
    p->playlistController->savePlaylistState(p->playlistController->currentPlaylist(),
                                             p->getState(p->playlistController->currentPlaylist()));
}

QString PlaylistWidget::name() const
{
    return tr("Playlist");
}

QString PlaylistWidget::layoutName() const
{
    return QStringLiteral("Playlist");
}

void PlaylistWidget::saveLayoutData(QJsonObject& layout)
{
    layout[QStringLiteral("SingleMode")] = p->singleMode;

    if(!p->columns.empty()) {
        QStringList columns;

        for(int i{0}; const auto& column : p->columns) {
            const auto alignment = p->model->columnAlignment(i++);
            QString colStr       = QString::number(column.id);

            if(alignment != Qt::AlignLeft) {
                colStr += QStringLiteral(":") + QString::number(alignment.toInt());
            }

            columns.push_back(colStr);
        }

        layout[QStringLiteral("Columns")] = columns.join(QStringLiteral("|"));
    }

    p->resetSort();

    if(!p->singleMode || !p->headerState.isEmpty()) {
        QByteArray state = p->singleMode && !p->headerState.isEmpty() ? p->headerState : p->header->saveHeaderState();
        state            = qCompress(state, 9);
        layout[QStringLiteral("HeaderState")] = QString::fromUtf8(state.toBase64());
    }
}

void PlaylistWidget::loadLayoutData(const QJsonObject& layout)
{
    if(layout.contains(QStringLiteral("SingleMode"))) {
        p->singleMode = layout.value(QStringLiteral("SingleMode")).toBool();
    }

    if(layout.contains(QStringLiteral("Columns"))) {
        p->columns.clear();

        const QString columnData    = layout.value(QStringLiteral("Columns")).toString();
        const QStringList columnIds = columnData.split(QStringLiteral("|"));

        for(int i{0}; const auto& columnId : columnIds) {
            const auto column     = columnId.split(QStringLiteral(":"));
            const auto columnItem = p->columnRegistry.itemById(column.at(0).toInt());

            if(columnItem.isValid()) {
                p->columns.push_back(columnItem);

                if(column.size() > 1) {
                    p->model->changeColumnAlignment(i, static_cast<Qt::Alignment>(column.at(1).toInt()));
                }
                else {
                    p->model->changeColumnAlignment(i, Qt::AlignLeft);
                }
            }
            ++i;
        }

        p->updateSpans();
    }

    if(layout.contains(QStringLiteral("HeaderState"))) {
        auto state = QByteArray::fromBase64(layout.value(QStringLiteral("HeaderState")).toString().toUtf8());

        if(state.isEmpty()) {
            return;
        }

        p->headerState = qUncompress(state);
    }
}

void PlaylistWidget::finalise()
{
    p->currentPreset = p->presetRegistry.itemById(p->settings->value<PlaylistCurrentPreset>());

    p->header->setSectionsClickable(!p->singleMode);
    p->header->setSortIndicatorShown(!p->singleMode);

    if(!p->singleMode && !p->columns.empty() && !p->headerState.isEmpty()) {
        if(!p->columns.empty() && !p->headerState.isEmpty()) {
            QObject::connect(
                p->model, &QAbstractItemModel::modelReset, this,
                [this]() { p->header->restoreHeaderState(p->headerState); }, Qt::SingleShotConnection);
        }
    }

    if(!p->singleMode && p->columns.empty()) {
        p->setSingleMode(false);
    }
    else if(p->playlistController->currentPlaylist()) {
        p->changePlaylist(nullptr, p->playlistController->currentPlaylist());
    }
}

void PlaylistWidget::contextMenuEvent(QContextMenuEvent* event)
{
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    const auto selected     = p->playlistView->selectionModel()->selectedRows();
    const bool hasSelection = !selected.empty();

    if(hasSelection) {
        auto* playAction = new QAction(tr("&Play"), this);
        QObject::connect(playAction, &QAction::triggered, this, [this]() { p->playSelectedTracks(); });
        menu->addAction(playAction);

        menu->addSeparator();

        if(auto* removeCmd = p->actionManager->command(Constants::Actions::Remove)) {
            menu->addAction(removeCmd->action());
        }

        p->addSortMenu(menu, selected.size() == 1);
    }

    p->addPresetMenu(menu);

    if(hasSelection) {
        if(auto* addQueueCmd = p->actionManager->command(Constants::Actions::AddToQueue)) {
            menu->addAction(addQueueCmd->action());
        }

        if(auto* removeQueueCmd = p->actionManager->command(Constants::Actions::RemoveFromQueue)) {
            menu->addAction(removeQueueCmd->action());
        }

        p->selectionController->addTrackContextMenu(menu);
    }

    menu->popup(event->globalPos());
}

void PlaylistWidget::keyPressEvent(QKeyEvent* event)
{
    const auto key = event->key();

    if(event == QKeySequence::SelectAll) {
        p->selectAll();
    }
    else if(key == Qt::Key_Enter || key == Qt::Key_Return) {
        if(p->selectionController->hasTracks()) {
            p->selectionController->executeAction(TrackAction::Play);
        }
        p->playlistView->clearSelection();
    }

    QWidget::keyPressEvent(event);
}
} // namespace Fooyin

#include "moc_playlistwidget.cpp"
