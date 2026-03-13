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

#pragma once

#include "playlistwidgetsession.h"

#include <core/playlist/playlistchangeset.h>

class QWidget;

namespace Fooyin {
class EditablePlaylistSession final : public PlaylistWidgetSession
{
public:
    EditablePlaylistSession();

    [[nodiscard]] PlaylistWidget::ModeCapabilities capabilities() const override;
    [[nodiscard]] QString emptyText() const override;
    [[nodiscard]] QString loadingText() const override;
    [[nodiscard]] int renderedTrackCount(const PlaylistController* playlistController) const override;
    [[nodiscard]] Playlist* modelPlaylist(Playlist* currentPlaylist) const override;
    [[nodiscard]] PlaylistTrackList modelTracks(Playlist* currentPlaylist) const override;
    [[nodiscard]] TrackSelection selection(Playlist* currentPlaylist, const TrackList& tracks,
                                           const std::set<int>& trackIndexes,
                                           const PlaylistTrack& firstTrack) const override;
    [[nodiscard]] bool canDequeue(const PlayerController* playerController, Playlist* currentPlaylist,
                                  const std::set<int>& trackIndexes,
                                  const std::set<Track>& selectedTracks) const override;
    [[nodiscard]] PlaylistTrackList searchSourceTracks(const PlaylistController* playlistController,
                                                       const MusicLibrary* library) const override;
    [[nodiscard]] bool restoreCurrentPlaylistOnFinalise() const override;

    void setupConnections(PlaylistWidgetSessionHost& host) override;
    void setupActions(PlaylistWidgetSessionHost& host, ActionContainer* editMenu,
                      const QStringList& editCategory) override;
    void requestPlaylistFocus(PlaylistWidgetSessionHost& host) override;
    void changePlaylist(PlaylistWidgetSessionHost& host, Playlist* prevPlaylist, Playlist* playlist) override;
    void destroy(PlaylistWidgetSessionHost& host) override;
    void handleAboutToBeReset(PlaylistWidgetSessionHost& host) override;
    void resetTree(PlaylistWidgetSessionHost& host) override;
    void deferFollowCurrentTrack(PlaylistWidgetSessionHost& host) override;
    void handleRestoredState(PlaylistWidgetSessionHost& host) override;
    void handleTracksChanged(PlaylistWidgetSessionHost& host, const std::vector<int>& indexes, bool allNew) override;
    void applyReadOnlyState(PlaylistWidgetSessionHost& host, bool readOnly) override;
    void updateContextMenuState(PlaylistWidgetSessionHost& host, const QModelIndexList& selected,
                                PlaylistWidget::ContextMenuState& state) override;
    void updateSelectionState(PlaylistWidgetSessionHost& host, const std::set<int>& trackIndexes,
                              const std::set<Track>& selectedTracks, const PlaylistTrack& firstTrack) override;
    void queueSelectedTracks(PlaylistWidgetSessionHost& host, bool next, bool send) override;
    void dequeueSelectedTracks(PlaylistWidgetSessionHost& host) override;
    void removeDuplicates(PlaylistWidgetSessionHost& host) override;
    void removeDeadTracks(PlaylistWidgetSessionHost& host) override;
    void scanDroppedTracks(PlaylistWidgetSessionHost& host, const QList<QUrl>& urls, int index) override;
    void tracksInserted(PlaylistWidgetSessionHost& host, const TrackGroups& tracks) override;
    void tracksRemoved(PlaylistWidgetSessionHost& host, const std::vector<int>& indexes) override;
    void tracksMoved(PlaylistWidgetSessionHost& host, const MoveOperation& operation) override;
    void clearTracks(PlaylistWidgetSessionHost& host) override;
    void cutTracks(PlaylistWidgetSessionHost& host) override;
    void copyTracks(PlaylistWidgetSessionHost& host) override;
    void pasteTracks(PlaylistWidgetSessionHost& host) override;
    void playlistTracksAdded(PlaylistWidgetSessionHost& host, const TrackList& tracks, int index) override;
    void cropSelection(PlaylistWidgetSessionHost& host) override;
    void sortTracks(PlaylistWidgetSessionHost& host, const QString& script) override;
    void sortColumn(PlaylistWidgetSessionHost& host, int column, Qt::SortOrder order) override;

    [[nodiscard]] QAction* cropAction() const override;
    [[nodiscard]] QAction* stopAfterAction() const override;
    [[nodiscard]] QAction* cutAction() const override;
    [[nodiscard]] QAction* copyAction() const override;
    [[nodiscard]] QAction* pasteAction() const override;
    [[nodiscard]] QAction* removeTrackAction() const override;
    [[nodiscard]] QAction* addToQueueAction() const override;
    [[nodiscard]] QAction* queueNextAction() const override;
    [[nodiscard]] QAction* removeFromQueueAction() const override;

private:
    [[nodiscard]] uint64_t beginSortRequest();
    void finishSortRequest(uint64_t token, bool sortingColumn);

    void applyPlaylistChangeSet(PlaylistWidgetSessionHost& host, const PlaylistChangeset& changeSet);
    void refreshActionState(PlaylistWidget* widget);
    void handleTrackIndexesChanged(PlaylistWidget* widget, int playingIndex);
    void stopAfterTrack(PlaylistWidget* widget) const;
    void handlePlayingTrackChanged(PlaylistWidget* widget, const PlaylistTrack& track) const;
    void selectTrackIds(PlaylistWidget* widget, const std::vector<int>& ids) const;
    void filterTracks(PlaylistWidget* widget, const PlaylistTrackList& tracks);
    void ensureActions(QWidget* parent);

    bool m_showPlaying;
    bool m_pendingFocus;
    int m_dropIndex;
    int m_currentIndex;
    uint64_t m_sortRequestToken;

    QAction* m_undoAction;
    QAction* m_redoAction;
    QAction* m_cropAction;
    QAction* m_stopAfterAction;
    QAction* m_cutAction;
    QAction* m_copyAction;
    QAction* m_pasteAction;
    QAction* m_clearAction;
    QAction* m_removeTrackAction;
    QAction* m_removeDuplicatesAction;
    QAction* m_removeDeadTracksAction;
    QAction* m_addToQueueAction;
    QAction* m_queueNextAction;
    QAction* m_removeFromQueueAction;
};
} // namespace Fooyin
