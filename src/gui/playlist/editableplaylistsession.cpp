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

#include "editableplaylistsession.h"

#include "internalguisettings.h"
#include "playlist/playlistinteractor.h"
#include "playlistcommands.h"
#include "playlistcontroller.h"
#include "playlistuicontroller.h"
#include "playlistview.h"

#include <core/player/playercontroller.h>
#include <core/playlist/playlisthandler.h>
#include <gui/guiconstants.h>
#include <gui/guisettings.h>
#include <utils/actions/actioncontainer.h>
#include <utils/actions/actionmanager.h>
#include <utils/actions/command.h>
#include <utils/async.h>
#include <utils/signalthrottler.h>
#include <utils/utils.h>

#include <QAction>
#include <QItemSelection>
#include <QItemSelectionModel>
#include <QKeySequence>

#include <algorithm>
#include <ranges>
#include <utility>

namespace Fooyin {
namespace {
PlaylistWidgetSessionHost& widgetSessionHost(PlaylistWidget* widget)
{
    return widget->sessionHost();
}

EditablePlaylistSessionHost& editableHost(PlaylistWidget* widget)
{
    return widget->editableSessionHost();
}

bool canEditPlaylistTracks(const Playlist* playlist)
{
    return playlist && !playlist->isAutoPlaylist();
}

bool canReorderPlaylist(const Playlist* playlist)
{
    return playlist && (!playlist->isAutoPlaylist() || !playlist->forceSorted());
}
} // namespace

std::unique_ptr<PlaylistWidgetSession> PlaylistWidgetSession::createEditable()
{
    return std::make_unique<EditablePlaylistSession>();
}

EditablePlaylistSession::EditablePlaylistSession()
    : m_showPlaying{false}
    , m_pendingFocus{false}
    , m_dropIndex{-1}
    , m_currentIndex{-1}
    , m_sortRequestToken{0}
    , m_undoAction{nullptr}
    , m_redoAction{nullptr}
    , m_cropAction{nullptr}
    , m_stopAfterAction{nullptr}
    , m_cutAction{nullptr}
    , m_copyAction{nullptr}
    , m_pasteAction{nullptr}
    , m_clearAction{nullptr}
    , m_removeTrackAction{nullptr}
    , m_removeDuplicatesAction{nullptr}
    , m_removeDeadTracksAction{nullptr}
    , m_addToQueueAction{nullptr}
    , m_queueNextAction{nullptr}
    , m_removeFromQueueAction{nullptr}
{ }

uint64_t EditablePlaylistSession::beginSortRequest()
{
    return ++m_sortRequestToken;
}

void EditablePlaylistSession::finishSortRequest(uint64_t token, bool sortingColumn)
{
    if(token != m_sortRequestToken) {
        return;
    }

    if(sortingColumn) {
        setSortingColumn(false);
    }
    setSorting(false);
}

PlaylistWidget::ModeCapabilities EditablePlaylistSession::capabilities() const
{
    return {.editablePlaylist = true, .playlistBackedSelection = true};
}

QString EditablePlaylistSession::emptyText() const
{
    return PlaylistWidget::tr("Playlist empty");
}

QString EditablePlaylistSession::loadingText() const
{
    return PlaylistWidget::tr("Loading playlist…");
}

int EditablePlaylistSession::renderedTrackCount(const PlaylistController* playlistController) const
{
    if(const auto* playlist = playlistController->currentPlaylist()) {
        return playlist->trackCount();
    }

    return 0;
}

Playlist* EditablePlaylistSession::modelPlaylist(Playlist* currentPlaylist) const
{
    return currentPlaylist;
}

PlaylistTrackList EditablePlaylistSession::modelTracks(Playlist* currentPlaylist) const
{
    if(!currentPlaylist) {
        return {};
    }

    return hasSearch() ? filteredTracks() : currentPlaylist->playlistTracks();
}

TrackSelection EditablePlaylistSession::selection(Playlist* currentPlaylist, const TrackList& tracks,
                                                  const std::set<int>& trackIndexes,
                                                  const PlaylistTrack& firstTrack) const
{
    TrackSelection trackSelection
        = makeTrackSelection(tracks, {trackIndexes.cbegin(), trackIndexes.cend()}, currentPlaylist);
    if(firstTrack.isValid()) {
        trackSelection.primaryPlaylistIndex = firstTrack.indexInPlaylist;
    }
    return trackSelection;
}

bool EditablePlaylistSession::canDequeue(const PlayerController* playerController, Playlist* currentPlaylist,
                                         const std::set<int>& trackIndexes,
                                         const std::set<Track>& /*selectedTracks*/) const
{
    if(!currentPlaylist) {
        return false;
    }

    const auto queuedTracks = playerController->playbackQueue().indexesForPlaylist(currentPlaylist->id());
    return std::ranges::any_of(queuedTracks,
                               [&trackIndexes](const auto& track) { return trackIndexes.contains(track.first); });
}

PlaylistTrackList EditablePlaylistSession::searchSourceTracks(const PlaylistController* playlistController,
                                                              const MusicLibrary* /*library*/) const
{
    if(const auto* playlist = playlistController->currentPlaylist()) {
        return playlist->playlistTracks();
    }

    return {};
}

bool EditablePlaylistSession::restoreCurrentPlaylistOnFinalise() const
{
    return true;
}

void EditablePlaylistSession::ensureActions(QWidget* parent)
{
    if(!m_undoAction) {
        m_undoAction = new QAction(PlaylistWidget::tr("&Undo"), parent);
    }
    if(!m_redoAction) {
        m_redoAction = new QAction(PlaylistWidget::tr("&Redo"), parent);
    }
    if(!m_cropAction) {
        m_cropAction = new QAction(PlaylistWidget::tr("&Crop"), parent);
    }
    if(!m_stopAfterAction) {
        m_stopAfterAction
            = new QAction(Utils::iconFromTheme(Constants::Icons::Stop), PlaylistWidget::tr("&Stop after this"), parent);
    }
    if(!m_cutAction) {
        m_cutAction = new QAction(PlaylistWidget::tr("Cu&t"), parent);
    }
    if(!m_copyAction) {
        m_copyAction = new QAction(PlaylistWidget::tr("&Copy"), parent);
    }
    if(!m_pasteAction) {
        m_pasteAction = new QAction(PlaylistWidget::tr("&Paste"), parent);
    }
    if(!m_clearAction) {
        m_clearAction = new QAction(PlaylistWidget::tr("C&lear"), parent);
    }
    if(!m_removeTrackAction) {
        m_removeTrackAction
            = new QAction(Utils::iconFromTheme(Constants::Icons::Remove), PlaylistWidget::tr("&Remove"), parent);
    }
    if(!m_removeDuplicatesAction) {
        m_removeDuplicatesAction = new QAction(PlaylistWidget::tr("Remove duplicates"), parent);
    }
    if(!m_removeDeadTracksAction) {
        m_removeDeadTracksAction = new QAction(PlaylistWidget::tr("Remove dead tracks"), parent);
    }
    if(!m_addToQueueAction) {
        m_addToQueueAction = new QAction(Utils::iconFromTheme(Constants::Icons::Add),
                                         PlaylistWidget::tr("Add to playback &queue"), parent);
    }
    if(!m_queueNextAction) {
        m_queueNextAction = new QAction(PlaylistWidget::tr("&Queue to play next"), parent);
    }
    if(!m_removeFromQueueAction) {
        m_removeFromQueueAction = new QAction(Utils::iconFromTheme(Constants::Icons::Remove),
                                              PlaylistWidget::tr("Remove from playback q&ueue"), parent);
    }
}

void EditablePlaylistSession::setupConnections(PlaylistWidgetSessionHost& sessionHost)
{
    auto* widget = sessionHost.sessionWidget();
    auto& host   = editableHost(widget);

    QObject::connect(host.presetRegistry(), &PresetRegistry::presetChanged, widget,
                     [widget](const PlaylistPreset& preset) { editableHost(widget).handlePresetChanged(preset); });

    QObject::connect(widget->playlistView(), &ExpandedTreeView::middleClicked, widget, &PlaylistWidget::middleClicked);
    QObject::connect(widget->playlistModel(), &PlaylistModel::playlistTracksChanged, widget,
                     [widget, this](int playingIndex) { handleTrackIndexesChanged(widget, playingIndex); });
    QObject::connect(widget->playlistModel(), &PlaylistModel::filesDropped, widget,
                     [widget, this](const QList<QUrl>& urls, int index) {
                         scanDroppedTracks(widgetSessionHost(widget), urls, index);
                     });
    QObject::connect(widget->playlistModel(), &PlaylistModel::tracksInserted, widget,
                     [widget, this](const TrackGroups& tracks) { tracksInserted(widgetSessionHost(widget), tracks); });
    QObject::connect(
        widget->playlistModel(), &PlaylistModel::tracksMoved, widget,
        [widget, this](const MoveOperation& operation) { tracksMoved(widgetSessionHost(widget), operation); });
    QObject::connect(widget->playlistModel(), &QAbstractItemModel::modelReset, widget,
                     [widget, this]() { resetTree(widgetSessionHost(widget)); });

    QObject::connect(
        widget->playlistController()->playlistHandler(), &PlaylistHandler::activePlaylistChanged, widget, [widget]() {
            widget->playlistModel()->playingTrackChanged(widget->playerController()->currentPlaylistTrack());
        });
    QObject::connect(widget->playlistController(), &PlaylistController::currentPlaylistTracksChanged, widget,
                     [widget, this](const std::vector<int>& indexes, bool allNew) {
                         handleTracksChanged(widgetSessionHost(widget), indexes, allNew);
                     });
    QObject::connect(widget->playlistController(), &PlaylistController::currentPlaylistQueueChanged,
                     widget->playlistModel(),
                     [widget](const std::vector<int>& indexes) { widget->playlistModel()->refreshTracks(indexes); });
    QObject::connect(widget->playlistController(), &PlaylistController::currentPlaylistChanged, widget,
                     [widget, this](Playlist* prevPlaylist, Playlist* playlist) {
                         changePlaylist(widgetSessionHost(widget), prevPlaylist, playlist);
                     });
    QObject::connect(widget->playlistController(), &PlaylistController::playlistsLoaded, widget, [widget, this]() {
        changePlaylist(widgetSessionHost(widget), nullptr, widget->playlistController()->currentPlaylist());
    });
    QObject::connect(widget->playlistController(), &PlaylistController::playingTrackChanged, widget,
                     [widget, this](const PlaylistTrack& track) { handlePlayingTrackChanged(widget, track); });
    QObject::connect(widget->playlistController(), &PlaylistController::playStateChanged, widget->playlistModel(),
                     &PlaylistModel::playStateChanged);
    QObject::connect(widget->playlistController(), &PlaylistController::currentPlaylistTracksAdded, widget,
                     [widget, this](const TrackList& tracks, int index) {
                         playlistTracksAdded(widgetSessionHost(widget), tracks, index);
                     });
    QObject::connect(widget->playlistController(), &PlaylistController::currentPlaylistTracksPatched, widget,
                     [widget, this](const PlaylistChangeset& changeSet) {
                         applyPlaylistChangeSet(widgetSessionHost(widget), changeSet);
                     });
    QObject::connect(widget->playlistController()->uiController(), &PlaylistUiController::selectTracks, widget,
                     [widget, this](const std::vector<int>& ids) { selectTrackIds(widget, ids); });
    QObject::connect(widget->playlistController()->uiController(), &PlaylistUiController::filterTracks, widget,
                     [widget, this](const PlaylistTrackList& tracks) { filterTracks(widget, tracks); });
    QObject::connect(widget->playlistController()->uiController(), &PlaylistUiController::requestPlaylistFocus,
                     widget->playlistModel(), [widget, this]() { requestPlaylistFocus(widgetSessionHost(widget)); });
    QObject::connect(widget->playlistController()->uiController(), &PlaylistUiController::showCurrentTrack, widget,
                     [widget, this]() {
                         if(widget->playlistController()->currentIsActive()) {
                             widget->followCurrentTrack();
                         }
                         else {
                             deferFollowCurrentTrack(widgetSessionHost(widget));
                         }
                     });
    host.settingsManager()->subscribe<Settings::Gui::Internal::PlaylistMiddleClick>(
        widget, [widget](int action) { editableHost(widget).setMiddleClickAction(static_cast<TrackAction>(action)); });
    host.settingsManager()->subscribe<Settings::Gui::Internal::PlaylistHeader>(
        widget, [widget](bool show) { editableHost(widget).setHeaderVisible(show); });
    host.settingsManager()->subscribe<Settings::Gui::Internal::PlaylistScrollBar>(
        widget, [widget](bool show) { editableHost(widget).setScrollbarVisible(show); });
    host.settingsManager()->subscribe<Settings::Gui::Internal::PlaylistAltColours>(
        host.playlistView(), &PlaylistView::setAlternatingRowColors);
}

void EditablePlaylistSession::setupActions(PlaylistWidgetSessionHost& sessionHost, ActionContainer* editMenu,
                                           const QStringList& editCategory)
{
    auto* widget = sessionHost.sessionWidget();
    auto& host   = editableHost(widget);
    ensureActions(widget);

    m_cropAction->setStatusTip(
        PlaylistWidget::tr("Remove all tracks from the playlist except for the selected tracks"));
    QObject::connect(m_cropAction, &QAction::triggered, widget,
                     [widget, this]() { cropSelection(widgetSessionHost(widget)); });

    m_stopAfterAction->setStatusTip(PlaylistWidget::tr("Stop playback at the end of the selected track"));
    QObject::connect(m_stopAfterAction, &QAction::triggered, widget, [widget, this]() { stopAfterTrack(widget); });

    host.actionManager()->addContextObject(host.playlistContext());

    m_undoAction->setStatusTip(PlaylistWidget::tr("Undo the previous playlist change"));
    auto* undoCmd = host.actionManager()->registerAction(m_undoAction, Constants::Actions::Undo,
                                                         host.playlistContext()->context());
    undoCmd->setCategories(editCategory);
    undoCmd->setDefaultShortcut(QKeySequence::Undo);
    editMenu->addAction(undoCmd);
    QObject::connect(m_undoAction, &QAction::triggered, widget,
                     [widget]() { editableHost(widget).playlistController()->undoPlaylistChanges(); });
    QObject::connect(host.playlistController(), &PlaylistController::playlistHistoryChanged, widget,
                     [this, widget]() { refreshActionState(widget); });

    m_redoAction->setStatusTip(PlaylistWidget::tr("Redo the previous playlist change"));
    auto* redoCmd = host.actionManager()->registerAction(m_redoAction, Constants::Actions::Redo,
                                                         host.playlistContext()->context());
    redoCmd->setCategories(editCategory);
    redoCmd->setDefaultShortcut(QKeySequence::Redo);
    editMenu->addAction(redoCmd);
    QObject::connect(m_redoAction, &QAction::triggered, widget,
                     [widget]() { editableHost(widget).playlistController()->redoPlaylistChanges(); });
    QObject::connect(host.playlistController(), &PlaylistController::playlistHistoryChanged, widget,
                     [this, widget]() { refreshActionState(widget); });

    editMenu->addSeparator();

    m_cutAction->setStatusTip(PlaylistWidget::tr("Cut the selected tracks"));
    auto* cutCommand
        = host.actionManager()->registerAction(m_cutAction, Constants::Actions::Cut, host.playlistContext()->context());
    cutCommand->setCategories(editCategory);
    cutCommand->setDefaultShortcut(QKeySequence::Cut);
    editMenu->addAction(cutCommand);
    QObject::connect(m_cutAction, &QAction::triggered, widget,
                     [widget, this]() { cutTracks(widgetSessionHost(widget)); });

    m_copyAction->setStatusTip(PlaylistWidget::tr("Copy the selected tracks"));
    auto* copyCommand = host.actionManager()->registerAction(m_copyAction, Constants::Actions::Copy,
                                                             host.playlistContext()->context());
    copyCommand->setCategories(editCategory);
    copyCommand->setDefaultShortcut(QKeySequence::Copy);
    editMenu->addAction(copyCommand);
    QObject::connect(m_copyAction, &QAction::triggered, widget,
                     [widget, this]() { copyTracks(widgetSessionHost(widget)); });
    m_copyAction->setEnabled(host.playlistView()->selectionModel()->hasSelection());

    m_pasteAction->setStatusTip(PlaylistWidget::tr("Paste the selected tracks"));
    auto* pasteCommand = host.actionManager()->registerAction(m_pasteAction, Constants::Actions::Paste,
                                                              host.playlistContext()->context());
    pasteCommand->setCategories(editCategory);
    pasteCommand->setDefaultShortcut(QKeySequence::Paste);
    editMenu->addAction(m_pasteAction);
    QObject::connect(host.playlistController(), &PlaylistController::clipboardChanged, widget,
                     [this, widget]() { refreshActionState(widget); });
    QObject::connect(m_pasteAction, &QAction::triggered, widget,
                     [widget, this]() { pasteTracks(widgetSessionHost(widget)); });

    editMenu->addSeparator();

    m_clearAction->setStatusTip(PlaylistWidget::tr("Remove all tracks from the current playlist"));
    auto* clearCmd = host.actionManager()->registerAction(m_clearAction, Constants::Actions::Clear,
                                                          host.playlistContext()->context());
    clearCmd->setCategories(editCategory);
    clearCmd->setAttribute(ProxyAction::UpdateText);
    editMenu->addAction(clearCmd);
    QObject::connect(m_clearAction, &QAction::triggered, widget,
                     [widget, this]() { clearTracks(widgetSessionHost(widget)); });

    m_removeTrackAction->setStatusTip(PlaylistWidget::tr("Remove the selected tracks from the current playlist"));
    m_removeTrackAction->setEnabled(false);
    auto* removeCmd = host.actionManager()->registerAction(m_removeTrackAction, Constants::Actions::Remove,
                                                           host.playlistContext()->context());
    removeCmd->setCategories(editCategory);
    removeCmd->setAttribute(ProxyAction::UpdateText);
    removeCmd->setDefaultShortcut(QKeySequence::Delete);
    QObject::connect(m_removeTrackAction, &QAction::triggered, widget, [widget, this]() {
        tracksRemoved(widgetSessionHost(widget), selectedPlaylistIndexes(widgetSessionHost(widget)));
    });

    m_addToQueueAction->setStatusTip(PlaylistWidget::tr("Add the selected tracks to the playback queue"));
    m_addToQueueAction->setEnabled(false);
    host.actionManager()->registerAction(m_addToQueueAction, Constants::Actions::AddToQueue,
                                         host.playlistContext()->context());
    QObject::connect(m_addToQueueAction, &QAction::triggered, widget,
                     [widget, this]() { queueSelectedTracks(widgetSessionHost(widget), false, false); });

    m_queueNextAction->setStatusTip(PlaylistWidget::tr("Add the selected tracks to the front of the playback queue"));
    m_queueNextAction->setEnabled(false);
    host.actionManager()->registerAction(m_queueNextAction, Constants::Actions::QueueNext,
                                         host.playlistContext()->context());
    QObject::connect(m_queueNextAction, &QAction::triggered, widget,
                     [widget, this]() { queueSelectedTracks(widgetSessionHost(widget), true, false); });

    m_removeFromQueueAction->setStatusTip(PlaylistWidget::tr("Remove the selected tracks from the playback queue"));
    m_removeFromQueueAction->setVisible(false);
    host.actionManager()->registerAction(m_removeFromQueueAction, Constants::Actions::RemoveFromQueue,
                                         host.playlistContext()->context());
    QObject::connect(m_removeFromQueueAction, &QAction::triggered, widget,
                     [widget, this]() { dequeueSelectedTracks(widgetSessionHost(widget)); });

    editMenu->addSeparator();

    m_removeDuplicatesAction->setStatusTip(PlaylistWidget::tr("Remove duplicate tracks from the playlist"));
    editMenu->addAction(m_removeDuplicatesAction);
    QObject::connect(m_removeDuplicatesAction, &QAction::triggered, widget,
                     [widget, this]() { removeDuplicates(widgetSessionHost(widget)); });

    m_removeDeadTracksAction->setStatusTip(PlaylistWidget::tr("Remove dead (non-existant) tracks from the playlist"));
    editMenu->addAction(m_removeDeadTracksAction);
    QObject::connect(m_removeDeadTracksAction, &QAction::triggered, widget,
                     [widget, this]() { removeDeadTracks(widgetSessionHost(widget)); });

    editMenu->addSeparator();

    refreshActionState(widget);
    updateSelectionState(sessionHost, {}, {}, {});
}

void EditablePlaylistSession::requestPlaylistFocus(PlaylistWidgetSessionHost& sessionHost)
{
    auto& host = editableHost(sessionHost.sessionWidget());
    if(host.playlistView()->playlistLoaded() && !host.resetThrottler()->isActive()) {
        host.playlistView()->setFocus(Qt::ActiveWindowFocusReason);
        host.playlistView()->setCurrentIndex(host.playlistModel()->indexAtPlaylistIndex(0, false));
    }
    else {
        m_pendingFocus = true;
    }
}

void EditablePlaylistSession::changePlaylist(PlaylistWidgetSessionHost& sessionHost, Playlist* prevPlaylist,
                                             Playlist* playlist)
{
    auto* widget = sessionHost.sessionWidget();
    auto& host   = editableHost(widget);

    host.clearDelayedStateLoad();
    if(prevPlaylist && host.playlistView()->playlistLoaded()) {
        savePlaylistViewState(host, prevPlaylist);
    }

    host.playlistController()->setSearch(prevPlaylist, search());
    host.resetSort(true);
    setFilteredTracks({});

    const QString currentSearch = host.playlistController()->currentSearch(playlist);
    if(currentSearch.isEmpty()) {
        clearSearch();
    }
    else {
        setSearch(currentSearch);
    }
    emit widget->changeSearch(currentSearch);

    if(currentSearch.isEmpty()) {
        host.resetModelThrottled();
    }
}

void EditablePlaylistSession::destroy(PlaylistWidgetSessionHost& sessionHost)
{
    auto& host = editableHost(sessionHost.sessionWidget());
    if(host.playlistController()->currentPlaylist()) {
        savePlaylistViewState(host, host.playlistController()->currentPlaylist(), false);
    }
    host.playlistController()->clearHistory();
}

void EditablePlaylistSession::refreshActionState(PlaylistWidget* widget)
{
    auto& host                  = editableHost(widget);
    auto* currentPlaylist       = host.playlistController()->currentPlaylist();
    const bool canEditTracks    = canEditPlaylistTracks(currentPlaylist);
    const bool canReorderTracks = canReorderPlaylist(currentPlaylist);
    const bool hasSelection
        = host.playlistView()->selectionModel() && host.playlistView()->selectionModel()->hasSelection();

    m_undoAction->setEnabled(canReorderTracks && host.playlistController()->canUndo());
    m_redoAction->setEnabled(canReorderTracks && host.playlistController()->canRedo());
    m_cropAction->setEnabled(canEditTracks && hasSelection);
    m_cutAction->setEnabled(canEditTracks && hasSelection);
    m_copyAction->setEnabled(hasSelection);
    m_pasteAction->setEnabled(canEditTracks && !host.playlistController()->clipboardEmpty());
    m_clearAction->setEnabled(canEditTracks && host.playlistModel()->rowCount({}) > 0);
    m_removeTrackAction->setEnabled(canEditTracks && hasSelection);
    m_removeDuplicatesAction->setEnabled(canEditTracks);
    m_removeDeadTracksAction->setEnabled(canEditTracks);
}

void EditablePlaylistSession::applyPlaylistChangeSet(PlaylistWidgetSessionHost& sessionHost,
                                                     const PlaylistChangeset& changeSet)
{
    auto& host            = editableHost(sessionHost.sessionWidget());
    auto* currentPlaylist = host.playlistController()->currentPlaylist();
    if(!currentPlaylist) {
        return;
    }

    if(changeSet.requiresReset || hasSearch()) {
        handleTracksChanged(sessionHost, {}, false);
        return;
    }

    QModelIndexList trackIndexesToRemove;
    for(const int index : changeSet.removedIndexes) {
        const QModelIndex trackIndex = host.playlistModel()->indexAtPlaylistIndex(index, false);
        if(trackIndex.isValid()) {
            trackIndexesToRemove.push_back(trackIndex);
        }
    }
    if(!trackIndexesToRemove.empty()) {
        host.playlistModel()->removeTracks(trackIndexesToRemove);
    }

    const auto applyMovesAndUpdates
        = [model = host.playlistModel(), moves = changeSet.moves, updatedIndexes = changeSet.updatedIndexes,
           currentPlaylist, this, widget                                         = sessionHost.sessionWidget()] {
              for(const auto& move : moves) {
                  if(!move.isValid()) {
                      continue;
                  }

                  MoveOperation operation;
                  operation.emplace_back(move.targetIndex, TrackIndexRangeList{{move.sourceIndex, move.sourceIndex}});
                  model->moveTracks(operation);
              }

              if(!updatedIndexes.empty()) {
                  model->updateTracks(updatedIndexes);
              }

              model->updateHeader(currentPlaylist);
              refreshActionState(widget);
          };

    TrackGroups insertions;
    for(const auto& insertion : changeSet.insertions) {
        if(insertion.isValid()) {
            insertions[insertion.index] = PlaylistTrack::fromTracks(insertion.tracks, currentPlaylist->id());
        }
    }

    if(!insertions.empty()) {
        QObject::connect(host.playlistModel(), &PlaylistModel::playlistTracksChanged, host.playlistModel(),
                         applyMovesAndUpdates, Qt::SingleShotConnection);
        host.playlistModel()->insertTracks(insertions);
    }
    else {
        applyMovesAndUpdates();
    }
}

void EditablePlaylistSession::handleAboutToBeReset(PlaylistWidgetSessionHost& sessionHost)
{
    auto& host = editableHost(sessionHost.sessionWidget());
    if(host.playlistModel()->isDirty()) {
        const QModelIndex currentIndex = host.playlistView()->currentIndex();
        if(currentIndex.isValid()) {
            m_currentIndex = currentIndex.data(PlaylistItem::Index).toInt();
        }
    }
    else {
        host.playlistView()->clearSelection();
    }
}

void EditablePlaylistSession::resetTree(PlaylistWidgetSessionHost& sessionHost)
{
    auto& host = editableHost(sessionHost.sessionWidget());
    if(host.playlistModel()->isDirty() && m_currentIndex >= 0) {
        const QModelIndex currentIndex = host.playlistModel()->indexAtPlaylistIndex(m_currentIndex);
        if(currentIndex.isValid()) {
            host.playlistView()->selectionModel()->setCurrentIndex(currentIndex, QItemSelectionModel::NoUpdate);
        }
        m_currentIndex = -1;
        return;
    }

    host.resetSort(false);
    restorePlaylistViewState(host, host.playlistController()->currentPlaylist());
    refreshActionState(sessionHost.sessionWidget());

    if(m_pendingFocus) {
        m_pendingFocus = false;
        requestPlaylistFocus(sessionHost);
    }
}

void EditablePlaylistSession::deferFollowCurrentTrack(PlaylistWidgetSessionHost& /*sessionHost*/)
{
    m_showPlaying = true;
}

void EditablePlaylistSession::handleRestoredState(PlaylistWidgetSessionHost& sessionHost)
{
    auto& host = editableHost(sessionHost.sessionWidget());
    if(m_showPlaying) {
        host.followCurrentTrack();
        m_showPlaying = false;
    }
}

void EditablePlaylistSession::handleTracksChanged(PlaylistWidgetSessionHost& sessionHost,
                                                  const std::vector<int>& indexes, bool allNew)
{
    auto* widget = sessionHost.sessionWidget();
    auto& host   = editableHost(widget);
    if(!host.playlistController()->currentPlaylist()) {
        return;
    }

    if(!isSortingColumn()) {
        host.resetSort(true);
    }

    setFilteredTracks({});
    savePlaylistViewState(host, host.playlistController()->currentPlaylist());
    host.playlistController()->setSearch(host.playlistController()->currentPlaylist(), {});
    clearSearch();
    emit widget->changeSearch({});

    auto restoreSelection = [&host](int currentIndex, const std::vector<int>& selectedIndexes) {
        restorePlaylistViewState(host, host.playlistController()->currentPlaylist());
        restoreSelectedPlaylistIndexes(host, selectedIndexes);
        if(currentIndex >= 0) {
            const QModelIndex index = host.playlistModel()->indexAtPlaylistIndex(currentIndex, true);
            if(index.isValid()) {
                host.playlistView()->selectionModel()->setCurrentIndex(index, QItemSelectionModel::NoUpdate);
            }
        }
    };

    const int changedTrackCount = static_cast<int>(indexes.size());
    if(indexes.empty() || changedTrackCount > 500
       || changedTrackCount > (host.playlistController()->currentPlaylist()->trackCount() / 2)) {
        std::vector<int> selectedIndexes;
        if(!allNew) {
            selectedIndexes = selectedPlaylistIndexes(host);
            QObject::connect(
                host.playlistModel(), &PlaylistModel::playlistTracksChanged, widget,
                [restoreSelection, selectedIndexes]() { restoreSelection(-1, selectedIndexes); },
                Qt::SingleShotConnection);
        }

        host.resetModelThrottled();
        return;
    }

    int currentIndex{-1};
    if(host.playlistView()->currentIndex().isValid()) {
        currentIndex = host.playlistView()->currentIndex().data(PlaylistItem::Index).toInt();
    }
    const std::vector<int> selectedIndexes = selectedPlaylistIndexes(host);
    QObject::connect(
        host.playlistModel(), &PlaylistModel::playlistTracksChanged, widget,
        [restoreSelection, currentIndex, selectedIndexes]() { restoreSelection(currentIndex, selectedIndexes); },
        Qt::SingleShotConnection);

    host.playlistModel()->updateTracks(indexes);
}

void EditablePlaylistSession::applyReadOnlyState(PlaylistWidgetSessionHost& sessionHost, bool readOnly)
{
    auto& host            = editableHost(sessionHost.sessionWidget());
    auto* currentPlaylist = host.playlistController()->currentPlaylist();
    const bool canReorder = canReorderPlaylist(currentPlaylist) && !readOnly;

    if(canEditPlaylistTracks(currentPlaylist)) {
        host.playlistView()->setDragDropMode(QAbstractItemView::DragDrop);
    }
    else if(canReorder) {
        host.playlistView()->setDragDropMode(QAbstractItemView::InternalMove);
    }
    else {
        host.playlistView()->setDragDropMode(QAbstractItemView::NoDragDrop);
    }

    host.playlistView()->viewport()->setAcceptDrops(canReorder);
    host.playlistView()->setDragEnabled(canReorder);
    host.playlistView()->setDefaultDropAction(canReorder ? Qt::MoveAction : Qt::IgnoreAction);

    refreshActionState(sessionHost.sessionWidget());
}

void EditablePlaylistSession::updateContextMenuState(PlaylistWidgetSessionHost& sessionHost,
                                                     const QModelIndexList& selected,
                                                     PlaylistWidget::ContextMenuState& state)
{
    const auto& host   = editableHost(sessionHost.sessionWidget());
    auto* playlist     = host.playlistController()->currentPlaylist();
    const bool canEdit = canEditPlaylistTracks(playlist);
    const bool canSort = canReorderPlaylist(playlist);

    state.showStopAfter = host.playlistController()->currentIsActive() && host.playlistView()->currentIndex().isValid()
                       && host.playlistView()->currentIndex().data(PlaylistItem::Type).toInt() == PlaylistItem::Track;
    state.showEditablePlaylistActions = canEdit;
    state.showSortMenu                = canSort;
    state.showClipboard               = canEdit;
    state.usePlaylistQueueCommands    = true;
    state.disableSortMenu             = selected.size() == 1;
}

void EditablePlaylistSession::updateSelectionState(PlaylistWidgetSessionHost& sessionHost,
                                                   const std::set<int>& trackIndexes,
                                                   const std::set<Track>& selectedTracks,
                                                   const PlaylistTrack& firstTrack)
{
    PlaylistWidgetSession::updateSelectionState(sessionHost, trackIndexes, selectedTracks, firstTrack);

    auto& host = editableHost(sessionHost.sessionWidget());
    if(host.settingsManager()->value<Settings::Gui::PlaybackFollowsCursor>()
       && host.playlistController()->currentPlaylist()
       && host.playlistController()->currentPlaylist()->currentTrackIndex() != firstTrack.indexInPlaylist) {
        host.playerController()->scheduleNextTrack(firstTrack);
    }

    const bool hasSelection = !trackIndexes.empty();
    m_addToQueueAction->setEnabled(hasSelection);
    m_queueNextAction->setEnabled(hasSelection);
    refreshActionState(sessionHost.sessionWidget());
}

void EditablePlaylistSession::queueSelectedTracks(PlaylistWidgetSessionHost& sessionHost, bool next, bool send)
{
    auto& host          = editableHost(sessionHost.sessionWidget());
    const auto selected = filterSelectedIndexes(host.playlistView());
    if(selected.empty()) {
        return;
    }

    QueueTracks tracks;
    tracks.reserve(selected.size());
    for(const QModelIndex& index : selected) {
        const auto track = index.data(PlaylistItem::Role::ItemData).value<PlaylistTrack>();
        if(track.isValid()) {
            tracks.push_back(track);
        }
    }

    if(send) {
        host.playerController()->replaceTracks(tracks);
    }
    else if(next) {
        host.playerController()->queueTracksNext(tracks);
    }
    else {
        host.playerController()->queueTracks(tracks);
    }

    m_removeFromQueueAction->setVisible(true);
}

void EditablePlaylistSession::dequeueSelectedTracks(PlaylistWidgetSessionHost& sessionHost)
{
    auto& host          = editableHost(sessionHost.sessionWidget());
    const auto selected = filterSelectedIndexes(host.playlistView());
    if(selected.empty() || !host.playlistController()->currentPlaylist()) {
        return;
    }

    QueueTracks tracks;
    tracks.reserve(selected.size());
    for(const QModelIndex& index : selected) {
        const auto track = index.data(PlaylistItem::Role::ItemData).value<PlaylistTrack>();
        if(track.isValid()) {
            tracks.push_back(track);
        }
    }

    host.playerController()->dequeueTracks(tracks);
    m_removeFromQueueAction->setVisible(false);
}

void EditablePlaylistSession::removeDuplicates(PlaylistWidgetSessionHost& sessionHost)
{
    auto& host = editableHost(sessionHost.sessionWidget());
    if(auto* playlist = host.playlistController()->currentPlaylist(); canEditPlaylistTracks(playlist)) {
        tracksRemoved(sessionHost, host.playlistController()->playlistHandler()->duplicateTrackIndexes(playlist->id()));
    }
}

void EditablePlaylistSession::removeDeadTracks(PlaylistWidgetSessionHost& sessionHost)
{
    auto& host = editableHost(sessionHost.sessionWidget());
    if(auto* playlist = host.playlistController()->currentPlaylist(); canEditPlaylistTracks(playlist)) {
        tracksRemoved(sessionHost, host.playlistController()->playlistHandler()->deadTrackIndexes(playlist->id()));
    }
}

void EditablePlaylistSession::scanDroppedTracks(PlaylistWidgetSessionHost& sessionHost, const QList<QUrl>& urls,
                                                int index)
{
    auto& host = editableHost(sessionHost.sessionWidget());
    if(!host.playlistController()->currentPlaylist()) {
        return;
    }

    m_dropIndex = index;
    host.playlistView()->setFocus(Qt::ActiveWindowFocusReason);

    host.playlistInteractor()->filesToTracks(
        urls, [this, widget = sessionHost.sessionWidget()](const TrackList& tracks) {
            auto& editableHostRef = editableHost(widget);
            if(!editableHostRef.playlistController()->currentPlaylist()) {
                return;
            }

            const UId playlistId      = editableHostRef.playlistController()->currentPlaylist()->id();
            const auto playlistTracks = PlaylistTrack::fromTracks(tracks, playlistId);
            auto* insertCommand = new InsertTracks(editableHostRef.playerController(), editableHostRef.playlistModel(),
                                                   playlistId, {{m_dropIndex, playlistTracks}});
            editableHostRef.playlistController()->addToHistory(insertCommand);

            if(m_dropIndex >= 0) {
                m_dropIndex += static_cast<int>(tracks.size());
            }
        });
}

void EditablePlaylistSession::tracksInserted(PlaylistWidgetSessionHost& sessionHost, const TrackGroups& tracks)
{
    auto& host    = editableHost(sessionHost.sessionWidget());
    auto* command = new InsertTracks(host.playerController(), host.playlistModel(),
                                     host.playlistController()->currentPlaylistId(), tracks);
    host.playlistController()->addToHistory(command);
    refreshActionState(sessionHost.sessionWidget());
    if(host.playlistController()->currentPlaylist()) {
        host.playlistModel()->updateHeader(host.playlistController()->currentPlaylist());
    }
    host.playlistView()->setFocus(Qt::ActiveWindowFocusReason);
}

void EditablePlaylistSession::tracksRemoved(PlaylistWidgetSessionHost& sessionHost, const std::vector<int>& indexes)
{
    auto& host = editableHost(sessionHost.sessionWidget());
    if(!host.playlistController()->currentPlaylist() || indexes.empty()) {
        return;
    }

    QModelIndexList trackIndexes;
    for(const int index : indexes) {
        const QModelIndex trackIndex = host.playlistModel()->indexAtPlaylistIndex(index, false);
        if(trackIndex.isValid()) {
            trackIndexes.push_back(trackIndex);
        }
    }

    const auto oldTracks = host.playlistController()->currentPlaylist()->playlistTracks();
    host.playlistController()->playlistHandler()->removePlaylistTracks(
        host.playlistController()->currentPlaylist()->id(), indexes);

    if(indexes.size() > 500) {
        auto* resetCmd = new ResetTracks(host.playerController(), host.playlistModel(),
                                         host.playlistController()->currentPlaylistId(), oldTracks,
                                         host.playlistController()->currentPlaylist()->playlistTracks());
        host.playlistController()->addToHistory(resetCmd);
    }
    else {
        auto* delCmd = new RemoveTracks(host.playerController(), host.playlistModel(),
                                        host.playlistController()->currentPlaylist()->id(),
                                        PlaylistModel::saveTrackGroups(trackIndexes));
        host.playlistController()->addToHistory(delCmd);
    }

    refreshActionState(sessionHost.sessionWidget());
    host.playlistModel()->updateHeader(host.playlistController()->currentPlaylist());
}

void EditablePlaylistSession::tracksMoved(PlaylistWidgetSessionHost& sessionHost, const MoveOperation& operation)
{
    auto& host    = editableHost(sessionHost.sessionWidget());
    auto* moveCmd = new MoveTracks(host.playerController(), host.playlistModel(),
                                   host.playlistController()->currentPlaylistId(), operation);
    host.playlistController()->addToHistory(moveCmd);
    host.playlistModel()->updateHeader(host.playlistController()->currentPlaylist());
    host.playlistView()->setFocus(Qt::ActiveWindowFocusReason);
}

void EditablePlaylistSession::clearTracks(PlaylistWidgetSessionHost& sessionHost)
{
    auto& host = editableHost(sessionHost.sessionWidget());
    if(!host.playlistController()->currentPlaylist()) {
        return;
    }

    auto* resetCmd
        = new ResetTracks(host.playerController(), host.playlistModel(), host.playlistController()->currentPlaylistId(),
                          host.playlistController()->currentPlaylist()->playlistTracks(), {});
    host.playlistController()->addToHistory(resetCmd);
}

void EditablePlaylistSession::cutTracks(PlaylistWidgetSessionHost& sessionHost)
{
    copyTracks(sessionHost);
    tracksRemoved(sessionHost, selectedPlaylistIndexes(editableHost(sessionHost.sessionWidget())));
}

void EditablePlaylistSession::copyTracks(PlaylistWidgetSessionHost& sessionHost)
{
    auto& host             = editableHost(sessionHost.sessionWidget());
    const auto selected    = filterSelectedIndexes(host.playlistView());
    const TrackList tracks = getTracks(selected);
    if(!tracks.empty()) {
        host.playlistController()->setClipboard(tracks);
    }
}

void EditablePlaylistSession::pasteTracks(PlaylistWidgetSessionHost& sessionHost)
{
    auto& host = editableHost(sessionHost.sessionWidget());
    if(!host.playlistController()->currentPlaylist() || host.playlistController()->clipboardEmpty()) {
        return;
    }

    const auto selected    = filterSelectedIndexes(host.playlistView());
    const TrackList tracks = host.playlistController()->clipboard();

    int insertIndex{-1};
    if(!selected.empty()) {
        insertIndex = selected.front().data(PlaylistItem::Index).toInt();
    }

    const UId playlistId      = host.playlistController()->currentPlaylist()->id();
    const auto playlistTracks = PlaylistTrack::fromTracks(tracks, playlistId);
    auto* insertCommand
        = new InsertTracks(host.playerController(), host.playlistModel(), playlistId, {{insertIndex, playlistTracks}});
    host.playlistController()->addToHistory(insertCommand);

    QObject::connect(
        host.playlistModel(), &PlaylistModel::playlistTracksChanged, host.playlistView(),
        [hostPtr = &host, insertIndex]() {
            const QModelIndex index = hostPtr->playlistModel()->indexAtPlaylistIndex(insertIndex, true);
            if(index.isValid()) {
                hostPtr->playlistView()->scrollTo(index, QAbstractItemView::EnsureVisible);
                hostPtr->playlistView()->setCurrentIndex(index);
            }
        },
        static_cast<Qt::ConnectionType>(Qt::SingleShotConnection | Qt::QueuedConnection));
}

void EditablePlaylistSession::playlistTracksAdded(PlaylistWidgetSessionHost& sessionHost, const TrackList& tracks,
                                                  int index)
{
    auto& host = editableHost(sessionHost.sessionWidget());
    if(!host.playlistController()->currentPlaylist()) {
        return;
    }

    const UId playlistId      = host.playlistController()->currentPlaylist()->id();
    const auto playlistTracks = PlaylistTrack::fromTracks(tracks, playlistId);
    auto* insertCommand
        = new InsertTracks(host.playerController(), host.playlistModel(), playlistId, {{index, playlistTracks}});
    host.playlistController()->addToHistory(insertCommand);
}

void EditablePlaylistSession::cropSelection(PlaylistWidgetSessionHost& sessionHost)
{
    auto& host = editableHost(sessionHost.sessionWidget());
    if(!host.playlistController()->currentPlaylist()) {
        return;
    }

    const auto selected = filterSelectedIndexes(host.playlistView());

    const int selectedCount      = static_cast<int>(selected.size());
    const int playlistTrackCount = host.playlistController()->currentPlaylist()->trackCount();

    if(selectedCount >= playlistTrackCount) {
        return;
    }

    QModelIndexList allTrackIndexes;
    getAllTrackIndexes(host.playlistModel(), {}, allTrackIndexes);

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

    const auto oldTracks = host.playlistController()->currentPlaylist()->playlistTracks();

    host.playlistController()->playlistHandler()->removePlaylistTracks(
        host.playlistController()->currentPlaylist()->id(), indexes);

    if(selectedCount > 500 || playlistTrackCount - selectedCount > 500) {
        auto* resetCmd = new ResetTracks(host.playerController(), host.playlistModel(),
                                         host.playlistController()->currentPlaylistId(), oldTracks,
                                         host.playlistController()->currentPlaylist()->playlistTracks());
        host.playlistController()->addToHistory(resetCmd);
    }
    else {
        auto* delCmd = new RemoveTracks(host.playerController(), host.playlistModel(),
                                        host.playlistController()->currentPlaylist()->id(),
                                        PlaylistModel::saveTrackGroups(tracksToRemove));
        host.playlistController()->addToHistory(delCmd);
    }

    host.playlistModel()->updateHeader(host.playlistController()->currentPlaylist());
}

void EditablePlaylistSession::sortTracks(PlaylistWidgetSessionHost& sessionHost, const QString& script)
{
    auto* widget = sessionHost.sessionWidget();
    auto& host   = editableHost(widget);
    if(!canReorderPlaylist(host.playlistController()->currentPlaylist())) {
        return;
    }

    auto* currentPlaylist    = host.playlistController()->currentPlaylist();
    const auto currentTracks = currentPlaylist->playlistTracks();
    const auto playlistId    = currentPlaylist->id();
    const auto sortToken     = beginSortRequest();

    auto handleSortedTracks = [hostPtr = &host, this, playlistId, currentTracks, trackCount = currentTracks.size(),
                               sortToken](const PlaylistTrackList& sortedTracks) {
        auto* activePlaylist = hostPtr->playlistController()->currentPlaylist();
        if(sortToken != m_sortRequestToken || !activePlaylist || activePlaylist->id() != playlistId
           || std::cmp_not_equal(activePlaylist->trackCount(), trackCount)) {
            finishSortRequest(sortToken, false);
            return;
        }

        auto* sortCmd = new ResetTracks(hostPtr->playerController(), hostPtr->playlistModel(), playlistId,
                                        currentTracks, sortedTracks);
        hostPtr->playlistController()->addToHistory(sortCmd);
    };

    if(host.playlistView()->selectionModel()->hasSelection()) {
        const auto selected = filterSelectedIndexes(host.playlistView());

        std::vector<int> indexesToSort;

        for(const QModelIndex& index : selected) {
            if(index.data(PlaylistItem::Type).toInt() == PlaylistItem::Track) {
                indexesToSort.push_back(index.data(PlaylistItem::Index).toInt());
            }
        }

        std::ranges::sort(indexesToSort);

        Utils::asyncExec([libraryManager = host.libraryManager(), currentTracks, script, indexesToSort]() {
            TrackSorter sorter{libraryManager};
            auto tracks = sorter.calcSortTracks(script, currentTracks, indexesToSort, PlaylistTrack::extractor);
            return PlaylistTrack::updateIndexes(tracks);
        }).then(widget, handleSortedTracks);
    }
    else {
        Utils::asyncExec([libraryManager = host.libraryManager(), currentTracks, script]() {
            TrackSorter sorter{libraryManager};
            auto tracks = sorter.calcSortTracks(script, currentTracks, PlaylistTrack::extractor);
            return PlaylistTrack::updateIndexes(tracks);
        }).then(widget, handleSortedTracks);
    }
}

void EditablePlaylistSession::sortColumn(PlaylistWidgetSessionHost& sessionHost, int column, Qt::SortOrder order)
{
    auto* widget = sessionHost.sessionWidget();
    auto& host   = editableHost(widget);
    if(!canReorderPlaylist(host.playlistController()->currentPlaylist()) || column < 0
       || std::cmp_greater_equal(column, host.layoutState().columns.size())) {
        return;
    }

    setSorting(true);
    setSortingColumn(true);

    auto* currentPlaylist    = host.playlistController()->currentPlaylist();
    const auto currentTracks = currentPlaylist->playlistTracks();
    const auto playlistId    = currentPlaylist->id();
    const QString sortField  = host.layoutState().columns.at(column).field;
    const auto sortToken     = beginSortRequest();

    Utils::asyncExec([libraryManager = host.libraryManager(), sortField, currentTracks, order]() {
        TrackSorter sorter{libraryManager};
        auto tracks = sorter.calcSortTracks(sortField, currentTracks, PlaylistTrack::extractor, order);
        return PlaylistTrack::updateIndexes(tracks);
    })
        .then(widget, [hostPtr = &host, this, playlistId, currentTracks, trackCount = currentTracks.size(),
                       sortToken](const PlaylistTrackList& sortedTracks) {
            auto* activePlaylist = hostPtr->playlistController()->currentPlaylist();
            if(sortToken != m_sortRequestToken || !activePlaylist || activePlaylist->id() != playlistId
               || std::cmp_not_equal(activePlaylist->trackCount(), trackCount)) {
                finishSortRequest(sortToken, true);
                return;
            }

            auto* sortCmd = new ResetTracks(hostPtr->playerController(), hostPtr->playlistModel(), playlistId,
                                            currentTracks, sortedTracks);
            hostPtr->playlistController()->addToHistory(sortCmd);
        });
}

QAction* EditablePlaylistSession::cropAction() const
{
    return m_cropAction;
}

QAction* EditablePlaylistSession::stopAfterAction() const
{
    return m_stopAfterAction;
}

QAction* EditablePlaylistSession::cutAction() const
{
    return m_cutAction;
}

QAction* EditablePlaylistSession::copyAction() const
{
    return m_copyAction;
}

QAction* EditablePlaylistSession::pasteAction() const
{
    return m_pasteAction;
}

QAction* EditablePlaylistSession::removeTrackAction() const
{
    return m_removeTrackAction;
}

QAction* EditablePlaylistSession::addToQueueAction() const
{
    return m_addToQueueAction;
}

QAction* EditablePlaylistSession::queueNextAction() const
{
    return m_queueNextAction;
}

QAction* EditablePlaylistSession::removeFromQueueAction() const
{
    return m_removeFromQueueAction;
}

void EditablePlaylistSession::handleTrackIndexesChanged(PlaylistWidget* widget, int playingIndex)
{
    auto& host = editableHost(widget);
    if(isSortingColumn()) {
        setSortingColumn(false);
    }
    else {
        host.resetSort(true);
    }

    if(!host.playlistController()->currentPlaylist()) {
        return;
    }

    const TrackList tracks = getAllTracks(host.playlistModel(), {QModelIndex{}});

    if(!tracks.empty()) {
        const QModelIndexList selected = filterSelectedIndexes(host.playlistView());
        if(!selected.empty()) {
            const TrackList selectedTracks = getAllTracks(host.playlistModel(), selected);
            std::vector<int> selectedIndexes;
            selectedIndexes.reserve(selected.size());
            for(const QModelIndex& index : selected) {
                selectedIndexes.emplace_back(index.data(PlaylistItem::Role::Index).toInt());
            }

            host.selectionController()->changeSelectedTracks(
                host.playlistContext(),
                makeTrackSelection(selectedTracks, selectedIndexes, host.playlistController()->currentPlaylist()));
        }
    }

    host.playlistController()->aboutToChangeTracks();
    host.playerController()->updateCurrentTrackIndex(playingIndex);
    host.playlistController()->playlistHandler()->replacePlaylistTracks(
        host.playlistController()->currentPlaylist()->id(), tracks);
    host.playlistController()->changedTracks();

    if(playingIndex >= 0 && !host.playerController()->currentIsQueueTrack()) {
        host.playlistController()->currentPlaylist()->changeCurrentIndex(playingIndex);
    }

    host.playlistModel()->updateHeader(host.playlistController()->currentPlaylist());
    setSorting(false);
}

void EditablePlaylistSession::stopAfterTrack(PlaylistWidget* widget) const
{
    auto& host = editableHost(widget);
    host.playlistModel()->stopAfterTrack(host.playlistView()->currentIndex());
}

void EditablePlaylistSession::handlePlayingTrackChanged(PlaylistWidget* widget, const PlaylistTrack& track) const
{
    auto& host = editableHost(widget);
    host.playlistModel()->playingTrackChanged(track);
    if(host.settingsManager()->value<Settings::Gui::CursorFollowsPlayback>()) {
        host.followCurrentTrack();
    }
}

void EditablePlaylistSession::selectTrackIds(PlaylistWidget* widget, const std::vector<int>& ids) const
{
    auto& host = editableHost(widget);
    QItemSelection selection;

    for(const int id : ids) {
        const QModelIndexList trackIndexes = host.playlistModel()->indexesOfTrackId(id);
        for(const QModelIndex& index : trackIndexes) {
            selection.select(index, index);
        }
    }

    if(!selection.empty()) {
        const QModelIndex firstIndex = selection.indexes().front();
        host.playlistView()->setCurrentIndex(firstIndex);
        host.playlistView()->selectionModel()->select(selection,
                                                      QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        host.playlistView()->scrollTo(firstIndex, QAbstractItemView::PositionAtCenter);
    }
}

void EditablePlaylistSession::filterTracks(PlaylistWidget* widget, const PlaylistTrackList& tracks)
{
    setFilteredTracks(tracks);
    editableHost(widget).resetModelThrottled();
}
} // namespace Fooyin
