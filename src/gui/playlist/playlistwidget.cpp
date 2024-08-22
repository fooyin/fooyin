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
#include "playlistcolumnregistry.h"
#include "playlistcommands.h"
#include "playlistcontroller.h"
#include "playlistdelegate.h"
#include "playlistview.h"
#include "playlistwidget_p.h"

#include <core/application.h>
#include <core/library/musiclibrary.h>
#include <core/library/trackfilter.h>
#include <core/library/tracksort.h>
#include <core/player/playercontroller.h>
#include <core/playlist/playlisthandler.h>
#include <core/plugins/coreplugincontext.h>
#include <gui/guiconstants.h>
#include <gui/guisettings.h>
#include <gui/trackselectioncontroller.h>
#include <utils/actions/actioncontainer.h>
#include <utils/actions/actionmanager.h>
#include <utils/actions/command.h>
#include <utils/async.h>
#include <utils/modelutils.h>
#include <utils/settings/settingsdialogcontroller.h>
#include <utils/stardelegate.h>
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
Fooyin::TrackList getTracks(const QModelIndexList& indexes)
{
    Fooyin::TrackList tracks;

    for(const QModelIndex& index : indexes) {
        if(index.isValid() && index.data(Fooyin::PlaylistItem::Type).toInt() == Fooyin::PlaylistItem::Track) {
            auto track = index.data(Fooyin::PlaylistItem::ItemData).value<Fooyin::Track>();
            if(track.isValid()) {
                tracks.emplace_back(track);
            }
        }
    }

    return tracks;
}

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

void getAllTrackIndexes(QAbstractItemModel* model, const QModelIndex& parent, QModelIndexList& indexList)
{
    while(model->canFetchMore(parent)) {
        model->fetchMore(parent);
    }

    const int rowCount = model->rowCount(parent);
    for(int row{0}; row < rowCount; ++row) {
        const QModelIndex index = model->index(row, 0, parent);
        if(index.data(Fooyin::PlaylistItem::Role::Type).toInt() == Fooyin::PlaylistItem::Track) {
            indexList.append(index);
        }
        else if(model->hasChildren(index)) {
            getAllTrackIndexes(model, index, indexList);
        }
    }
}

QModelIndexList filterSelectedIndexes(const QAbstractItemView* view)
{
    QModelIndexList filteredIndexes;
    const auto indexes = view->selectionModel()->selectedIndexes();

    if(!indexes.isEmpty()) {
        const int column = indexes.first().column();
        for(const auto& index : indexes) {
            if(index.column() == column) {
                filteredIndexes.append(index);
            }
        }
    }

    return filteredIndexes;
}
} // namespace

namespace Fooyin {
using namespace Settings::Gui::Internal;

PlaylistWidgetPrivate::PlaylistWidgetPrivate(PlaylistWidget* self, ActionManager* actionManager,
                                             PlaylistInteractor* playlistInteractor, CoverProvider* coverProvider,
                                             Application* core)
    : m_self{self}
    , m_actionManager{actionManager}
    , m_playlistInteractor{playlistInteractor}
    , m_playlistController{m_playlistInteractor->playlistController()}
    , m_playerController{playlistInteractor->playerController()}
    , m_selectionController{m_playlistController->selectionController()}
    , m_library{playlistInteractor->library()}
    , m_settings{core->settingsManager()}
    , m_settingsDialog{m_settings->settingsDialog()}
    , m_sorter{core->libraryManager()}
    , m_detached{false}
    , m_columnRegistry{m_playlistController->columnRegistry()}
    , m_presetRegistry{m_playlistController->presetRegistry()}
    , m_sortRegistry{core->sortingRegistry()}
    , m_layout{new QHBoxLayout(m_self)}
    , m_model{new PlaylistModel(m_playlistInteractor, coverProvider, m_settings, m_self)}
    , m_delgate{new PlaylistDelegate(m_self)}
    , m_starDelegate{nullptr}
    , m_playlistView{new PlaylistView(m_self)}
    , m_header{new AutoHeaderView(Qt::Horizontal, m_self)}
    , m_singleMode{false}
    , m_playlistContext{new WidgetContext(m_self, Context{Constants::Context::Playlist}, m_self)}
    , m_middleClickAction{static_cast<TrackAction>(m_settings->value<PlaylistMiddleClick>())}
    , m_playAction{new QAction(tr("&Play"), m_self)}
    , m_cropAction{new QAction(tr("&Crop"), m_self)}
    , m_stopAfter{new QAction(tr("&Stop after this"), m_self)}
    , m_cutAction{new QAction(tr("Cut"), m_self)}
    , m_copyAction{new QAction(tr("Copy"), m_self)}
    , m_pasteAction{new QAction(tr("Paste"), m_self)}
    , m_clearAction{new QAction(tr("&Clear"), m_self)}
    , m_removeTrackAction{new QAction(tr("Remove"), m_self)}
    , m_addToQueueAction{new QAction(tr("Add to playback queue"), m_self)}
    , m_removeFromQueueAction{new QAction(tr("Remove from playback queue"), m_self)}
    , m_sorting{false}
    , m_sortingColumn{false}
    , m_showPlaying{false}
{
    m_layout->setContentsMargins(0, 0, 0, 0);

    m_header->setStretchEnabled(true);
    m_header->setSectionsClickable(true);
    m_header->setSectionsMovable(true);
    m_header->setFirstSectionMovable(true);
    m_header->setContextMenuPolicy(Qt::CustomContextMenu);
    m_header->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    m_playlistView->setModel(m_model);
    m_playlistView->setHeader(m_header);
    m_playlistView->setItemDelegate(m_delgate);
    m_playlistView->viewport()->installEventFilter(new ToolTipFilter(m_self));

    m_layout->addWidget(m_playlistView);

    setHeaderHidden(!m_settings->value<PlaylistHeader>());
    setScrollbarHidden(m_settings->value<PlaylistScrollBar>());

    m_model->playingTrackChanged(m_playlistController->currentTrack());
    m_model->playStateChanged(m_playlistController->playState());
}

void PlaylistWidgetPrivate::setupConnections()
{
    // clang-format off
    QObject::connect(m_header, &QHeaderView::sectionCountChanged, m_playlistView, &PlaylistView::setupRatingDelegate);
    QObject::connect(m_header, &QHeaderView::sectionResized, m_self, [this](int column, int /*oldSize*/, int newSize) { m_model->setPixmapColumnSize(column, newSize); });
    QObject::connect(m_header, &QHeaderView::sortIndicatorChanged, this, &PlaylistWidgetPrivate::sortColumn);
    QObject::connect(m_header, &QHeaderView::customContextMenuRequested, this, &PlaylistWidgetPrivate::customHeaderMenuRequested);
    QObject::connect(m_playlistView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &PlaylistWidgetPrivate::selectionChanged);
    QObject::connect(m_playlistView, &PlaylistView::tracksRated, m_library, [this](const TrackList& tracks) { m_library->updateTrackStats(tracks); });
    QObject::connect(m_playlistView, &QAbstractItemView::doubleClicked, this, &PlaylistWidgetPrivate::doubleClicked);
    QObject::connect(m_model, &QAbstractItemModel::modelAboutToBeReset, m_playlistView, &QAbstractItemView::clearSelection);
    QObject::connect(m_playlistController, &PlaylistController::currentPlaylistTracksPlayed, m_model, &PlaylistModel::refreshTracks);

    QObject::connect(m_columnRegistry, &PlaylistColumnRegistry::itemRemoved, this, &PlaylistWidgetPrivate::onColumnRemoved);
    QObject::connect(m_columnRegistry, &PlaylistColumnRegistry::columnChanged, this, &PlaylistWidgetPrivate::onColumnChanged);

    if(!m_detached) {
        QObject::connect(m_presetRegistry, &PresetRegistry::presetChanged, this, &PlaylistWidgetPrivate::onPresetChanged);

        QObject::connect(m_playlistView, &ExpandedTreeView::middleClicked, this, &PlaylistWidgetPrivate::middleClicked);
        QObject::connect(m_model, &PlaylistModel::playlistTracksChanged, this, &PlaylistWidgetPrivate::trackIndexesChanged);
        QObject::connect(m_model, &PlaylistModel::filesDropped, this, &PlaylistWidgetPrivate::scanDroppedTracks);
        QObject::connect(m_model, &PlaylistModel::tracksInserted, this, &PlaylistWidgetPrivate::tracksInserted);
        QObject::connect(m_model, &PlaylistModel::tracksMoved, this, &PlaylistWidgetPrivate::tracksMoved);
        QObject::connect(m_model, &QAbstractItemModel::modelReset, this, &PlaylistWidgetPrivate::resetTree);

        QObject::connect(m_playlistController->playlistHandler(), &PlaylistHandler::activePlaylistChanged, this, [this]() { m_model->playingTrackChanged(m_playerController->currentPlaylistTrack()); });
        QObject::connect(m_playlistController, &PlaylistController::currentPlaylistTracksChanged, this, &PlaylistWidgetPrivate::handleTracksChanged);
        QObject::connect(m_playlistController, &PlaylistController::currentPlaylistQueueChanged, m_model, &PlaylistModel::refreshTracks);
        QObject::connect(m_playlistController, &PlaylistController::currentPlaylistChanged, this, &PlaylistWidgetPrivate::changePlaylist);
        QObject::connect(m_playlistController, &PlaylistController::playlistsLoaded, this, [this]() { changePlaylist(nullptr, m_playlistController->currentPlaylist()); });
        QObject::connect(m_playlistController, &PlaylistController::playingTrackChanged, this, &PlaylistWidgetPrivate::handlePlayingTrackChanged);
        QObject::connect(m_playlistController, &PlaylistController::playStateChanged, m_model, &PlaylistModel::playStateChanged);
        QObject::connect(m_playlistController, &PlaylistController::currentPlaylistTracksAdded, this, &PlaylistWidgetPrivate::playlistTracksAdded);
        QObject::connect(m_playlistController, &PlaylistController::selectTracks, this, &PlaylistWidgetPrivate::selectTrackIds);
        QObject::connect(m_playlistController, &PlaylistController::showCurrentTrack, m_self, [this]() {
            if(m_playlistController->currentIsActive()) {
                followCurrentTrack();
            }
            else {
                m_showPlaying = true;
            }
        });
        m_settings->subscribe<PlaylistMiddleClick>(m_self, [this](int action) { m_middleClickAction = static_cast<TrackAction>(action); });
        m_settings->subscribe<PlaylistHeader>(this, [this](bool show) { setHeaderHidden(!show); });
        m_settings->subscribe<PlaylistScrollBar>(this, &PlaylistWidgetPrivate::setScrollbarHidden);
    }
    // clang-format on
}

void PlaylistWidgetPrivate::setupActions()
{
    m_playAction->setStatusTip(tr("Start playback of the selected track"));
    QObject::connect(m_playAction, &QAction::triggered, this, [this]() { playSelectedTracks(); });

    if(m_detached) {
        return;
    }

    m_cropAction->setStatusTip(tr("Remove all tracks from the playlist except for the selected tracks"));
    QObject::connect(m_cropAction, &QAction::triggered, this, [this]() { cropSelection(); });

    m_stopAfter->setStatusTip(tr("Stop playback at the end of the selected track"));
    QObject::connect(m_stopAfter, &QAction::triggered, this, [this]() { stopAfterTrack(); });

    m_actionManager->addContextObject(m_playlistContext);

    auto* editMenu = m_actionManager->actionContainer(Constants::Menus::Edit);

    auto* undoAction = new QAction(tr("Undo"), this);
    undoAction->setStatusTip(tr("Undo the previous playlist change"));
    auto* undoCmd = m_actionManager->registerAction(undoAction, Constants::Actions::Undo, m_playlistContext->context());
    undoCmd->setDefaultShortcut(QKeySequence::Undo);
    editMenu->addAction(undoCmd);
    QObject::connect(undoAction, &QAction::triggered, this, [this]() { m_playlistController->undoPlaylistChanges(); });
    QObject::connect(m_playlistController, &PlaylistController::playlistHistoryChanged, this,
                     [this, undoAction]() { undoAction->setEnabled(m_playlistController->canUndo()); });
    undoAction->setEnabled(m_playlistController->canUndo());

    auto* redoAction = new QAction(tr("Redo"), this);
    redoAction->setStatusTip(tr("Redo the previous playlist change"));
    auto* redoCmd = m_actionManager->registerAction(redoAction, Constants::Actions::Redo, m_playlistContext->context());
    redoCmd->setDefaultShortcut(QKeySequence::Redo);
    editMenu->addAction(redoCmd);
    QObject::connect(redoAction, &QAction::triggered, this, [this]() { m_playlistController->redoPlaylistChanges(); });
    QObject::connect(m_playlistController, &PlaylistController::playlistHistoryChanged, this,
                     [this, redoAction]() { redoAction->setEnabled(m_playlistController->canRedo()); });
    redoAction->setEnabled(m_playlistController->canRedo());

    editMenu->addSeparator();

    m_cutAction->setStatusTip(tr("Cut the selected tracks"));
    auto* cutCommand
        = m_actionManager->registerAction(m_cutAction, Constants::Actions::Cut, m_playlistContext->context());
    cutCommand->setDefaultShortcut(QKeySequence::Cut);
    editMenu->addAction(cutCommand);
    QObject::connect(m_playlistView->selectionModel(), &QItemSelectionModel::selectionChanged, this,
                     [this]() { m_cutAction->setEnabled(m_playlistView->selectionModel()->hasSelection()); });
    QObject::connect(m_cutAction, &QAction::triggered, this, &PlaylistWidgetPrivate::cutTracks);
    m_cutAction->setEnabled(m_playlistView->selectionModel()->hasSelection());

    m_copyAction->setStatusTip(tr("Copy the selected tracks"));
    auto* copyCommand
        = m_actionManager->registerAction(m_copyAction, Constants::Actions::Copy, m_playlistContext->context());
    copyCommand->setDefaultShortcut(QKeySequence::Copy);
    editMenu->addAction(copyCommand);
    QObject::connect(m_playlistView->selectionModel(), &QItemSelectionModel::selectionChanged, this,
                     [this]() { m_copyAction->setEnabled(m_playlistView->selectionModel()->hasSelection()); });
    QObject::connect(m_copyAction, &QAction::triggered, this, &PlaylistWidgetPrivate::copyTracks);
    m_copyAction->setEnabled(m_playlistView->selectionModel()->hasSelection());

    m_pasteAction->setStatusTip(tr("Paste the selected tracks"));
    auto* pasteCommand
        = m_actionManager->registerAction(m_pasteAction, Constants::Actions::Paste, m_playlistContext->context());
    pasteCommand->setDefaultShortcut(QKeySequence::Paste);
    editMenu->addAction(m_pasteAction);
    QObject::connect(m_playlistController, &PlaylistController::clipboardChanged, this,
                     [this]() { m_pasteAction->setEnabled(!m_playlistController->clipboardEmpty()); });
    QObject::connect(m_pasteAction, &QAction::triggered, this, &PlaylistWidgetPrivate::pasteTracks);
    m_pasteAction->setEnabled(!m_playlistController->clipboardEmpty());

    editMenu->addSeparator();

    m_clearAction->setStatusTip(tr("Remove all tracks from the current playlist"));
    auto* clearCmd
        = m_actionManager->registerAction(m_clearAction, Constants::Actions::Clear, m_playlistContext->context());
    clearCmd->setAttribute(ProxyAction::UpdateText);
    editMenu->addAction(clearCmd);
    QObject::connect(m_clearAction, &QAction::triggered, this, [this]() { clearTracks(); });
    m_clearAction->setEnabled(m_model->rowCount({}) > 0);

    auto* selectAllAction = new QAction(tr("&Select all"), m_self);
    selectAllAction->setStatusTip(tr("Select all tracks in the current playlist"));
    auto* selectAllCmd
        = m_actionManager->registerAction(selectAllAction, Constants::Actions::SelectAll, m_playlistContext->context());
    selectAllCmd->setDefaultShortcut(QKeySequence::SelectAll);
    editMenu->addAction(selectAllCmd);
    QObject::connect(selectAllAction, &QAction::triggered, this, [this]() { selectAll(); });

    m_removeTrackAction->setStatusTip(tr("Remove the selected tracks from the current playlist"));
    m_removeTrackAction->setEnabled(false);
    auto* removeCmd = m_actionManager->registerAction(m_removeTrackAction, Constants::Actions::Remove,
                                                      m_playlistContext->context());
    removeCmd->setAttribute(ProxyAction::UpdateText);
    removeCmd->setDefaultShortcut(QKeySequence::Delete);
    QObject::connect(m_removeTrackAction, &QAction::triggered, this, [this]() { tracksRemoved(); });

    m_addToQueueAction->setStatusTip(tr("Add the selected tracks to the playback queue"));
    m_addToQueueAction->setEnabled(false);
    m_actionManager->registerAction(m_addToQueueAction, Constants::Actions::AddToQueue, m_playlistContext->context());
    QObject::connect(m_addToQueueAction, &QAction::triggered, this, [this]() { queueSelectedTracks(); });

    m_removeFromQueueAction->setStatusTip(tr("Remove the selected tracks from the playback queue"));
    m_removeFromQueueAction->setVisible(false);
    m_actionManager->registerAction(m_removeFromQueueAction, Constants::Actions::RemoveFromQueue,
                                    m_playlistContext->context());
    QObject::connect(m_removeFromQueueAction, &QAction::triggered, this, [this]() { dequeueSelectedTracks(); });
}

void PlaylistWidgetPrivate::onColumnChanged(const PlaylistColumn& changedColumn)
{
    auto existingIt = std::find_if(m_columns.begin(), m_columns.end(), [&changedColumn](const auto& column) {
        return (column.isDefault && changedColumn.isDefault && column.name == changedColumn.name)
            || column.id == changedColumn.id;
    });

    if(existingIt != m_columns.end()) {
        *existingIt = changedColumn;
        resetModel();
    }
}

void PlaylistWidgetPrivate::onColumnRemoved(int id)
{
    PlaylistColumnList columns;
    std::ranges::copy_if(m_columns, std::back_inserter(columns), [id](const auto& column) { return column.id != id; });
    if(std::exchange(m_columns, columns) != columns) {
        resetModel();
    }
}

void PlaylistWidgetPrivate::onPresetChanged(const PlaylistPreset& preset)
{
    if(m_currentPreset.id == preset.id) {
        changePreset(preset);
    }
}

void PlaylistWidgetPrivate::changePreset(const PlaylistPreset& preset)
{
    m_currentPreset = preset;
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
    m_playlistView->playlistAboutToBeReset();
    restoreState(m_playlistController->currentPlaylist());
    m_clearAction->setEnabled(m_model->rowCount({}) > 0);
}

PlaylistViewState PlaylistWidgetPrivate::getState(Playlist* playlist) const
{
    if(!playlist) {
        return {};
    }

    PlaylistViewState state;

    if(playlist->trackCount() > 0) {
        QModelIndex topTrackIndex = m_playlistView->findIndexAt({0, 0}, true);

        if(topTrackIndex.isValid()) {
            topTrackIndex = topTrackIndex.siblingAtColumn(0);
            while(m_model->hasChildren(topTrackIndex)) {
                topTrackIndex = m_playlistView->indexBelow(topTrackIndex);
            }

            state.topIndex  = topTrackIndex.data(PlaylistItem::Index).toInt();
            state.scrollPos = m_playlistView->verticalScrollBar()->value();
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
        m_playlistController->savePlaylistState(playlist, state);
    }
}

void PlaylistWidgetPrivate::restoreState(Playlist* playlist)
{
    if(!playlist) {
        return;
    }

    auto restoreDefaultState = [this]() {
        m_playlistView->playlistReset();
        m_playlistView->scrollToTop();
        if(std::exchange(m_showPlaying, false)) {
            followCurrentTrack();
        }
    };

    if(playlist->trackCount() == 0) {
        restoreDefaultState();
        return;
    }

    const auto state = m_playlistController->playlistState(playlist);

    if(!state) {
        restoreDefaultState();
        return;
    }

    auto loadState = [this, state]() {
        const auto& [index, end] = m_model->trackIndexAtPlaylistIndex(state->topIndex, true);

        if(!index.isValid()) {
            return;
        }

        m_playlistView->playlistReset();
        m_playlistView->scrollTo(index, PlaylistView::PositionAtTop);
        m_playlistView->verticalScrollBar()->setValue(state->scrollPos);

        if(std::exchange(m_showPlaying, false)) {
            followCurrentTrack();
        }
    };

    QObject::connect(
        m_model, &PlaylistModel::playlistLoaded, this, [loadState]() { loadState(); }, Qt::SingleShotConnection);
}

void PlaylistWidgetPrivate::resetModel() const
{
    if(m_detached) {
        m_model->reset(m_currentPreset, m_singleMode ? PlaylistColumnList{} : m_columns,
                       m_playlistController->currentPlaylist(), m_filteredTracks);
    }
    else {
        m_model->reset(m_currentPreset, m_singleMode ? PlaylistColumnList{} : m_columns,
                       m_playlistController->currentPlaylist());
    }
}

std::vector<int> PlaylistWidgetPrivate::selectedPlaylistIndexes() const
{
    std::vector<int> selectedPlaylistIndexes;

    const auto selected = filterSelectedIndexes(m_playlistView);
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

    const int columnCount = static_cast<int>(m_columns.size());

    QItemSelection indexesToSelect;
    indexesToSelect.reserve(static_cast<qsizetype>(indexes.size()));

    for(const int selectedIndex : indexes) {
        const QModelIndex index = m_model->indexAtPlaylistIndex(selectedIndex);
        if(index.isValid()) {
            const QModelIndex last = index.siblingAtColumn(columnCount - 1);
            indexesToSelect.append({index, last.isValid() ? last : index});
        }
    }

    m_playlistView->selectionModel()->select(indexesToSelect, QItemSelectionModel::ClearAndSelect);
}

bool PlaylistWidgetPrivate::isHeaderHidden() const
{
    return m_header->isHidden();
}

bool PlaylistWidgetPrivate::isScrollbarHidden() const
{
    return m_playlistView->verticalScrollBarPolicy() == Qt::ScrollBarAlwaysOff;
}

void PlaylistWidgetPrivate::setHeaderHidden(bool hide) const
{
    m_header->setFixedHeight(hide ? 0 : QWIDGETSIZE_MAX);
    m_header->adjustSize();
}

void PlaylistWidgetPrivate::setScrollbarHidden(bool showScrollBar) const
{
    m_playlistView->setVerticalScrollBarPolicy(!showScrollBar ? Qt::ScrollBarAlwaysOff : Qt::ScrollBarAsNeeded);
}

void PlaylistWidgetPrivate::selectAll() const
{
    while(m_model->canFetchMore({})) {
        m_model->fetchMore({});
    }

    QMetaObject::invokeMethod(m_playlistView, [this]() { m_playlistView->selectAll(); }, Qt::QueuedConnection);
}

void PlaylistWidgetPrivate::selectionChanged() const
{
    if(!m_playlistController->currentPlaylist()) {
        return;
    }

    TrackList tracks;
    std::set<int> trackIndexes;
    int firstIndex{-1};

    QModelIndexList indexes = filterSelectedIndexes(m_playlistView);
    std::ranges::sort(indexes, Utils::sortModelIndexes);

    for(const QModelIndex& index : std::as_const(indexes)) {
        const auto type = index.data(PlaylistItem::Type).toInt();
        if(type == PlaylistItem::Track) {
            tracks.push_back(index.data(PlaylistItem::Role::ItemData).value<Track>());
            const int trackIndex = index.data(PlaylistItem::Role::Index).toInt();
            trackIndexes.emplace(trackIndex);
            firstIndex = firstIndex >= 0 ? std::min(firstIndex, trackIndex) : trackIndex;
        }
    }

    m_selectionController->changeSelectedTracks(m_playlistContext, firstIndex, tracks);
    emit m_self->selectionChanged(indexes);

    if(tracks.empty()) {
        m_removeTrackAction->setEnabled(false);
        m_addToQueueAction->setEnabled(false);
        m_removeFromQueueAction->setVisible(false);
        return;
    }

    if(m_settings->value<Settings::Gui::PlaybackFollowsCursor>()) {
        if(m_playlistController->currentPlaylist()->currentTrackIndex() != firstIndex) {
            if(m_playlistController->playState() != Player::PlayState::Playing) {
                m_playlistController->currentPlaylist()->changeCurrentIndex(firstIndex);
            }
            else {
                m_playlistController->currentPlaylist()->scheduleNextIndex(firstIndex);
            }
            m_playlistController->playlistHandler()->schedulePlaylist(m_playlistController->currentPlaylist());
        }
    }

    m_removeTrackAction->setEnabled(true);
    m_addToQueueAction->setEnabled(true);

    const auto queuedTracks
        = m_playerController->playbackQueue().indexesForPlaylist(m_playlistController->currentPlaylist()->id());
    const bool canDeque = std::ranges::any_of(
        queuedTracks, [&trackIndexes](const auto& track) { return trackIndexes.contains(track.first); });
    m_removeFromQueueAction->setVisible(canDeque);
}

void PlaylistWidgetPrivate::trackIndexesChanged(int playingIndex)
{
    if(m_sortingColumn) {
        m_sortingColumn = false;
    }
    else {
        resetSort(true);
    }

    if(!m_playlistController->currentPlaylist()) {
        return;
    }

    const TrackList tracks = getAllTracks(m_model, {QModelIndex{}});

    if(!tracks.empty()) {
        const QModelIndexList selected = filterSelectedIndexes(m_playlistView);
        if(!selected.empty()) {
            const int firstIndex           = selected.front().data(PlaylistItem::Role::Index).toInt();
            const TrackList selectedTracks = getAllTracks(m_model, selected);
            m_selectionController->changeSelectedTracks(m_playlistContext, firstIndex, selectedTracks);
        }
    }

    m_playlistController->playlistHandler()->clearSchedulePlaylist();

    m_playlistController->aboutToChangeTracks();
    m_playerController->updateCurrentTrackIndex(playingIndex);
    m_playlistController->playlistHandler()->replacePlaylistTracks(m_playlistController->currentPlaylist()->id(),
                                                                   tracks);
    m_playlistController->changedTracks();

    if(playingIndex >= 0 && !m_playerController->currentIsQueueTrack()) {
        m_playlistController->currentPlaylist()->changeCurrentIndex(playingIndex);
    }

    m_model->updateHeader(m_playlistController->currentPlaylist());

    m_sorting = false;
}

void PlaylistWidgetPrivate::stopAfterTrack()
{
    const auto index = m_playlistView->currentIndex();

    for(int col{0}; const auto& column : m_columns) {
        if(column.field == QLatin1String{PlayingIcon}) {
            m_model->stopAfterTrack(index.siblingAtColumn(col));
        }
        ++col;
    }
}

void PlaylistWidgetPrivate::playSelectedTracks() const
{
    if(!m_playlistController->currentPlaylist()) {
        return;
    }

    const auto selected = filterSelectedIndexes(m_playlistView);

    if(selected.empty()) {
        return;
    }

    for(const QModelIndex& index : selected) {
        if(index.isValid() && index.data(PlaylistItem::Type).toInt() == PlaylistItem::Track) {
            auto track = index.data(PlaylistItem::ItemData).value<Track>();
            if(track.isValid()) {
                const int playIndex = index.data(PlaylistItem::Index).toInt();
                m_playlistController->currentPlaylist()->changeCurrentIndex(playIndex);
                m_playlistController->startPlayback();
                return;
            }
        }
    }
}

void PlaylistWidgetPrivate::queueSelectedTracks(bool send) const
{
    if(!m_playlistController->currentPlaylist()) {
        return;
    }

    const auto selected  = filterSelectedIndexes(m_playlistView);
    const UId playlistId = m_playlistController->currentPlaylist()->id();

    QueueTracks tracks;

    for(const QModelIndex& index : selected) {
        if(index.isValid() && index.data(PlaylistItem::Type).toInt() == PlaylistItem::Track) {
            auto track = index.data(PlaylistItem::ItemData).value<Track>();
            if(track.isValid()) {
                tracks.emplace_back(track, playlistId, index.data(PlaylistItem::Index).toInt());
            }
        }
    }

    if(send) {
        m_playerController->replaceTracks(tracks);
    }
    else {
        m_playerController->queueTracks(tracks);
    }

    m_removeFromQueueAction->setVisible(true);
}

void PlaylistWidgetPrivate::dequeueSelectedTracks() const
{
    if(!m_playlistController->currentPlaylist()) {
        return;
    }

    const auto selected  = filterSelectedIndexes(m_playlistView);
    const UId playlistId = m_playlistController->currentPlaylist()->id();

    QueueTracks tracks;

    for(const QModelIndex& index : selected) {
        if(index.isValid() && index.data(PlaylistItem::Type).toInt() == PlaylistItem::Track) {
            auto track = index.data(PlaylistItem::ItemData).value<Track>();
            if(track.isValid()) {
                tracks.emplace_back(track, playlistId, index.data(PlaylistItem::Index).toInt());
            }
        }
    }

    m_playerController->dequeueTracks(tracks);
    m_removeFromQueueAction->setVisible(false);
}

void PlaylistWidgetPrivate::scanDroppedTracks(const QList<QUrl>& urls, int index)
{
    if(!m_playlistController->currentPlaylist()) {
        return;
    }

    m_playlistView->setFocus(Qt::ActiveWindowFocusReason);

    m_playlistInteractor->filesToTracks(urls, [this, index](const TrackList& tracks) {
        auto* insertCmd = new InsertTracks(m_playerController, m_model, m_playlistController->currentPlaylist()->id(),
                                           {{index, tracks}});
        m_playlistController->addToHistory(insertCmd);
    });
}

void PlaylistWidgetPrivate::tracksInserted(const TrackGroups& tracks) const
{
    if(!m_playlistController->currentPlaylist()) {
        return;
    }

    auto* insertCmd
        = new InsertTracks(m_playerController, m_model, m_playlistController->currentPlaylist()->id(), tracks);
    m_playlistController->addToHistory(insertCmd);

    m_playlistView->setFocus(Qt::ActiveWindowFocusReason);
}

void PlaylistWidgetPrivate::tracksRemoved() const
{
    if(!m_playlistController->currentPlaylist()) {
        return;
    }

    m_playlistController->playlistHandler()->clearSchedulePlaylist();

    const auto selected = filterSelectedIndexes(m_playlistView);

    PlaylistCommand* delCmd{nullptr};

    if(selected.size() == m_playlistController->currentPlaylist()->trackCount()) {
        delCmd = new ResetTracks(m_playerController, m_model, m_playlistController->currentPlaylistId(),
                                 m_playlistController->currentPlaylist()->tracks(), {});
    }
    else {
        QModelIndexList trackSelection;
        std::vector<int> indexes;

        for(const QModelIndex& index : selected) {
            if(index.isValid() && index.data(PlaylistItem::Type).toInt() == PlaylistItem::Track) {
                trackSelection.push_back(index);
                indexes.emplace_back(index.data(PlaylistItem::Index).toInt());
            }
        }

        if(selected.size() > 500) {
            // Faster to reset
            const TrackList oldTracks = m_playlistController->currentPlaylist()->tracks();
            m_playlistController->playlistHandler()->removePlaylistTracks(m_playlistController->currentPlaylist()->id(),
                                                                          indexes);
            delCmd = new ResetTracks(m_playerController, m_model, m_playlistController->currentPlaylistId(), oldTracks,
                                     m_playlistController->currentPlaylist()->tracks());
        }
        else {
            m_playlistController->playlistHandler()->removePlaylistTracks(m_playlistController->currentPlaylist()->id(),
                                                                          indexes);
            delCmd = new RemoveTracks(m_playerController, m_model, m_playlistController->currentPlaylist()->id(),
                                      PlaylistModel::saveTrackGroups(trackSelection));
        }
    }

    m_playlistController->addToHistory(delCmd);

    m_model->updateHeader(m_playlistController->currentPlaylist());
}

void PlaylistWidgetPrivate::tracksMoved(const MoveOperation& operation) const
{
    if(!m_playlistController->currentPlaylist()) {
        return;
    }

    auto* moveCmd
        = new MoveTracks(m_playerController, m_model, m_playlistController->currentPlaylist()->id(), operation);
    m_playlistController->addToHistory(moveCmd);

    m_playlistView->setFocus(Qt::ActiveWindowFocusReason);
}

void PlaylistWidgetPrivate::clearTracks() const
{
    if(!m_playlistController->currentPlaylist()) {
        return;
    }

    const TrackList oldTracks = m_playlistController->currentPlaylist()->tracks();
    auto* clearCmd
        = new ResetTracks(m_playerController, m_model, m_playlistController->currentPlaylistId(), oldTracks, {});
    m_playlistController->addToHistory(clearCmd);
}

void PlaylistWidgetPrivate::cutTracks() const
{
    if(!m_playlistController->currentPlaylist()) {
        return;
    }

    const auto selected    = filterSelectedIndexes(m_playlistView);
    const TrackList tracks = getTracks(selected);

    m_playlistController->setClipboard(tracks);
    tracksRemoved();
}

void PlaylistWidgetPrivate::copyTracks() const
{
    if(!m_playlistController->currentPlaylist()) {
        return;
    }

    const auto selected    = filterSelectedIndexes(m_playlistView);
    const TrackList tracks = getTracks(selected);

    m_playlistController->setClipboard(tracks);
}

void PlaylistWidgetPrivate::pasteTracks() const
{
    if(m_playlistController->clipboardEmpty()) {
        return;
    }

    const auto selected = filterSelectedIndexes(m_playlistView);
    const auto tracks   = m_playlistController->clipboard();

    int insertIndex{-1};
    if(!selected.empty()) {
        insertIndex = selected.front().data(PlaylistItem::Index).toInt();
    }

    auto* insertCmd = new InsertTracks(m_playerController, m_model, m_playlistController->currentPlaylist()->id(),
                                       {{insertIndex, tracks}});
    m_playlistController->addToHistory(insertCmd);

    QObject::connect(
        m_model, &PlaylistModel::playlistTracksChanged, m_playlistView,
        [this, insertIndex]() {
            const QModelIndex index = m_model->indexAtPlaylistIndex(insertIndex, true);
            if(index.isValid()) {
                m_playlistView->scrollTo(index, QAbstractItemView::EnsureVisible);
                m_playlistView->setCurrentIndex(index);
            }
        },
        static_cast<Qt::ConnectionType>(Qt::SingleShotConnection | Qt::QueuedConnection));
}

void PlaylistWidgetPrivate::playlistTracksAdded(const TrackList& tracks, int index) const
{
    auto* insertCmd = new InsertTracks(m_playerController, m_model, m_playlistController->currentPlaylist()->id(),
                                       {{index, tracks}});
    m_playlistController->addToHistory(insertCmd);
}

void PlaylistWidgetPrivate::handleTracksChanged(const std::vector<int>& indexes, bool allNew)
{
    if(!m_sortingColumn) {
        resetSort(true);
    }

    saveState(m_playlistController->currentPlaylist());

    auto restoreSelection = [this](const int currentIndex, const std::vector<int>& selectedIndexes) {
        restoreState(m_playlistController->currentPlaylist());
        restoreSelectedPlaylistIndexes(selectedIndexes);

        if(currentIndex >= 0) {
            const QModelIndex index = m_model->indexAtPlaylistIndex(currentIndex);
            if(index.isValid()) {
                m_playlistView->selectionModel()->setCurrentIndex(index, QItemSelectionModel::NoUpdate);
            }
        }
    };

    const auto changedTrackCount = static_cast<int>(indexes.size());
    // It's faster to just reset if we're going to be updating more than half the playlist
    // or we're updating a large number of tracks
    if(changedTrackCount > 500 || changedTrackCount > (m_playlistController->currentPlaylist()->trackCount() / 2)) {
        std::vector<int> selectedIndexes;

        if(!allNew) {
            selectedIndexes = selectedPlaylistIndexes();

            QObject::connect(
                m_model, &PlaylistModel::playlistTracksChanged, this,
                [restoreSelection, selectedIndexes]() { restoreSelection(-1, selectedIndexes); },
                Qt::SingleShotConnection);
        }

        resetModel();
    }
    else {
        int currentIndex{-1};
        if(m_playlistView->currentIndex().isValid()) {
            currentIndex = m_playlistView->currentIndex().data(PlaylistItem::Index).toInt();
        }
        const std::vector<int> selectedIndexes = selectedPlaylistIndexes();

        QObject::connect(
            m_model, &PlaylistModel::playlistTracksChanged, this,
            [restoreSelection, currentIndex, selectedIndexes]() { restoreSelection(currentIndex, selectedIndexes); },
            Qt::SingleShotConnection);

        m_model->updateTracks(indexes);
    }
}

void PlaylistWidgetPrivate::handlePlayingTrackChanged(const PlaylistTrack& track) const
{
    m_model->playingTrackChanged(track);
    if(m_settings->value<Settings::Gui::CursorFollowsPlayback>()) {
        followCurrentTrack();
    }
}

void PlaylistWidgetPrivate::selectTrackIds(const std::vector<int>& ids) const
{
    QItemSelection selection;

    for(const int id : ids) {
        const QModelIndexList trackIndexes = m_model->indexesOfTrackId(id);
        for(const QModelIndex& index : trackIndexes) {
            selection.select(index, index);
        }
    }

    m_playlistView->selectionModel()->select(selection,
                                             QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    if(!selection.empty()) {
        m_playlistView->scrollTo(selection.indexes().front(), QAbstractItemView::PositionAtCenter);
    }
}

void PlaylistWidgetPrivate::setSingleMode(bool enabled)
{
    if(std::exchange(m_singleMode, enabled) != m_singleMode && m_singleMode) {
        m_headerState = m_header->saveHeaderState();
        // Avoid display issues in case first section has been hidden
        m_header->showSection(m_header->logicalIndex(0));
    }

    m_header->setSectionsClickable(!m_singleMode);
    m_header->setSortIndicatorShown(!m_singleMode);

    if(!m_singleMode) {
        if(m_columns.empty()) {
            if(const auto column = m_columnRegistry->itemById(8)) {
                m_columns.push_back(column.value());
            }
            if(const auto column = m_columnRegistry->itemById(3)) {
                m_columns.push_back(column.value());
            }
            if(const auto column = m_columnRegistry->itemById(0)) {
                m_columns.push_back(column.value());
            }
            if(const auto column = m_columnRegistry->itemById(1)) {
                m_columns.push_back(column.value());
            }
            if(const auto column = m_columnRegistry->itemById(7)) {
                m_columns.push_back(column.value());
            }
        }

        auto resetColumns = [this]() {
            m_model->resetColumnAlignments();
            m_header->resetSectionPositions();
            m_header->setHeaderSectionWidths({{0, 0.06}, {1, 0.38}, {2, 0.08}, {3, 0.38}, {4, 0.10}});
            m_model->changeColumnAlignment(0, Qt::AlignCenter);
            m_model->changeColumnAlignment(2, Qt::AlignRight);
            m_model->changeColumnAlignment(4, Qt::AlignRight);
        };

        auto resetState = [this, resetColumns]() {
            if(!m_headerState.isEmpty()) {
                m_header->restoreHeaderState(m_headerState);
            }
            else {
                resetColumns();
            }
            updateSpans();
        };

        if(std::cmp_equal(m_header->count(), m_columns.size())) {
            resetColumns();
            updateSpans();
        }
        else {
            QObject::connect(
                m_model, &QAbstractItemModel::modelReset, m_self, [resetState]() { resetState(); },
                Qt::SingleShotConnection);
        }
    }

    resetModel();
}

void PlaylistWidgetPrivate::updateSpans()
{
    auto isPixmap = [](const QString& field) {
        return field == QLatin1String(FrontCover) || field == QLatin1String(BackCover)
            || field == QLatin1String(ArtistPicture);
    };

    bool hasRating{false};
    for(int i{0}; const auto& column : m_columns) {
        if(isPixmap(column.field)) {
            m_playlistView->setSpan(i, true);
        }
        else {
            m_playlistView->setSpan(i, false);
        }

        if(column.field == QLatin1String{RatingEditor}) {
            hasRating = true;
            if(!m_starDelegate) {
                m_starDelegate = new StarDelegate(this);
            }
            m_playlistView->setItemDelegateForColumn(i, m_starDelegate);
        }
        else {
            m_playlistView->setItemDelegateForColumn(i, m_delgate);
        }
        ++i;
    }

    if(!hasRating && m_starDelegate) {
        m_starDelegate->deleteLater();
        m_starDelegate = nullptr;
    }
}

void PlaylistWidgetPrivate::customHeaderMenuRequested(const QPoint& pos)
{
    auto* menu = new QMenu(m_self);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    if(!m_singleMode) {
        addColumnsMenu(menu);

        menu->addSeparator();
        m_header->addHeaderContextMenu(menu, m_self->mapToGlobal(pos));
        menu->addSeparator();
        m_header->addHeaderAlignmentMenu(menu, m_self->mapToGlobal(pos));

        auto* resetAction = new QAction(tr("Reset columns to default"), menu);
        QObject::connect(resetAction, &QAction::triggered, m_self, [this]() {
            m_columns.clear();
            m_headerState.clear();
            setSingleMode(false);
        });
        menu->addAction(resetAction);
    }

    auto* columnModeAction = new QAction(tr("Single-column mode"), menu);
    columnModeAction->setCheckable(true);
    columnModeAction->setChecked(m_singleMode);
    QObject::connect(columnModeAction, &QAction::triggered, m_self, [this]() { setSingleMode(!m_singleMode); });
    menu->addAction(columnModeAction);

    menu->addSeparator();
    addPresetMenu(menu);

    menu->popup(m_self->mapToGlobal(pos));
}

void PlaylistWidgetPrivate::doubleClicked(const QModelIndex& index) const
{
    if(index.isValid()) {
        m_selectionController->executeAction(TrackAction::Play);
        m_playlistView->clearSelection();
    }
}

void PlaylistWidgetPrivate::middleClicked(const QModelIndex& /*index*/) const
{
    if(m_middleClickAction == TrackAction::None) {
        return;
    }

    queueSelectedTracks(m_middleClickAction == TrackAction::SendToQueue);
}

void PlaylistWidgetPrivate::followCurrentTrack() const
{
    if(m_playlistController->playState() != Player::PlayState::Playing) {
        return;
    }

    const auto [track, playlistId, index] = m_model->playingTrack();

    if(!track.isValid() || playlistId != m_playlistController->currentPlaylistId()) {
        return;
    }

    QModelIndex modelIndex = m_model->indexAtPlaylistIndex(index);
    while(modelIndex.isValid() && m_header->isSectionHidden(modelIndex.column())) {
        modelIndex = modelIndex.siblingAtColumn(modelIndex.column() + 1);
    }

    if(!modelIndex.isValid()) {
        return;
    }

    const QRect indexRect    = m_playlistView->visualRect(modelIndex);
    const QRect viewportRect = m_playlistView->viewport()->rect();

    if((indexRect.top() < 0) || (viewportRect.bottom() - indexRect.bottom() < 0)) {
        m_playlistView->scrollTo(modelIndex, QAbstractItemView::PositionAtCenter);
    }

    m_playlistView->setCurrentIndex(modelIndex);
}

void PlaylistWidgetPrivate::cropSelection() const
{
    if(!m_playlistController->currentPlaylist()) {
        return;
    }

    m_playlistController->playlistHandler()->clearSchedulePlaylist();

    const auto selected = filterSelectedIndexes(m_playlistView);

    const int selectedCount      = static_cast<int>(selected.size());
    const int playlistTrackCount = m_playlistController->currentPlaylist()->trackCount();

    if(selectedCount >= playlistTrackCount) {
        // Nothing to do
        return;
    }

    QModelIndexList allTrackIndexes;
    getAllTrackIndexes(m_model, {}, allTrackIndexes);

    QModelIndexList tracksToRemove;
    std::vector<int> indexes;

    for(const QModelIndex& index : selected) {
        if(index.isValid() && index.data(PlaylistItem::Type).toInt() == PlaylistItem::Track) {
            allTrackIndexes.removeAll(index);
        }
    }

    for(const QModelIndex& index : std::as_const(allTrackIndexes)) {
        indexes.emplace_back(index.data(PlaylistItem::Index).toInt());
        tracksToRemove.push_back(index);
    }

    const TrackList oldTracks = m_playlistController->currentPlaylist()->tracks();

    m_playlistController->playlistHandler()->removePlaylistTracks(m_playlistController->currentPlaylist()->id(),
                                                                  indexes);

    if(selectedCount > 500 || playlistTrackCount - selectedCount > 500) {
        // Faster to reset
        auto* resetCmd = new ResetTracks(m_playerController, m_model, m_playlistController->currentPlaylistId(),
                                         oldTracks, m_playlistController->currentPlaylist()->tracks());
        m_playlistController->addToHistory(resetCmd);
    }
    else {
        auto* delCmd = new RemoveTracks(m_playerController, m_model, m_playlistController->currentPlaylist()->id(),
                                        PlaylistModel::saveTrackGroups(tracksToRemove));
        m_playlistController->addToHistory(delCmd);
    }

    m_model->updateHeader(m_playlistController->currentPlaylist());
}

void PlaylistWidgetPrivate::sortTracks(const QString& script)
{
    if(!m_playlistController->currentPlaylist()) {
        return;
    }

    auto* currentPlaylist    = m_playlistController->currentPlaylist();
    const auto currentTracks = currentPlaylist->tracks();

    auto handleSortedTracks = [this, currentPlaylist](const TrackList& sortedTracks) {
        auto* sortCmd = new ResetTracks(m_playerController, m_model, currentPlaylist->id(), currentPlaylist->tracks(),
                                        sortedTracks);
        m_playlistController->addToHistory(sortCmd);
    };

    if(m_playlistView->selectionModel()->hasSelection()) {
        const auto selected = filterSelectedIndexes(m_playlistView);

        std::vector<int> indexesToSort;

        for(const QModelIndex& index : selected) {
            if(index.data(PlaylistItem::Type).toInt() == PlaylistItem::Track) {
                indexesToSort.push_back(index.data(PlaylistItem::Index).toInt());
            }
        }

        std::ranges::sort(indexesToSort);

        Utils::asyncExec([this, currentTracks, script, indexesToSort]() {
            return m_sorter.calcSortTracks(script, currentTracks, indexesToSort);
        }).then(m_self, handleSortedTracks);
    }
    else {
        Utils::asyncExec([this, currentTracks, script]() {
            return m_sorter.calcSortTracks(script, currentTracks);
        }).then(m_self, handleSortedTracks);
    }
}

void PlaylistWidgetPrivate::sortColumn(int column, Qt::SortOrder order)
{
    if(!m_playlistController->currentPlaylist() || column < 0 || std::cmp_greater_equal(column, m_columns.size())) {
        return;
    }

    m_sorting       = true;
    m_sortingColumn = true;

    auto* currentPlaylist    = m_playlistController->currentPlaylist();
    const auto currentTracks = currentPlaylist->tracks();
    const QString sortField  = m_columns.at(column).field;

    Utils::asyncExec([this, sortField, currentTracks, order]() {
        return m_sorter.calcSortTracks(sortField, currentTracks, order);
    }).then(m_self, [this, currentPlaylist](const TrackList& sortedTracks) {
        auto* sortCmd = new ResetTracks(m_playerController, m_model, currentPlaylist->id(), currentPlaylist->tracks(),
                                        sortedTracks);
        m_playlistController->addToHistory(sortCmd);
    });
}

void PlaylistWidgetPrivate::resetSort(bool force)
{
    if(force) {
        m_sorting = false;
    }

    if(!m_sorting) {
        m_header->setSortIndicator(-1, Qt::AscendingOrder);
    }
}

void PlaylistWidgetPrivate::addSortMenu(QMenu* parent, bool disabled)
{
    auto* sortMenu = new QMenu(PlaylistWidget::tr("Sort"), parent);

    if(disabled) {
        sortMenu->setEnabled(false);
    }
    else {
        const auto& groups = m_sortRegistry->items();
        for(const auto& script : groups) {
            auto* switchSort = new QAction(script.name, sortMenu);
            QObject::connect(switchSort, &QAction::triggered, m_self, [this, script]() { sortTracks(script.script); });
            sortMenu->addAction(switchSort);
        }
    }

    parent->addMenu(sortMenu);
}

void PlaylistWidgetPrivate::addClipboardMenu(QMenu* parent, bool hasSelection) const
{
    if(hasSelection) {
        parent->addAction(m_cutAction);
        parent->addAction(m_copyAction);
    }

    if(!m_playlistController->clipboardEmpty()) {
        parent->addAction(m_pasteAction);
    }
}

void PlaylistWidgetPrivate::addPresetMenu(QMenu* parent)
{
    auto* presetsMenu = new QMenu(PlaylistWidget::tr("Presets"), parent);

    const auto presets = m_presetRegistry->items();
    for(const auto& preset : presets) {
        auto* switchPreset = new QAction(preset.name, presetsMenu);
        if(preset == m_currentPreset) {
            presetsMenu->setDefaultAction(switchPreset);
        }

        const int presetId = preset.id;
        QObject::connect(switchPreset, &QAction::triggered, m_self, [this, presetId]() {
            if(const auto item = m_presetRegistry->itemById(presetId)) {
                changePreset(item.value());
            }
        });

        presetsMenu->addAction(switchPreset);
    }

    parent->addMenu(presetsMenu);
}

void PlaylistWidgetPrivate::addColumnsMenu(QMenu* parent)
{
    auto* columnsMenu = new QMenu(tr("Columns"), parent);
    auto* columnGroup = new QActionGroup{parent};
    columnGroup->setExclusionPolicy(QActionGroup::ExclusionPolicy::None);

    auto hasColumn = [this](int id) {
        return std::ranges::any_of(m_columns, [id](const PlaylistColumn& column) { return column.id == id; });
    };

    for(const auto& column : m_columnRegistry->items()) {
        auto* columnAction = new QAction(column.name, columnsMenu);
        columnAction->setData(column.id);
        columnAction->setCheckable(true);
        columnAction->setChecked(hasColumn(column.id));
        columnsMenu->addAction(columnAction);
        columnGroup->addAction(columnAction);
    }

    QObject::connect(columnGroup, &QActionGroup::triggered, m_self, [this](QAction* action) {
        const int columnId = action->data().toInt();
        if(action->isChecked()) {
            if(const auto column = m_columnRegistry->itemById(action->data().toInt())) {
                m_columns.push_back(column.value());
                updateSpans();
                changePreset(m_currentPreset);
            }
        }
        else {
            auto colIt
                = std::ranges::find_if(m_columns, [columnId](const PlaylistColumn& col) { return col.id == columnId; });
            if(colIt != m_columns.end()) {
                const int removedIndex = static_cast<int>(std::distance(m_columns.begin(), colIt));
                m_columns.erase(colIt);
                updateSpans();
                m_model->removeColumn(removedIndex);
            }
        }
    });

    auto* moreSettings = new QAction(tr("More…"), columnsMenu);
    QObject::connect(moreSettings, &QAction::triggered, m_self,
                     [this]() { m_settingsDialog->openAtPage(Constants::Page::PlaylistColumns); });
    columnsMenu->addSeparator();
    columnsMenu->addAction(moreSettings);

    parent->addMenu(columnsMenu);
}

PlaylistWidget::PlaylistWidget(ActionManager* actionManager, PlaylistInteractor* playlistInteractor,
                               CoverProvider* coverProvider, Application* core, QWidget* parent)
    : FyWidget{parent}
    , p{std::make_unique<PlaylistWidgetPrivate>(this, actionManager, playlistInteractor, coverProvider, core)}
{
    setObjectName(PlaylistWidget::name());
    setFeature(Search);
}

PlaylistWidget::~PlaylistWidget()
{
    if(p->m_detached || !p->m_playlistController->currentPlaylist()) {
        return;
    }

    p->m_playlistController->clearHistory();
    p->m_playlistController->savePlaylistState(p->m_playlistController->currentPlaylist(),
                                               p->getState(p->m_playlistController->currentPlaylist()));
}

void PlaylistWidget::setDetached(bool detached)
{
    p->m_detached = detached;
    p->m_playlistView->viewport()->setAcceptDrops(!detached);
}

PlaylistView* PlaylistWidget::view() const
{
    return p->m_playlistView;
}

PlaylistModel* PlaylistWidget::model() const
{
    return p->m_model;
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
    layout[u"Preset"]     = p->m_currentPreset.id;
    layout[u"SingleMode"] = p->m_singleMode;

    if(!p->m_columns.empty()) {
        QStringList columns;

        for(int i{0}; const auto& column : p->m_columns) {
            const auto alignment = p->m_model->columnAlignment(i++);
            QString colStr       = QString::number(column.id);

            if(alignment != Qt::AlignLeft) {
                colStr += u":" + QString::number(alignment.toInt());
            }

            columns.push_back(colStr);
        }

        layout[u"Columns"] = columns.join(QStringLiteral("|"));
    }

    p->resetSort();

    if(!p->m_singleMode || !p->m_headerState.isEmpty()) {
        QByteArray state
            = p->m_singleMode && !p->m_headerState.isEmpty() ? p->m_headerState : p->m_header->saveHeaderState();
        state                  = qCompress(state, 9);
        layout[u"HeaderState"] = QString::fromUtf8(state.toBase64());
    }
}

void PlaylistWidget::loadLayoutData(const QJsonObject& layout)
{
    if(layout.contains(u"Preset")) {
        const int presetId = layout.value(u"Preset").toInt();
        if(const auto preset = p->m_presetRegistry->itemById(presetId)) {
            p->m_currentPreset = preset.value();
        }
    }
    if(layout.contains(u"SingleMode")) {
        p->m_singleMode = layout.value(u"SingleMode").toBool();
    }

    if(layout.contains(u"Columns")) {
        p->m_columns.clear();

        const QString columnData    = layout.value(u"Columns").toString();
        const QStringList columnIds = columnData.split(u'|');

        for(int i{0}; const auto& columnId : columnIds) {
            const auto column = columnId.split(u':');

            if(const auto columnItem = p->m_columnRegistry->itemById(column.at(0).toInt())) {
                p->m_columns.push_back(columnItem.value());

                if(column.size() > 1) {
                    p->m_model->changeColumnAlignment(i, static_cast<Qt::Alignment>(column.at(1).toInt()));
                }
                else {
                    p->m_model->changeColumnAlignment(i, Qt::AlignLeft);
                }
            }
            ++i;
        }

        p->updateSpans();
    }

    if(layout.contains(u"HeaderState")) {
        const auto headerState = layout.value(u"HeaderState").toString().toUtf8();

        if(!headerState.isEmpty()
#if QT_VERSION >= QT_VERSION_CHECK(6, 3, 0)
           && headerState.isValidUtf8()
#endif
        ) {
            const auto state = QByteArray::fromBase64(headerState);
            p->m_headerState = qUncompress(state);
        }
    }
}

void PlaylistWidget::finalise()
{
    p->setupConnections();
    p->setupActions();

    if(!p->m_currentPreset.isValid()) {
        if(const auto preset = p->m_presetRegistry->itemById(p->m_settings->fileValue(PlaylistCurrentPreset).toInt())) {
            p->m_currentPreset = preset.value();
        }
    }

    p->m_header->setSectionsClickable(!p->m_singleMode);
    p->m_header->setSortIndicatorShown(!p->m_singleMode);

    if(!p->m_singleMode && !p->m_columns.empty() && !p->m_headerState.isEmpty()) {
        p->m_header->restoreHeaderState(p->m_headerState);
    }

    if(!p->m_singleMode && p->m_columns.empty()) {
        p->setSingleMode(false);
        p->m_model->reset({});
    }
    else if(p->m_playlistController->currentPlaylist()) {
        p->changePlaylist(nullptr, p->m_playlistController->currentPlaylist());
    }
}

void PlaylistWidget::searchEvent(const QString& search)
{
    const auto prevSearch = std::exchange(p->m_search, search);

    if(search.length() < 3) {
        p->m_search.clear();
    }

    auto selectTracks = [this](const TrackList& tracks) {
        std::vector<int> trackIds;
        for(const Track& track : tracks) {
            trackIds.emplace_back(track.id());
        }
        p->selectTrackIds(trackIds);
    };

    auto handleFilteredTracks = [this, selectTracks](const TrackList& filteredTracks) {
        p->m_filteredTracks = filteredTracks;
        if(p->m_detached) {
            p->m_model->reset(p->m_currentPreset, p->m_columns, p->m_playlistController->currentPlaylist(),
                              filteredTracks);
        }
        else {
            selectTracks(filteredTracks);
        }
    };

    auto filterAndHandleTracks = [this, handleFilteredTracks](const TrackList& tracks) {
        Utils::asyncExec([search = p->m_search, tracks]() {
            return Filter::filterTracks(tracks, search);
        }).then(this, handleFilteredTracks);
    };

    if(!prevSearch.isEmpty() && p->m_search.length() > prevSearch.length()) {
        filterAndHandleTracks(p->m_filteredTracks);
    }
    else if(!p->m_search.isEmpty()) {
        if(const auto* playlist = p->m_playlistController->currentPlaylist()) {
            filterAndHandleTracks(playlist->tracks());
        }
    }
    else {
        if(p->m_detached) {
            p->m_model->reset({});
        }
        else {
            selectTracks({});
        }
    }
}

void PlaylistWidget::contextMenuEvent(QContextMenuEvent* event)
{
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    const auto selected     = filterSelectedIndexes(p->m_playlistView);
    const bool hasSelection = !selected.empty();

    if(hasSelection) {
        menu->addAction(p->m_playAction);

        if(!p->m_detached && p->m_playlistController->currentIsActive()) {
            const auto currentIndex = p->m_playlistView->currentIndex();
            if(currentIndex.isValid() && currentIndex.data(PlaylistItem::Type).toInt() == PlaylistItem::Track) {
                menu->addAction(p->m_stopAfter);
            }
        }

        menu->addSeparator();

        if(!p->m_detached) {
            if(auto* removeCmd = p->m_actionManager->command(Constants::Actions::Remove)) {
                menu->addAction(removeCmd->action());
            }

            menu->addAction(p->m_cropAction);

            p->addSortMenu(menu, selected.size() == 1);
            menu->addSeparator();
        }
    }

    if(!p->m_detached) {
        p->addClipboardMenu(menu, hasSelection);
        menu->addSeparator();
    }

    p->addPresetMenu(menu);
    menu->addSeparator();

    if(hasSelection) {
        if(!p->m_detached) {
            if(auto* addQueueCmd = p->m_actionManager->command(Constants::Actions::AddToQueue)) {
                menu->addAction(addQueueCmd->action());
            }

            if(auto* removeQueueCmd = p->m_actionManager->command(Constants::Actions::RemoveFromQueue)) {
                menu->addAction(removeQueueCmd->action());
            }

            menu->addSeparator();
        }
        p->m_selectionController->addTrackContextMenu(menu);
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
        if(p->m_selectionController->hasTracks()) {
            p->m_selectionController->executeAction(TrackAction::Play);
        }
        p->m_playlistView->clearSelection();
    }

    QWidget::keyPressEvent(event);
}
} // namespace Fooyin

#include "moc_playlistwidget.cpp"
