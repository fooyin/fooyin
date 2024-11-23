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
#include <core/constants.h>
#include <core/coresettings.h>
#include <core/library/musiclibrary.h>
#include <core/library/tracksort.h>
#include <core/player/playercontroller.h>
#include <core/playlist/playlisthandler.h>
#include <core/plugins/coreplugincontext.h>
#include <gui/guiconstants.h>
#include <gui/guisettings.h>
#include <gui/trackselectioncontroller.h>
#include <gui/widgets/autoheaderview.h>
#include <utils/actions/actioncontainer.h>
#include <utils/actions/actionmanager.h>
#include <utils/actions/command.h>
#include <utils/async.h>
#include <utils/modelutils.h>
#include <utils/settings/settingsdialogcontroller.h>
#include <utils/signalthrottler.h>
#include <utils/stardelegate.h>
#include <utils/tooltipfilter.h>
#include <utils/utils.h>

#include <QActionGroup>
#include <QApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonObject>
#include <QKeyEvent>
#include <QMenu>
#include <QScrollBar>
#include <QSortFilterProxyModel>

#include <stack>

using namespace Qt::StringLiterals;

namespace {
Fooyin::TrackList getTracks(const QModelIndexList& indexes)
{
    Fooyin::TrackList tracks;

    for(const QModelIndex& index : indexes) {
        if(index.isValid() && index.data(Fooyin::PlaylistItem::Type).toInt() == Fooyin::PlaylistItem::Track) {
            auto track = index.data(Fooyin::PlaylistItem::ItemData).value<Fooyin::PlaylistTrack>();
            if(track.isValid()) {
                tracks.push_back(track.track);
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

        const int rowCount = model->rowCount(currentIndex);
        if(rowCount == 0
           && currentIndex.data(Fooyin::PlaylistItem::Role::Type).toInt() == Fooyin::PlaylistItem::Track) {
            auto track = currentIndex.data(Fooyin::PlaylistItem::ItemData).value<Fooyin::PlaylistTrack>();
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

void getAllTrackIndexes(QAbstractItemModel* model, const QModelIndex& parent, QModelIndexList& indexList)
{
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
                                             Application* core, PlaylistWidget::Mode mode)
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
    , m_mode{mode}
    , m_resetThrottler{new SignalThrottler(m_self)}
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
    , m_playlistContext{new WidgetContext(m_self, Context{Id{Constants::Context::Playlist}.append(m_self->id())},
                                          m_self)}
    , m_middleClickAction{static_cast<TrackAction>(m_settings->value<PlaylistMiddleClick>())}
    , m_undoAction{nullptr}
    , m_redoAction{nullptr}
    , m_playAction{new QAction(Utils::iconFromTheme(Constants::Icons::Play), tr("&Play"), m_self)}
    , m_cropAction{new QAction(tr("&Crop"), m_self)}
    , m_stopAfter{new QAction(Utils::iconFromTheme(Constants::Icons::Stop), tr("&Stop after this"), m_self)}
    , m_cutAction{new QAction(tr("Cu&t"), m_self)}
    , m_copyAction{new QAction(tr("&Copy"), m_self)}
    , m_pasteAction{new QAction(tr("&Paste"), m_self)}
    , m_clearAction{new QAction(tr("C&lear"), m_self)}
    , m_removeTrackAction{new QAction(Utils::iconFromTheme(Constants::Icons::Remove), tr("&Remove"), m_self)}
    , m_addToQueueAction{new QAction(Utils::iconFromTheme(Constants::Icons::Add), tr("Add to playback &queue"), m_self)}
    , m_queueNextAction{new QAction(tr("&Queue to play next"), m_self)}
    , m_removeFromQueueAction{new QAction(Utils::iconFromTheme(Constants::Icons::Remove),
                                          tr("Remove from playback q&ueue"), m_self)}
    , m_sorting{false}
    , m_sortingColumn{false}
    , m_showPlaying{false}
    , m_pendingFocus{false}
    , m_dropIndex{-1}
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
    m_playlistView->viewport()->setAcceptDrops(mode == PlaylistWidget::Mode::Playlist);
    m_playlistView->viewport()->installEventFilter(new ToolTipFilter(m_self));

    m_layout->addWidget(m_playlistView);

    setHeaderHidden(!m_settings->value<PlaylistHeader>());
    setScrollbarHidden(m_settings->value<PlaylistScrollBar>());
    m_playlistView->setAlternatingRowColors(m_settings->value<Settings::Gui::Internal::PlaylistAltColours>());

    m_model->playingTrackChanged(m_playlistController->currentTrack());
    m_model->playStateChanged(m_playlistController->playState());
    // QApplication::font only works from main thread,
    // so we have to pass this to the script formatter from here
    m_model->setFont(QApplication::font("Fooyin::PlaylistView"));

    setupView();
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

    QObject::connect(m_resetThrottler, &SignalThrottler::triggered, this, &PlaylistWidgetPrivate::resetModel);

    if(m_mode == PlaylistWidget::Mode::Playlist) {
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
        QObject::connect(m_playlistController, &PlaylistController::filterTracks, this, &PlaylistWidgetPrivate::filterTracks);
        QObject::connect(m_playlistController, &PlaylistController::requestPlaylistFocus, m_model, [this]() {
            if(m_playlistView->playlistLoaded() && !m_resetThrottler->isActive()) {
                m_playlistView->setFocus(Qt::ActiveWindowFocusReason);
                m_playlistView->setCurrentIndex(m_model->indexAtPlaylistIndex(0, false));
            } else {
                m_pendingFocus = true;
            }
        });
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
        m_settings->subscribe<PlaylistAltColours>(m_playlistView, &PlaylistView::setAlternatingRowColors);
    }
    // clang-format on

    auto handleStyleChange = [this]() {
        m_model->setFont(QApplication::font("Fooyin::PlaylistView"));
        resetModelThrottled();
    };
    m_settings->subscribe<Settings::Gui::Theme>(this, handleStyleChange);
    m_settings->subscribe<Settings::Gui::Style>(this, handleStyleChange);

    m_settings->subscribe<Settings::Core::UseVariousForCompilations>(
        m_self, [this]() { changePlaylist(m_playlistController->currentPlaylist(), nullptr); });
}

void PlaylistWidgetPrivate::setupActions()
{
    m_playAction->setStatusTip(tr("Start playback of the selected track"));
    QObject::connect(m_playAction, &QAction::triggered, this, [this]() { playSelectedTracks(); });

    auto* editMenu = m_actionManager->actionContainer(Constants::Menus::Edit);

    const QStringList editCategory = {tr("Edit")};

    if(m_mode == PlaylistWidget::Mode::Playlist) {
        m_cropAction->setStatusTip(tr("Remove all tracks from the playlist except for the selected tracks"));
        QObject::connect(m_cropAction, &QAction::triggered, this, [this]() { cropSelection(); });

        m_stopAfter->setStatusTip(tr("Stop playback at the end of the selected track"));
        QObject::connect(m_stopAfter, &QAction::triggered, this, [this]() { stopAfterTrack(); });

        m_actionManager->addContextObject(m_playlistContext);

        m_undoAction = new QAction(tr("&Undo"), this);
        m_undoAction->setStatusTip(tr("Undo the previous playlist change"));
        auto* undoCmd
            = m_actionManager->registerAction(m_undoAction, Constants::Actions::Undo, m_playlistContext->context());
        undoCmd->setCategories(editCategory);
        undoCmd->setDefaultShortcut(QKeySequence::Undo);
        editMenu->addAction(undoCmd);
        QObject::connect(m_undoAction, &QAction::triggered, this,
                         [this]() { m_playlistController->undoPlaylistChanges(); });
        QObject::connect(m_playlistController, &PlaylistController::playlistHistoryChanged, this,
                         [this]() { m_undoAction->setEnabled(m_playlistController->canUndo()); });
        m_undoAction->setEnabled(m_playlistController->canUndo());

        m_redoAction = new QAction(tr("&Redo"), this);
        m_redoAction->setStatusTip(tr("Redo the previous playlist change"));
        auto* redoCmd
            = m_actionManager->registerAction(m_redoAction, Constants::Actions::Redo, m_playlistContext->context());
        redoCmd->setCategories(editCategory);
        redoCmd->setDefaultShortcut(QKeySequence::Redo);
        editMenu->addAction(redoCmd);
        QObject::connect(m_redoAction, &QAction::triggered, this,
                         [this]() { m_playlistController->redoPlaylistChanges(); });
        QObject::connect(m_playlistController, &PlaylistController::playlistHistoryChanged, this,
                         [this]() { m_redoAction->setEnabled(m_playlistController->canRedo()); });
        m_redoAction->setEnabled(m_playlistController->canRedo());

        editMenu->addSeparator();

        m_cutAction->setStatusTip(tr("Cut the selected tracks"));
        auto* cutCommand
            = m_actionManager->registerAction(m_cutAction, Constants::Actions::Cut, m_playlistContext->context());
        cutCommand->setCategories(editCategory);
        cutCommand->setDefaultShortcut(QKeySequence::Cut);
        editMenu->addAction(cutCommand);
        QObject::connect(m_cutAction, &QAction::triggered, this, &PlaylistWidgetPrivate::cutTracks);
        m_cutAction->setEnabled(m_playlistView->selectionModel()->hasSelection());

        m_copyAction->setStatusTip(tr("Copy the selected tracks"));
        auto* copyCommand
            = m_actionManager->registerAction(m_copyAction, Constants::Actions::Copy, m_playlistContext->context());
        copyCommand->setCategories(editCategory);
        copyCommand->setDefaultShortcut(QKeySequence::Copy);
        editMenu->addAction(copyCommand);
        QObject::connect(m_copyAction, &QAction::triggered, this, &PlaylistWidgetPrivate::copyTracks);
        m_copyAction->setEnabled(m_playlistView->selectionModel()->hasSelection());

        m_pasteAction->setStatusTip(tr("Paste the selected tracks"));
        auto* pasteCommand
            = m_actionManager->registerAction(m_pasteAction, Constants::Actions::Paste, m_playlistContext->context());
        pasteCommand->setCategories(editCategory);
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
        clearCmd->setCategories(editCategory);
        clearCmd->setAttribute(ProxyAction::UpdateText);
        editMenu->addAction(clearCmd);
        QObject::connect(m_clearAction, &QAction::triggered, this, [this]() { clearTracks(); });
        m_clearAction->setEnabled(m_model->rowCount({}) > 0);
    }

    auto* selectAllAction = new QAction(tr("Select &all"), m_self);
    selectAllAction->setStatusTip(tr("Select all tracks in the current playlist"));
    auto* selectAllCmd
        = m_actionManager->registerAction(selectAllAction, Constants::Actions::SelectAll, m_playlistContext->context());
    selectAllCmd->setCategories(editCategory);
    selectAllCmd->setDefaultShortcut(QKeySequence::SelectAll);
    editMenu->addAction(selectAllCmd);
    QObject::connect(selectAllAction, &QAction::triggered, this, [this]() { selectAll(); });

    if(m_mode == PlaylistWidget::Mode::Playlist) {
        m_removeTrackAction->setStatusTip(tr("Remove the selected tracks from the current playlist"));
        m_removeTrackAction->setEnabled(false);
        auto* removeCmd = m_actionManager->registerAction(m_removeTrackAction, Constants::Actions::Remove,
                                                          m_playlistContext->context());
        removeCmd->setCategories(editCategory);
        removeCmd->setAttribute(ProxyAction::UpdateText);
        removeCmd->setDefaultShortcut(QKeySequence::Delete);
        QObject::connect(m_removeTrackAction, &QAction::triggered, this, [this]() { tracksRemoved(); });

        m_addToQueueAction->setStatusTip(tr("Add the selected tracks to the playback queue"));
        m_addToQueueAction->setEnabled(false);
        m_actionManager->registerAction(m_addToQueueAction, Constants::Actions::AddToQueue,
                                        m_playlistContext->context());
        QObject::connect(m_addToQueueAction, &QAction::triggered, this, [this]() { queueSelectedTracks(false); });

        m_queueNextAction->setStatusTip(tr("Add the selected tracks to the start of the playback queue"));
        m_queueNextAction->setEnabled(false);
        m_actionManager->registerAction(m_queueNextAction, Constants::Actions::QueueNext, m_playlistContext->context());
        QObject::connect(m_queueNextAction, &QAction::triggered, this, [this]() { queueSelectedTracks(true); });

        m_removeFromQueueAction->setStatusTip(tr("Remove the selected tracks from the playback queue"));
        m_removeFromQueueAction->setVisible(false);
        m_actionManager->registerAction(m_removeFromQueueAction, Constants::Actions::RemoveFromQueue,
                                        m_playlistContext->context());
        QObject::connect(m_removeFromQueueAction, &QAction::triggered, this, [this]() { dequeueSelectedTracks(); });
    }
}

void PlaylistWidgetPrivate::setupView() const
{
    switch(m_mode) {
        case(PlaylistWidget::Mode::Playlist):
            m_playlistView->setEmptyText(tr("Playlist empty"));
            m_playlistView->setLoadingText(tr("Loading playlist…"));
            break;
        case(PlaylistWidget::Mode::DetachedPlaylist):
            m_playlistView->setEmptyText(tr("No results"));
            m_playlistView->setLoadingText(tr("Searching…"));
            break;
        case(PlaylistWidget::Mode::DetachedLibrary):
            m_playlistView->setEmptyText(tr("No results"));
            m_playlistView->setLoadingText(tr("Searching…"));
            break;
    }
}

void PlaylistWidgetPrivate::onColumnChanged(const PlaylistColumn& changedColumn)
{
    auto existingIt = std::find_if(m_columns.begin(), m_columns.end(), [&changedColumn](const auto& column) {
        return (column.isDefault && changedColumn.isDefault && column.name == changedColumn.name)
            || column.id == changedColumn.id;
    });

    if(existingIt != m_columns.end()) {
        *existingIt = changedColumn;
        resetModelThrottled();
    }
}

void PlaylistWidgetPrivate::onColumnRemoved(int id)
{
    PlaylistColumnList columns;
    std::ranges::copy_if(m_columns, std::back_inserter(columns), [id](const auto& column) { return column.id != id; });
    if(std::exchange(m_columns, columns) != columns) {
        resetModelThrottled();
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
    resetModelThrottled();
}

void PlaylistWidgetPrivate::changePlaylist(Playlist* prevPlaylist, Playlist* playlist)
{
    QObject::disconnect(m_delayedStateLoad);
    if(m_playlistView->playlistLoaded()) {
        saveState(prevPlaylist);
    }
    m_playlistController->setSearch(prevPlaylist, m_search);
    resetSort(true);
    m_filteredTracks.clear();

    m_search = m_playlistController->currentSearch(playlist);
    emit m_self->changeSearch(m_search);

    if(m_search.isEmpty()) {
        resetModelThrottled();
    }
}

void PlaylistWidgetPrivate::resetTree()
{
    resetSort();
    restoreState(m_playlistController->currentPlaylist());
    m_clearAction->setEnabled(!m_playlistController->currentIsAuto() && m_model->rowCount({}) > 0);

    if(m_pendingFocus) {
        m_pendingFocus = false;
        m_playlistView->setFocus(Qt::ActiveWindowFocusReason);
        m_playlistView->setCurrentIndex(m_model->indexAtPlaylistIndex(0, false));
    }
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

    if(!m_self->isVisible()) {
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
        const auto index = m_model->indexAtPlaylistIndex(state->topIndex, false);

        if(!index.isValid()) {
            return false;
        }

        m_playlistView->scrollTo(index, PlaylistView::PositionAtTop);
        m_playlistView->verticalScrollBar()->setValue(state->scrollPos);
        m_playlistView->playlistReset();

        if(std::exchange(m_showPlaying, false)) {
            followCurrentTrack();
        }
        return true;
    };

    if(!loadState()) {
        m_delayedStateLoad = QObject::connect(
            m_model, &PlaylistModel::playlistLoaded, this, [loadState]() { loadState(); }, Qt::SingleShotConnection);
    }
}

void PlaylistWidgetPrivate::resetModel() const
{
    m_playlistView->playlistAboutToBeReset();

    Playlist* currentPlaylist = m_playlistController->currentPlaylist();

    const bool isAutoPlaylist = m_playlistController->currentIsAuto();
    const bool readOnly       = (!m_search.isEmpty() || isAutoPlaylist);

    setReadOnly(readOnly, !isAutoPlaylist);

    switch(m_mode) {
        case(PlaylistWidget::Mode::Playlist):
            if(currentPlaylist) {
                m_model->reset(m_currentPreset, m_singleMode ? PlaylistColumnList{} : m_columns, currentPlaylist,
                               !m_search.isEmpty() ? m_filteredTracks : currentPlaylist->playlistTracks());
            }
            break;
        case(PlaylistWidget::Mode::DetachedPlaylist):
        case(PlaylistWidget::Mode::DetachedLibrary):
            m_model->reset(m_currentPreset, m_singleMode ? PlaylistColumnList{} : m_columns, nullptr, m_filteredTracks);
            break;
    }
}

void PlaylistWidgetPrivate::resetModelThrottled() const
{
    m_resetThrottler->throttle();
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
        const QModelIndex index = m_model->indexAtPlaylistIndex(selectedIndex, true);
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
    QMetaObject::invokeMethod(m_playlistView, [this]() { m_playlistView->selectAll(); }, Qt::QueuedConnection);
}

void PlaylistWidgetPrivate::selectionChanged() const
{
    if(m_mode == PlaylistWidget::Mode::DetachedPlaylist || !m_playlistController->currentPlaylist()) {
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
            tracks.push_back(index.data(PlaylistItem::Role::ItemData).value<PlaylistTrack>().track);
            const int trackIndex = index.data(PlaylistItem::Role::Index).toInt();
            trackIndexes.emplace(trackIndex);
            firstIndex = firstIndex >= 0 ? std::min(firstIndex, trackIndex) : trackIndex;
        }
    }

    if(m_mode == PlaylistWidget::Mode::DetachedLibrary) {
        firstIndex = 0;
    }

    m_selectionController->changeSelectedTracks(m_playlistContext, firstIndex, tracks);

    if(tracks.empty()) {
        m_removeTrackAction->setEnabled(false);
        m_addToQueueAction->setEnabled(false);
        m_queueNextAction->setEnabled(false);
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

    if(!m_playlistController->currentIsAuto()) {
        m_cutAction->setEnabled(true);
        m_pasteAction->setEnabled(true);
        m_removeTrackAction->setEnabled(true);
    }

    m_copyAction->setEnabled(true);
    m_addToQueueAction->setEnabled(true);
    m_queueNextAction->setEnabled(true);

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

    m_self->startPlayback();
}

void PlaylistWidgetPrivate::queueSelectedTracks(bool next, bool send) const
{
    if(!m_playlistController->currentPlaylist()) {
        return;
    }

    const auto selected = filterSelectedIndexes(m_playlistView);

    QueueTracks tracks;

    for(const QModelIndex& index : selected) {
        if(index.isValid() && index.data(PlaylistItem::Type).toInt() == PlaylistItem::Track) {
            auto track = index.data(PlaylistItem::ItemData).value<PlaylistTrack>();
            if(track.isValid()) {
                tracks.push_back(track);
            }
        }
    }

    if(send) {
        m_playerController->replaceTracks(tracks);
    }
    else if(next) {
        m_playerController->queueTracksNext(tracks);
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

    const auto selected = filterSelectedIndexes(m_playlistView);

    QueueTracks tracks;

    for(const QModelIndex& index : selected) {
        if(index.isValid() && index.data(PlaylistItem::Type).toInt() == PlaylistItem::Track) {
            auto track = index.data(PlaylistItem::ItemData).value<PlaylistTrack>();
            if(track.isValid()) {
                tracks.push_back(track);
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

    m_dropIndex = index;

    m_playlistView->setFocus(Qt::ActiveWindowFocusReason);

    if(m_playlistInteractor) {
        m_playlistInteractor->filesToTracks(urls, [this](const TrackList& tracks) {
            const auto playlistId     = m_playlistController->currentPlaylist()->id();
            const auto playlistTracks = PlaylistTrack::fromTracks(tracks, playlistId);
            auto* insertCmd
                = new InsertTracks(m_playerController, m_model, playlistId, {{m_dropIndex, playlistTracks}});
            m_playlistController->addToHistory(insertCmd);
            if(m_dropIndex >= 0) {
                m_dropIndex += static_cast<int>(tracks.size());
            }
        });
    }
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
                                 m_playlistController->currentPlaylist()->playlistTracks(), {});
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
            const auto oldTracks = m_playlistController->currentPlaylist()->playlistTracks();
            m_playlistController->playlistHandler()->removePlaylistTracks(m_playlistController->currentPlaylist()->id(),
                                                                          indexes);
            delCmd = new ResetTracks(m_playerController, m_model, m_playlistController->currentPlaylistId(), oldTracks,
                                     m_playlistController->currentPlaylist()->playlistTracks());
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

    auto* clearCmd = new ResetTracks(m_playerController, m_model, m_playlistController->currentPlaylistId(),
                                     m_playlistController->currentPlaylist()->playlistTracks(), {});
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

    const auto playlistId     = m_playlistController->currentPlaylist()->id();
    const auto playlistTracks = PlaylistTrack::fromTracks(tracks, playlistId);
    auto* insertCmd = new InsertTracks(m_playerController, m_model, playlistId, {{insertIndex, playlistTracks}});
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
    const auto playlistId     = m_playlistController->currentPlaylist()->id();
    const auto playlistTracks = PlaylistTrack::fromTracks(tracks, playlistId);
    auto* insertCmd           = new InsertTracks(m_playerController, m_model, playlistId, {{index, playlistTracks}});
    m_playlistController->addToHistory(insertCmd);
}

void PlaylistWidgetPrivate::handleTracksChanged(const std::vector<int>& indexes, bool allNew)
{
    if(!m_sortingColumn) {
        resetSort(true);
    }

    m_filteredTracks.clear();

    saveState(m_playlistController->currentPlaylist());
    m_playlistController->setSearch(m_playlistController->currentPlaylist(), {});
    emit m_self->changeSearch({});

    auto restoreSelection = [this](const int currentIndex, const std::vector<int>& selectedIndexes) {
        restoreState(m_playlistController->currentPlaylist());
        restoreSelectedPlaylistIndexes(selectedIndexes);

        if(currentIndex >= 0) {
            const QModelIndex index = m_model->indexAtPlaylistIndex(currentIndex, true);
            if(index.isValid()) {
                m_playlistView->selectionModel()->setCurrentIndex(index, QItemSelectionModel::NoUpdate);
            }
        }
    };

    const auto changedTrackCount = static_cast<int>(indexes.size());
    // It's faster to just reset if we're going to be updating more than half the playlist
    // or we're updating a large number of tracks
    if(indexes.empty() || changedTrackCount > 500
       || changedTrackCount > (m_playlistController->currentPlaylist()->trackCount() / 2)) {
        std::vector<int> selectedIndexes;

        if(!allNew) {
            selectedIndexes = selectedPlaylistIndexes();

            QObject::connect(
                m_model, &PlaylistModel::playlistTracksChanged, this,
                [restoreSelection, selectedIndexes]() { restoreSelection(-1, selectedIndexes); },
                Qt::SingleShotConnection);
        }

        resetModelThrottled();
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

    if(!selection.empty()) {
        const QModelIndex firstIndex = selection.indexes().front();
        m_playlistView->setCurrentIndex(firstIndex);
        m_playlistView->selectionModel()->select(selection,
                                                 QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        m_playlistView->scrollTo(firstIndex, QAbstractItemView::PositionAtCenter);
    }
}

void PlaylistWidgetPrivate::filterTracks(const PlaylistTrackList& tracks)
{
    m_filteredTracks = tracks;
    resetModelThrottled();
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

void PlaylistWidgetPrivate::setReadOnly(bool readOnly, bool allowSorting) const
{
    m_header->setSectionsClickable(!readOnly || allowSorting);

    if(m_mode == PlaylistWidget::Mode::Playlist) {
        m_playlistView->setDragEnabled(!readOnly);
        m_playlistView->setDragDropMode(!readOnly ? QAbstractItemView::DragDrop : QAbstractItemView::NoDragDrop);
        m_undoAction->setEnabled(!readOnly);
        m_redoAction->setEnabled(!readOnly);
        m_removeTrackAction->setEnabled(!readOnly);
        m_removeFromQueueAction->setEnabled(!readOnly);
        m_clearAction->setEnabled(!readOnly);
        m_cutAction->setEnabled(!readOnly);
        m_pasteAction->setEnabled(!readOnly);
    }
}

void PlaylistWidgetPrivate::updateSpans()
{
    auto isPixmap = [](const QString& field) {
        return field == QLatin1String(Constants::FrontCover) || field == QLatin1String(Constants::BackCover)
            || field == QLatin1String(Constants::ArtistPicture);
    };

    bool hasRating{false};
    for(int i{0}; const auto& column : m_columns) {
        if(isPixmap(column.field)) {
            m_playlistView->setSpan(i, true);
        }
        else {
            m_playlistView->setSpan(i, false);
        }

        if(column.field == QLatin1String{Constants::RatingEditor}) {
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
        m_self->startPlayback();
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

    QModelIndex modelIndex = m_model->indexAtPlaylistIndex(index, true);
    while(modelIndex.isValid() && m_header->isSectionHidden(modelIndex.column())) {
        modelIndex = modelIndex.siblingAtColumn(modelIndex.column() + 1);
    }

    if(modelIndex.data(PlaylistItem::ItemData).value<PlaylistTrack>().track.id()
       != m_playerController->currentTrackId()) {
        return;
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

    const auto oldTracks = m_playlistController->currentPlaylist()->playlistTracks();

    m_playlistController->playlistHandler()->removePlaylistTracks(m_playlistController->currentPlaylist()->id(),
                                                                  indexes);

    if(selectedCount > 500 || playlistTrackCount - selectedCount > 500) {
        // Faster to reset
        auto* resetCmd = new ResetTracks(m_playerController, m_model, m_playlistController->currentPlaylistId(),
                                         oldTracks, m_playlistController->currentPlaylist()->playlistTracks());
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
    const auto currentTracks = currentPlaylist->playlistTracks();

    auto handleSortedTracks = [this, currentPlaylist](const PlaylistTrackList& sortedTracks) {
        auto* sortCmd = new ResetTracks(m_playerController, m_model, currentPlaylist->id(),
                                        currentPlaylist->playlistTracks(), sortedTracks);
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
            auto tracks = m_sorter.calcSortTracks(script, currentTracks, indexesToSort, PlaylistTrack::extractor,
                                                  PlaylistTrack::extractorConst);
            return PlaylistTrack::updateIndexes(tracks);
        }).then(m_self, handleSortedTracks);
    }
    else {
        Utils::asyncExec([this, currentTracks, script]() {
            auto tracks = m_sorter.calcSortTracks(script, currentTracks, PlaylistTrack::extractor,
                                                  PlaylistTrack::extractorConst);
            return PlaylistTrack::updateIndexes(tracks);
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

    auto* currentPlaylist = m_playlistController->currentPlaylist();
    const auto currentTracks
        = m_mode != PlaylistWidget::Mode::Playlist ? m_filteredTracks : currentPlaylist->playlistTracks();
    const QString sortField = m_columns.at(column).field;

    Utils::asyncExec([this, sortField, currentTracks, order]() {
        auto tracks = m_sorter.calcSortTracks(sortField, currentTracks, PlaylistTrack::extractor,
                                              PlaylistTrack::extractorConst, order);
        return PlaylistTrack::updateIndexes(tracks);
    }).then(m_self, [this, currentPlaylist, currentTracks](const PlaylistTrackList& sortedTracks) {
        auto* sortCmd
            = new ResetTracks(m_playerController, m_model, currentPlaylist->id(), currentTracks, sortedTracks);
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
                               CoverProvider* coverProvider, Application* core, Mode mode, QWidget* parent)
    : FyWidget{parent}
    , p{std::make_unique<PlaylistWidgetPrivate>(this, actionManager, playlistInteractor, coverProvider, core, mode)}
{
    setObjectName(PlaylistWidget::name());
    setFeature(ExclusiveSearch);
}

PlaylistWidget::~PlaylistWidget()
{
    if(p->m_mode != Mode::Playlist || !p->m_playlistController->currentPlaylist()) {
        return;
    }

    p->m_playlistController->clearHistory();
    p->m_playlistController->savePlaylistState(p->m_playlistController->currentPlaylist(),
                                               p->getState(p->m_playlistController->currentPlaylist()));
}

PlaylistView* PlaylistWidget::view() const
{
    return p->m_playlistView;
}

PlaylistModel* PlaylistWidget::model() const
{
    return p->m_model;
}

int PlaylistWidget::trackCount() const
{
    switch(p->m_mode) {
        case(Mode::Playlist):
            if(auto* playlist = p->m_playlistController->currentPlaylist()) {
                return playlist->trackCount();
            }
            break;
        case(Mode::DetachedPlaylist):
        case(Mode::DetachedLibrary):
            return static_cast<int>(p->m_filteredTracks.size());
    }

    return 0;
}

void PlaylistWidget::startPlayback()
{
    if(p->m_selectionController->hasTracks()) {
        PlaylistAction::ActionOptions options;
        if(p->m_mode == Mode::DetachedLibrary) {
            options |= PlaylistAction::TempPlaylist;
        }
        p->m_selectionController->executeAction(TrackAction::Play, options);
    }
}

QString PlaylistWidget::name() const
{
    return tr("Playlist");
}

QString PlaylistWidget::layoutName() const
{
    return u"Playlist"_s;
}

void PlaylistWidget::saveLayoutData(QJsonObject& layout)
{
    layout["Preset"_L1]     = p->m_currentPreset.id;
    layout["SingleMode"_L1] = p->m_singleMode;

    if(!p->m_columns.empty()) {
        QStringList columns;

        for(int i{0}; const auto& column : p->m_columns) {
            const auto alignment = p->m_model->columnAlignment(i++);
            QString colStr       = QString::number(column.id);

            if(alignment != Qt::AlignLeft) {
                colStr += ":"_L1 + QString::number(alignment.toInt());
            }

            columns.push_back(colStr);
        }

        layout["Columns"_L1] = columns.join(u"|"_s);
    }

    p->resetSort();

    if(!p->m_singleMode || !p->m_headerState.isEmpty()) {
        QByteArray state
            = p->m_singleMode && !p->m_headerState.isEmpty() ? p->m_headerState : p->m_header->saveHeaderState();
        state                    = qCompress(state, 9);
        layout["HeaderState"_L1] = QString::fromUtf8(state.toBase64());
    }
}

void PlaylistWidget::loadLayoutData(const QJsonObject& layout)
{
    if(layout.contains("Preset"_L1)) {
        const int presetId = layout.value("Preset"_L1).toInt();
        if(const auto preset = p->m_presetRegistry->itemById(presetId)) {
            p->m_currentPreset = preset.value();
        }
    }
    if(layout.contains("SingleMode"_L1)) {
        p->m_singleMode = layout.value("SingleMode"_L1).toBool();
    }

    if(layout.contains("Columns"_L1)) {
        p->m_columns.clear();

        const QString columnData    = layout.value("Columns"_L1).toString();
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

    if(layout.contains("HeaderState"_L1)) {
        const auto headerState = layout.value("HeaderState"_L1).toString().toUtf8();

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

    if(!p->m_singleMode) {
        p->setSingleMode(false);
        p->m_model->reset({});
    }
    else if(p->m_mode == Mode::Playlist && p->m_playlistController->currentPlaylist()) {
        p->changePlaylist(nullptr, p->m_playlistController->currentPlaylist());
    }

    if(p->m_mode != Mode::Playlist) {
        QMetaObject::invokeMethod(this, [this]() { p->resetSort(true); }, Qt::QueuedConnection);
    }
}

void PlaylistWidget::searchEvent(const QString& search)
{
    if(p->m_mode != Mode::Playlist) {
        p->resetSort(true);
    }

    p->m_search = search;
    if(search.length() < 1) {
        p->m_search.clear();
        p->m_filteredTracks.clear();
    }

    auto handleFilteredTracks = [this](const PlaylistTrackList& filteredTracks) {
        p->m_filteredTracks = filteredTracks;
        p->resetModelThrottled();
    };

    auto filterAndHandleTracks = [this, handleFilteredTracks](const PlaylistTrackList& tracks) {
        Utils::asyncExec([search = p->m_search, tracks]() {
            ScriptParser parser;
            return parser.filter(search, tracks);
        }).then(this, handleFilteredTracks);
    };

    if(!p->m_search.isEmpty()) {
        if(p->m_mode == Mode::DetachedLibrary) {
            filterAndHandleTracks(PlaylistTrack::fromTracks(p->m_library->tracks(), {}));
        }
        else if(const auto* playlist = p->m_playlistController->currentPlaylist()) {
            filterAndHandleTracks(playlist->playlistTracks());
        }
    }
    else {
        p->resetModelThrottled();
    }
}

void PlaylistWidget::contextMenuEvent(QContextMenuEvent* event)
{
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    const auto selected     = filterSelectedIndexes(p->m_playlistView);
    const bool hasSelection = !selected.empty();

    const bool isAutoPlaylist = p->m_playlistController->currentIsAuto();

    if(hasSelection) {
        menu->addAction(p->m_playAction);

        if(p->m_mode == Mode::Playlist && p->m_playlistController->currentIsActive()) {
            const auto currentIndex = p->m_playlistView->currentIndex();
            if(currentIndex.isValid() && currentIndex.data(PlaylistItem::Type).toInt() == PlaylistItem::Track) {
                menu->addAction(p->m_stopAfter);
            }
        }

        menu->addSeparator();

        if(!isAutoPlaylist && p->m_mode == Mode::Playlist) {
            if(auto* removeCmd = p->m_actionManager->command(Constants::Actions::Remove)) {
                menu->addAction(removeCmd->action());
            }

            menu->addAction(p->m_cropAction);

            p->addSortMenu(menu, selected.size() == 1);
            menu->addSeparator();
        }
    }

    if(!isAutoPlaylist && p->m_mode == Mode::Playlist) {
        p->addClipboardMenu(menu, hasSelection);
        menu->addSeparator();
    }

    p->addPresetMenu(menu);
    menu->addSeparator();

    if(hasSelection) {
        if(p->m_mode == Mode::Playlist) {
            if(auto* addQueueCmd = p->m_actionManager->command(Constants::Actions::AddToQueue)) {
                menu->addAction(addQueueCmd->action());
            }
            if(auto* addQueueNextCmd = p->m_actionManager->command(Constants::Actions::QueueNext)) {
                menu->addAction(addQueueNextCmd->action());
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
        startPlayback();
        p->m_playlistView->clearSelection();
    }

    QWidget::keyPressEvent(event);
}
} // namespace Fooyin

#include "moc_playlistwidget.cpp"
