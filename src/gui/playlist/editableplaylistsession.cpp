/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
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

#include "core/library/tracksort.h"
#include "internalguisettings.h"
#include "playlist/playlistinteractor.h"
#include "playlist/presetregistry.h"
#include "playlistcommands.h"
#include "playlistcontroller.h"
#include "playlistuicontroller.h"
#include "playlistview.h"

#include <core/player/playercontroller.h>
#include <core/playlist/playlisthandler.h>
#include <gui/guiconstants.h>
#include <gui/guisettings.h>
#include <gui/iconloader.h>
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
#include <QScrollBar>

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
        m_stopAfterAction = new QAction(PlaylistWidget::tr("&Stop after this"), parent);
        Gui::setThemeIcon(m_stopAfterAction, Constants::Icons::Stop);
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
        m_removeTrackAction = new QAction(PlaylistWidget::tr("&Remove"), parent);
        Gui::setThemeIcon(m_removeTrackAction, Constants::Icons::Remove);
    }
    if(!m_removeDuplicatesAction) {
        m_removeDuplicatesAction = new QAction(PlaylistWidget::tr("Remove duplicates"), parent);
    }
    if(!m_removeDeadTracksAction) {
        m_removeDeadTracksAction = new QAction(PlaylistWidget::tr("Remove dead tracks"), parent);
    }
    if(!m_addToQueueAction) {
        m_addToQueueAction = new QAction(PlaylistWidget::tr("Add to playback &queue"), parent);
        Gui::setThemeIcon(m_addToQueueAction, Constants::Icons::Add);
    }
    if(!m_queueNextAction) {
        m_queueNextAction = new QAction(PlaylistWidget::tr("&Queue to play next"), parent);
        Gui::setThemeIcon(m_queueNextAction, Constants::Icons::Next);
    }
    if(!m_removeFromQueueAction) {
        m_removeFromQueueAction = new QAction(PlaylistWidget::tr("Remove from playback q&ueue"), parent);
        Gui::setThemeIcon(m_removeFromQueueAction, Constants::Icons::Remove);
    }
}

void EditablePlaylistSession::setupConnections(PlaylistWidgetSessionHost& sessionHost)
{
    auto* widget = sessionHost.sessionWidget();
    auto& host   = editableHost(widget);

    QObject::connect(host.presetRegistry(), &PresetRegistry::presetChanged, widget,
                     [widget](const PlaylistPreset& preset) { editableHost(widget).handlePresetChanged(preset); });

    QObject::connect(widget->playlistView(), &ExpandedTreeView::middleClicked, widget, &PlaylistWidget::middleClicked);
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
                     [widget, this](const PlaylistTrack& track) {
                         auto& editableSession = editableHost(widget);
                         handlePlayingTrackChanged(widget, track);
                         if(editableSession.settingsManager()->value<Settings::Gui::CursorFollowsPlayback>()) {
                             followCurrentTrack(editableHost(widget));
                         }
                     });
    QObject::connect(widget->playerController(), &PlayerController::trackChangeRequested, widget,
                     [widget](const Player::TrackChangeRequest& request) {
                         widget->playlistModel()->playingTrackChanged(request.track);
                     });
    QObject::connect(widget->playlistController(), &PlaylistController::playStateChanged, widget->playlistModel(),
                     &PlaylistModel::playStateChanged);
    QObject::connect(widget->playerController(), &PlayerController::positionChangedSeconds, widget->playlistModel(),
                     [widget](uint64_t) { widget->playlistModel()->refreshPlayingTrackPositionData(); });
    QObject::connect(widget->playerController(), &PlayerController::positionMoved, widget->playlistModel(),
                     [widget](uint64_t) { widget->playlistModel()->refreshPlayingTrackPositionData(); });
    QObject::connect(widget->playerController(), &PlayerController::bitrateChanged, widget->playlistModel(),
                     [widget](int) { widget->playlistModel()->refreshPlayingTrackBitrateData(); });
    QObject::connect(widget->playlistController(), &PlaylistController::currentPlaylistTracksRemoved, widget,
                     [widget, this](const std::vector<int>& indexes) {
                         handlePlaylistTracksRemoved(widgetSessionHost(widget), indexes);
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
                     [widget, this]() { followCurrentTrack(editableHost(widget)); });
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
    Context actionContext = host.playlistContext()->context();
    actionContext.erase(Constants::Context::TrackSelection);

    m_undoAction->setStatusTip(PlaylistWidget::tr("Undo the previous playlist change"));
    auto* undoCmd = host.actionManager()->registerAction(m_undoAction, Constants::Actions::Undo, actionContext);
    undoCmd->setCategories(editCategory);
    undoCmd->setDefaultShortcut(QKeySequence::Undo);
    editMenu->addAction(undoCmd);
    QObject::connect(m_undoAction, &QAction::triggered, widget,
                     [widget]() { editableHost(widget).playlistController()->undoPlaylistChanges(); });
    QObject::connect(host.playlistController(), &PlaylistController::playlistHistoryChanged, widget,
                     [this, widget]() { refreshActionState(widget); });

    m_redoAction->setStatusTip(PlaylistWidget::tr("Redo the previous playlist change"));
    auto* redoCmd = host.actionManager()->registerAction(m_redoAction, Constants::Actions::Redo, actionContext);
    redoCmd->setCategories(editCategory);
    redoCmd->setDefaultShortcut(QKeySequence::Redo);
    editMenu->addAction(redoCmd);
    QObject::connect(m_redoAction, &QAction::triggered, widget,
                     [widget]() { editableHost(widget).playlistController()->redoPlaylistChanges(); });
    QObject::connect(host.playlistController(), &PlaylistController::playlistHistoryChanged, widget,
                     [this, widget]() { refreshActionState(widget); });

    editMenu->addSeparator();

    m_cutAction->setStatusTip(PlaylistWidget::tr("Cut the selected tracks"));
    auto* cutCommand = host.actionManager()->registerAction(m_cutAction, Constants::Actions::Cut, actionContext);
    cutCommand->setCategories(editCategory);
    cutCommand->setDefaultShortcut(QKeySequence::Cut);
    editMenu->addAction(cutCommand);
    QObject::connect(m_cutAction, &QAction::triggered, widget,
                     [widget, this]() { cutTracks(widgetSessionHost(widget)); });

    m_copyAction->setStatusTip(PlaylistWidget::tr("Copy the selected tracks"));
    auto* copyCommand = host.actionManager()->registerAction(m_copyAction, Constants::Actions::Copy, actionContext);
    copyCommand->setCategories(editCategory);
    copyCommand->setDefaultShortcut(QKeySequence::Copy);
    editMenu->addAction(copyCommand);
    QObject::connect(m_copyAction, &QAction::triggered, widget,
                     [widget, this]() { copyTracks(widgetSessionHost(widget)); });
    m_copyAction->setEnabled(host.playlistView()->selectionModel()->hasSelection());

    m_pasteAction->setStatusTip(PlaylistWidget::tr("Paste the selected tracks"));
    auto* pasteCommand = host.actionManager()->registerAction(m_pasteAction, Constants::Actions::Paste, actionContext);
    pasteCommand->setCategories(editCategory);
    pasteCommand->setDefaultShortcut(QKeySequence::Paste);
    editMenu->addAction(m_pasteAction);
    QObject::connect(host.playlistController(), &PlaylistController::clipboardChanged, widget,
                     [this, widget]() { refreshActionState(widget); });
    QObject::connect(m_pasteAction, &QAction::triggered, widget,
                     [widget, this]() { pasteTracks(widgetSessionHost(widget)); });

    editMenu->addSeparator();

    m_clearAction->setStatusTip(PlaylistWidget::tr("Remove all tracks from the current playlist"));
    auto* clearCmd = host.actionManager()->registerAction(m_clearAction, Constants::Actions::Clear, actionContext);
    clearCmd->setCategories(editCategory);
    clearCmd->setAttribute(ProxyAction::UpdateText);
    editMenu->addAction(clearCmd);
    QObject::connect(m_clearAction, &QAction::triggered, widget,
                     [widget, this]() { clearTracks(widgetSessionHost(widget)); });

    m_removeTrackAction->setStatusTip(PlaylistWidget::tr("Remove the selected tracks from the current playlist"));
    m_removeTrackAction->setEnabled(false);
    auto* removeCmd
        = host.actionManager()->registerAction(m_removeTrackAction, Constants::Actions::Remove, actionContext);
    removeCmd->setCategories(editCategory);
    removeCmd->setAttribute(ProxyAction::UpdateText);
    removeCmd->setDefaultShortcut(QKeySequence::Delete);
    QObject::connect(m_removeTrackAction, &QAction::triggered, widget, [widget, this]() {
        auto& removeHost   = widgetSessionHost(widget);
        const auto indexes = selectedPlaylistIndexes(removeHost);
        tracksRemoved(removeHost, indexes);
        selectTrackAfterRemoval(removeHost, indexes);
    });

    m_addToQueueAction->setStatusTip(PlaylistWidget::tr("Add the selected tracks to the playback queue"));
    m_addToQueueAction->setEnabled(false);
    host.actionManager()->registerAction(m_addToQueueAction, Constants::Actions::AddToQueue, actionContext);
    QObject::connect(m_addToQueueAction, &QAction::triggered, widget,
                     [widget, this]() { queueSelectedTracks(widgetSessionHost(widget), false, false); });

    m_queueNextAction->setStatusTip(PlaylistWidget::tr("Add the selected tracks to the front of the playback queue"));
    m_queueNextAction->setEnabled(false);
    host.actionManager()->registerAction(m_queueNextAction, Constants::Actions::QueueNext, actionContext);
    QObject::connect(m_queueNextAction, &QAction::triggered, widget,
                     [widget, this]() { queueSelectedTracks(widgetSessionHost(widget), true, false); });

    m_removeFromQueueAction->setStatusTip(PlaylistWidget::tr("Remove the selected tracks from the playback queue"));
    m_removeFromQueueAction->setVisible(false);
    host.actionManager()->registerAction(m_removeFromQueueAction, Constants::Actions::RemoveFromQueue, actionContext);
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
    Q_EMIT widget->changeSearch(currentSearch);

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

    if(changeSet.requiresReset || changeSet.replacesAllEntries || hasSearch()) {
        handleTracksChanged(sessionHost, {}, changeSet.replacesAllEntries);
        return;
    }

    QModelIndexList trackIndexesToRemove;
    for(const UId& entryId : changeSet.removedEntries) {
        const QModelIndex trackIndex = host.playlistModel()->indexAtTrackEntry(entryId);
        trackIndexesToRemove.push_back(trackIndex);
    }
    if(!trackIndexesToRemove.empty()) {
        host.playlistModel()->removeTracks(trackIndexesToRemove);
    }

    const auto applyMovesAndUpdates
        = [model = host.playlistModel(), moves = changeSet.moves, updatedEntries = changeSet.updatedEntries,
           currentPlaylist, playerController = host.playerController(), this, widget = sessionHost.sessionWidget()] {
              if(editableHost(widget).playlistController()->currentPlaylist() != currentPlaylist) {
                  return;
              }

              for(const auto& move : moves) {
                  if(!move.isValid()) {
                      continue;
                  }

                  const QModelIndex sourceIndex = model->indexAtTrackEntry(move.entryId);
                  MoveOperation operation;
                  const int sourcePlaylistIndex = sourceIndex.data(PlaylistItem::Role::Index).toInt();
                  operation.emplace_back(move.targetIndex, TrackIndexRangeList{{.first = sourcePlaylistIndex,
                                                                                .last  = sourcePlaylistIndex}});
                  model->moveTracks(operation);
              }

              std::vector<int> updatedIndexes;
              updatedIndexes.reserve(updatedEntries.size());
              for(const UId& entryId : updatedEntries) {
                  const int index = currentPlaylist ? currentPlaylist->indexOfTrackEntry(entryId) : -1;
                  updatedIndexes.push_back(index);
              }

              if(!updatedIndexes.empty()) {
                  QObject::connect(
                      model, &PlaylistModel::trackGroupApplied, widget,
                      [widget, this]() {
                          auto& hostRef = editableHost(widget);
                          handleTrackIndexesChanged(widget, hostRef.playlistModel()->playingTrack().indexInPlaylist);
                      },
                      Qt::SingleShotConnection);
                  model->updateTracks(updatedIndexes);
              }
              else {
                  auto& hostRef = editableHost(widget);
                  handleTrackIndexesChanged(widget, hostRef.playlistModel()->playingTrack().indexInPlaylist);
              }

              model->updateHeader(currentPlaylist);
              model->playingTrackChanged(playerController->currentPlaylistTrack());
              refreshActionState(widget);
          };

    TrackGroups insertions;
    for(const auto& insertion : changeSet.insertions) {
        if(insertion.isValid()) {
            insertions[insertion.index] = insertion.tracks;
        }
    }

    if(!insertions.empty()) {
        QObject::connect(host.playlistModel(), &PlaylistModel::trackGroupApplied, sessionHost.sessionWidget(),
                         applyMovesAndUpdates, Qt::SingleShotConnection);
        host.playlistModel()->insertTracks(insertions);
    }
    else {
        applyMovesAndUpdates();
    }
}

void EditablePlaylistSession::handlePlaylistTracksRemoved(PlaylistWidgetSessionHost& sessionHost,
                                                          const std::vector<int>& indexes)
{
    auto& host            = editableHost(sessionHost.sessionWidget());
    auto* currentPlaylist = host.playlistController()->currentPlaylist();
    if(!currentPlaylist || indexes.empty()) {
        return;
    }

    if(hasSearch()) {
        handleTracksChanged(sessionHost, {}, false);
        return;
    }

    QModelIndexList trackIndexesToRemove;
    for(const int index : indexes) {
        const QModelIndex trackIndex = host.playlistModel()->indexAtPlaylistIndex(index, false);
        if(trackIndex.isValid()) {
            trackIndexesToRemove.push_back(trackIndex);
        }
    }

    if(!trackIndexesToRemove.empty()) {
        host.playlistModel()->removeTracks(trackIndexesToRemove);
    }

    host.playlistModel()->updateHeader(currentPlaylist);
    refreshActionState(sessionHost.sessionWidget());
    handleTrackIndexesChanged(sessionHost.sessionWidget(), host.playlistModel()->playingTrack().indexInPlaylist);
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

    if(host.playlistView()->hasActiveInlineEditor()) {
        refreshActionState(sessionHost.sessionWidget());
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
    handleDeferredFollowTrack(sessionHost);
}

void EditablePlaylistSession::handleDeferredFollowTrack(PlaylistWidgetSessionHost& sessionHost)
{
    auto& host = editableHost(sessionHost.sessionWidget());
    if(m_showPlaying && host.followCurrentTrack()) {
        m_showPlaying = false;
    }
}

void EditablePlaylistSession::followCurrentTrack(PlaylistWidgetSessionHost& sessionHost)
{
    auto* widget = sessionHost.sessionWidget();
    if(widget->playlistController()->currentIsActive() && widget->playlistModel()->playlistIsLoaded()
       && widget->followCurrentTrack()) {
        return;
    }

    deferFollowCurrentTrack(widgetSessionHost(widget));
}

void EditablePlaylistSession::handleTracksChanged(PlaylistWidgetSessionHost& sessionHost,
                                                  const std::vector<int>& indexes, bool allNew)
{
    auto* widget          = sessionHost.sessionWidget();
    auto& host            = editableHost(widget);
    auto* currentPlaylist = host.playlistController()->currentPlaylist();
    if(!currentPlaylist) {
        return;
    }

    if(!isSortingColumn()) {
        host.resetSort(true);
    }

    if(host.playlistView()->hasActiveInlineEditor() && !indexes.empty()) {
        host.playlistModel()->updateTracks(indexes);
        return;
    }

    setFilteredTracks({});
    const PlaylistViewState viewState = playlistViewState(host, currentPlaylist);
    host.playlistController()->setSearch(currentPlaylist, {});
    clearSearch();
    Q_EMIT widget->changeSearch({});

    auto restoreViewState
        = [&host, currentPlaylist, viewState, allNew](int currentIndex, const std::vector<int>& selectedIndexes) {
              if(host.playlistView()->hasActiveInlineEditor() || !currentPlaylist) {
                  return;
              }

              if(allNew) {
                  host.playlistView()->playlistReset();
                  host.playlistView()->scrollToTop();
                  return;
              }

              if(currentPlaylist->trackCount() > 0 && viewState.topIndex >= 0) {
                  const QModelIndex topIndex = host.playlistModel()->indexAtPlaylistIndex(viewState.topIndex, false);
                  if(topIndex.isValid()) {
                      host.playlistView()->scrollTo(topIndex, PlaylistView::PositionAtTop);
                      host.playlistView()->verticalScrollBar()->setValue(viewState.scrollPos);
                  }
              }
              host.playlistView()->playlistReset();

              restoreSelectedPlaylistIndexes(host, selectedIndexes);
              if(currentIndex >= 0) {
                  const QModelIndex index = host.playlistModel()->indexAtPlaylistIndex(currentIndex, true);
                  if(index.isValid()) {
                      host.playlistView()->selectionModel()->setCurrentIndex(index, QItemSelectionModel::NoUpdate);
                  }
              }
          };

    int currentIndex{-1};
    if(host.playlistView()->currentIndex().isValid()) {
        currentIndex = host.playlistView()->currentIndex().data(PlaylistItem::Index).toInt();
    }
    const std::vector<int> selectedIndexes = selectedPlaylistIndexes(host);

    auto finishTrackChange
        = [hostPtr = &host, this, currentPlaylist, currentIndex, selectedIndexes, restoreViewState]() {
              if(hostPtr->playlistController()->currentPlaylist() != currentPlaylist) {
                  return;
              }

              restoreViewState(currentIndex, selectedIndexes);
              syncTrackChangeState(*hostPtr, hostPtr->playlistModel()->playingTrack().indexInPlaylist);
          };

    const int changedTrackCount = static_cast<int>(indexes.size());
    if(indexes.empty() || changedTrackCount > 500 || changedTrackCount > (currentPlaylist->trackCount() / 2)) {
        QObject::connect(host.playlistModel(), &PlaylistModel::playlistLoaded, widget, finishTrackChange,
                         Qt::SingleShotConnection);
        host.resetModelThrottled();
        return;
    }

    QObject::connect(host.playlistModel(), &PlaylistModel::trackGroupApplied, widget, finishTrackChange,
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
        const auto track = index.data(PlaylistItem::Role::PersistentItemData).value<PlaylistTrack>();
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
        const auto track = index.data(PlaylistItem::Role::PersistentItemData).value<PlaylistTrack>();
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
            auto* insertCommand = new InsertTracks(editableHostRef.playlistController()->playlistHandler(), playlistId,
                                                   {{m_dropIndex, playlistTracks}});
            editableHostRef.playlistController()->addToHistory(insertCommand);

            if(m_dropIndex >= 0) {
                m_dropIndex += static_cast<int>(tracks.size());
            }
        });
}

void EditablePlaylistSession::tracksInserted(PlaylistWidgetSessionHost& sessionHost, const TrackGroups& tracks)
{
    auto& host    = editableHost(sessionHost.sessionWidget());
    auto* command = new InsertTracks(host.playlistController()->playlistHandler(),
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

    auto* delCmd = new RemoveTracks(host.playlistController()->playlistHandler(),
                                    host.playlistController()->currentPlaylist()->id(), indexes);
    host.playlistController()->addToHistory(delCmd);

    refreshActionState(sessionHost.sessionWidget());
    host.playlistModel()->updateHeader(host.playlistController()->currentPlaylist());
}

void EditablePlaylistSession::tracksMoved(PlaylistWidgetSessionHost& sessionHost, const MoveOperation& operation)
{
    auto& host    = editableHost(sessionHost.sessionWidget());
    auto* moveCmd = new MoveTracks(host.playlistController()->playlistHandler(),
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
        = new ResetTracks(host.playlistController()->playlistHandler(), host.playlistController()->currentPlaylistId(),
                          host.playlistController()->currentPlaylist()->playlistTracks(), {});
    host.playlistController()->addToHistory(resetCmd);
}

void EditablePlaylistSession::cutTracks(PlaylistWidgetSessionHost& sessionHost)
{
    copyTracks(sessionHost);
    const auto indexes = selectedPlaylistIndexes(editableHost(sessionHost.sessionWidget()));
    tracksRemoved(sessionHost, indexes);
    selectTrackAfterRemoval(sessionHost, indexes);
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
        insertIndex = std::ranges::max(selected | std::views::transform([](const QModelIndex& index) {
                                           return index.data(PlaylistItem::Index).toInt();
                                       }))
                    + 1;
    }

    const UId playlistId      = host.playlistController()->currentPlaylist()->id();
    const auto playlistTracks = PlaylistTrack::fromTracks(tracks, playlistId);
    auto* insertCommand
        = new InsertTracks(host.playlistController()->playlistHandler(), playlistId, {{insertIndex, playlistTracks}});
    host.playlistController()->addToHistory(insertCommand);
}

void EditablePlaylistSession::cropSelection(PlaylistWidgetSessionHost& sessionHost)
{
    auto& host            = editableHost(sessionHost.sessionWidget());
    auto* currentPlaylist = host.playlistController()->currentPlaylist();
    if(!currentPlaylist) {
        return;
    }

    const auto selected = filterSelectedIndexes(host.playlistView());
    std::set<int> selectedTrackIndexes;
    for(const QModelIndex& index : selected) {
        if(index.isValid() && index.data(PlaylistItem::Type).toInt() == PlaylistItem::Track) {
            selectedTrackIndexes.emplace(index.data(PlaylistItem::Index).toInt());
        }
    }

    const int selectedCount      = static_cast<int>(selectedTrackIndexes.size());
    const int playlistTrackCount = currentPlaylist->trackCount();

    if(selectedCount == 0 || selectedCount >= playlistTrackCount) {
        return;
    }

    QModelIndexList allTrackIndexes;
    getAllTrackIndexes(host.playlistModel(), {}, allTrackIndexes);

    std::vector<int> indexes;

    for(const QModelIndex& index : std::as_const(allTrackIndexes)) {
        const int trackIndex = index.data(PlaylistItem::Index).toInt();
        if(!selectedTrackIndexes.contains(trackIndex)) {
            indexes.emplace_back(trackIndex);
        }
    }

    if(indexes.empty()) {
        return;
    }

    auto* delCmd = new RemoveTracks(host.playlistController()->playlistHandler(), currentPlaylist->id(), indexes);
    host.playlistController()->addToHistory(delCmd);

    refreshActionState(sessionHost.sessionWidget());
    host.playlistModel()->updateHeader(currentPlaylist);
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

        auto* sortCmd = new ResetTracks(hostPtr->playlistController()->playlistHandler(), playlistId, currentTracks,
                                        sortedTracks);
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

    auto* currentPlaylist            = host.playlistController()->currentPlaylist();
    const auto currentTracks         = currentPlaylist->playlistTracks();
    const auto playlistId            = currentPlaylist->id();
    const PlaylistColumn& sortColumn = host.layoutState().columns.at(column);
    const QString sortField          = !sortColumn.sortField.isEmpty() ? sortColumn.sortField : sortColumn.field;
    const auto sortToken             = beginSortRequest();

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

            auto* sortCmd = new ResetTracks(hostPtr->playlistController()->playlistHandler(), playlistId, currentTracks,
                                            sortedTracks);
            hostPtr->playlistController()->addToHistory(sortCmd);
        });
}

void EditablePlaylistSession::selectTrackAfterRemoval(PlaylistWidgetSessionHost& sessionHost,
                                                      const std::vector<int>& removedIndexes)
{
    auto& host            = editableHost(sessionHost.sessionWidget());
    auto* currentPlaylist = host.playlistController()->currentPlaylist();
    if(!currentPlaylist || removedIndexes.empty() || currentPlaylist->trackCount() <= 0) {
        return;
    }

    auto validIndexes   = removedIndexes | std::views::filter([](int index) { return index >= 0; });
    const auto targetIt = std::ranges::min_element(validIndexes);
    if(targetIt == std::ranges::end(validIndexes)) {
        return;
    }

    const int targetPlaylistIndex = std::min(*targetIt, currentPlaylist->trackCount() - 1);
    const QModelIndex targetIndex = host.playlistModel()->indexAtPlaylistIndex(targetPlaylistIndex, false);
    if(!targetIndex.isValid()) {
        return;
    }

    host.playlistView()->selectionModel()->setCurrentIndex(targetIndex, QItemSelectionModel::ClearAndSelect
                                                                            | QItemSelectionModel::Rows);
    host.playlistView()->scrollTo(targetIndex, QAbstractItemView::EnsureVisible);
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

void EditablePlaylistSession::syncTrackChangeState(EditablePlaylistSessionHost& host, int playingIndex)
{
    if(!host.playlistController()->currentPlaylist()) {
        return;
    }

    const QModelIndexList selectedIndexes = filterSelectedIndexes(host.playlistView());
    if(!selectedIndexes.empty()) {
        const TrackList selectedTracks = getAllTracks(host.playlistModel(), selectedIndexes);
        std::vector<int> playlistIndexes;
        playlistIndexes.reserve(selectedIndexes.size());
        for(const QModelIndex& index : selectedIndexes) {
            playlistIndexes.emplace_back(index.data(PlaylistItem::Role::Index).toInt());
        }

        host.selectionController()->changeSelectedTracks(
            host.playlistContext(),
            makeTrackSelection(selectedTracks, playlistIndexes, host.playlistController()->currentPlaylist()));
    }

    if(playingIndex >= 0 && !host.playerController()->currentIsQueueTrack()
       && host.playerController()->currentPlaylistTrack().playlistId
              != host.playlistController()->currentPlaylist()->id()) {
        host.playlistController()->currentPlaylist()->changeCurrentIndex(playingIndex);
    }

    host.playlistModel()->updateHeader(host.playlistController()->currentPlaylist());
    setSorting(false);
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

    syncTrackChangeState(host, playingIndex);
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
