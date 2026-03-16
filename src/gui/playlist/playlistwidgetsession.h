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

#include "playlistwidget.h"
#include "playlistwidgetsessionhost.h"

#include <QAbstractItemModel>
#include <QAbstractItemView>
#include <QModelIndex>
#include <QUrl>

#include <memory>
#include <set>
#include <vector>

namespace Fooyin {
class ActionContainer;

class PlaylistWidgetSession
{
public:
    struct State
    {
        bool sorting{false};
        bool sortingColumn{false};
        QString search;
        PlaylistTrackList filteredTracks;
    };

    virtual ~PlaylistWidgetSession() = default;

    [[nodiscard]] static std::unique_ptr<PlaylistWidgetSession> createEditable();
    [[nodiscard]] static std::unique_ptr<PlaylistWidgetSession> createDetachedPlaylist();
    [[nodiscard]] static std::unique_ptr<PlaylistWidgetSession> createDetachedLibrary();

    [[nodiscard]] virtual PlaylistWidget::ModeCapabilities capabilities() const                      = 0;
    [[nodiscard]] virtual QString emptyText() const                                                  = 0;
    [[nodiscard]] virtual QString loadingText() const                                                = 0;
    [[nodiscard]] virtual int renderedTrackCount(const PlaylistController* playlistController) const = 0;
    [[nodiscard]] virtual Playlist* modelPlaylist(Playlist* currentPlaylist) const                   = 0;
    [[nodiscard]] virtual PlaylistTrackList modelTracks(Playlist* currentPlaylist) const             = 0;
    [[nodiscard]] virtual TrackSelection selection(Playlist* currentPlaylist, const TrackList& tracks,
                                                   const std::set<int>& trackIndexes,
                                                   const PlaylistTrack& firstTrack) const            = 0;
    [[nodiscard]] virtual bool canDequeue(const PlayerController* playerController, Playlist* currentPlaylist,
                                          const std::set<int>& trackIndexes,
                                          const std::set<Track>& selectedTracks) const               = 0;
    [[nodiscard]] virtual PlaylistTrackList searchSourceTracks(const PlaylistController* playlistController,
                                                               const MusicLibrary* library) const    = 0;
    [[nodiscard]] virtual PlaylistAction::ActionOptions playbackOptions() const
    {
        return {};
    }
    [[nodiscard]] virtual bool restoreCurrentPlaylistOnFinalise() const
    {
        return false;
    }
    [[nodiscard]] virtual bool canResetWithoutPlaylist() const
    {
        return false;
    }

    virtual void setupConnections(PlaylistWidgetSessionHost& host);
    virtual void setupActions(PlaylistWidgetSessionHost& host, ActionContainer* editMenu,
                              const QStringList& editCategory);
    virtual void selectionChanged(PlaylistWidgetSessionHost& host);
    virtual void requestPlaylistFocus(PlaylistWidgetSessionHost& host);
    virtual void changePlaylist(PlaylistWidgetSessionHost& host, Playlist* prevPlaylist, Playlist* playlist);
    virtual void destroy(PlaylistWidgetSessionHost& host);
    virtual void handleAboutToBeReset(PlaylistWidgetSessionHost& host);
    virtual void resetTree(PlaylistWidgetSessionHost& host);
    virtual void handleTracksChanged(PlaylistWidgetSessionHost& host, const std::vector<int>& indexes, bool allNew);
    virtual void searchEvent(PlaylistWidgetSessionHost& host, const QString& search);
    virtual void handleSearchChanged(PlaylistWidgetSessionHost& host, const QString& search);
    virtual void finalise(PlaylistWidgetSessionHost& host);
    virtual void applyReadOnlyState(PlaylistWidgetSessionHost& host, bool readOnly);
    virtual void deferFollowCurrentTrack(PlaylistWidgetSessionHost& host);
    virtual void handleRestoredState(PlaylistWidgetSessionHost& host);
    virtual void updateContextMenuState(PlaylistWidgetSessionHost& host, const QModelIndexList& selected,
                                        PlaylistWidget::ContextMenuState& state);
    virtual void updateSelectionState(PlaylistWidgetSessionHost& host, const std::set<int>& trackIndexes,
                                      const std::set<Track>& selectedTracks, const PlaylistTrack& firstTrack);
    virtual void queueSelectedTracks(PlaylistWidgetSessionHost& host, bool next, bool send);
    virtual void dequeueSelectedTracks(PlaylistWidgetSessionHost& host);
    virtual void removeDuplicates(PlaylistWidgetSessionHost& host);
    virtual void removeDeadTracks(PlaylistWidgetSessionHost& host);
    virtual void scanDroppedTracks(PlaylistWidgetSessionHost& host, const QList<QUrl>& urls, int index);
    virtual void tracksInserted(PlaylistWidgetSessionHost& host, const TrackGroups& tracks);
    virtual void tracksRemoved(PlaylistWidgetSessionHost& host, const std::vector<int>& indexes);
    virtual void tracksMoved(PlaylistWidgetSessionHost& host, const MoveOperation& operation);
    virtual void clearTracks(PlaylistWidgetSessionHost& host);
    virtual void cutTracks(PlaylistWidgetSessionHost& host);
    virtual void copyTracks(PlaylistWidgetSessionHost& host);
    virtual void pasteTracks(PlaylistWidgetSessionHost& host);
    virtual void playlistTracksAdded(PlaylistWidgetSessionHost& host, const TrackList& tracks, int index);
    virtual void cropSelection(PlaylistWidgetSessionHost& host);
    virtual void sortTracks(PlaylistWidgetSessionHost& host, const QString& script);
    virtual void sortColumn(PlaylistWidgetSessionHost& host, int column, Qt::SortOrder order);
    [[nodiscard]] virtual QAction* cropAction() const;
    [[nodiscard]] virtual QAction* stopAfterAction() const;
    [[nodiscard]] virtual QAction* cutAction() const;
    [[nodiscard]] virtual QAction* copyAction() const;
    [[nodiscard]] virtual QAction* pasteAction() const;
    [[nodiscard]] virtual QAction* removeTrackAction() const;
    [[nodiscard]] virtual QAction* addToQueueAction() const;
    [[nodiscard]] virtual QAction* queueNextAction() const;
    [[nodiscard]] virtual QAction* removeFromQueueAction() const;

    [[nodiscard]] bool isSorting() const;
    [[nodiscard]] bool isSortingColumn() const;
    [[nodiscard]] bool hasSearch() const;
    [[nodiscard]] const QString& search() const;
    [[nodiscard]] const PlaylistTrackList& filteredTracks() const;

    void setSorting(bool sorting);
    void setSortingColumn(bool sortingColumn);
    void resetSortState(bool force);
    void setSearch(QString search);
    void clearSearch();
    void setFilteredTracks(PlaylistTrackList filteredTracks);

protected:
    static TrackList getTracks(const QModelIndexList& indexes);
    static void getAllTrackIndexes(QAbstractItemModel* model, const QModelIndex& parent, QModelIndexList& indexList);
    static QModelIndexList filterSelectedIndexes(const QAbstractItemView* view);
    static TrackSelection makeTrackSelection(const TrackList& tracks, const std::vector<int>& playlistIndexes,
                                             const Playlist* playlist);
    static TrackList getAllTracks(QAbstractItemModel* model, const QModelIndexList& indexes);
    static PlaylistViewState playlistViewState(const PlaylistWidgetSessionHost& host, Playlist* playlist);
    static void savePlaylistViewState(const PlaylistWidgetSessionHost& host, Playlist* playlist,
                                      bool requireVisible = true);
    static void restorePlaylistViewState(PlaylistWidgetSessionHost& host, Playlist* playlist);
    static std::vector<int> selectedPlaylistIndexes(const PlaylistWidgetSessionHost& host);
    static void restoreSelectedPlaylistIndexes(const PlaylistWidgetSessionHost& host, const std::vector<int>& indexes);

private:
    State m_state;
};

} // namespace Fooyin
