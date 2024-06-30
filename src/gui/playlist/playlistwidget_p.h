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

#pragma once

#include "playlistcolumnregistry.h"
#include "playlistmodel.h"
#include "playlistpreset.h"
#include "presetregistry.h"

#include <core/library/sortingregistry.h>
#include <core/player/playbackqueue.h>

#include <QString>

class QHBoxLayout;
class QMenu;
class QAction;

namespace Fooyin {
class ActionManager;
class SettingsManager;
class SettingsDialogController;
class AutoHeaderView;
class WidgetContext;
class Playlist;
class TrackSelectionController;
class PlaylistWidget;
class PlaylistController;
class PlaylistInteractor;
class PlaylistModel;
class PlaylistView;
class MusicLibrary;
struct PlaylistViewState;
struct PlaylistTrack;

class PlaylistWidgetPrivate : public QObject
{
    Q_OBJECT

public:
    PlaylistWidgetPrivate(PlaylistWidget* self, ActionManager* actionManager, PlaylistInteractor* playlistInteractor,
                          CoverProvider* coverProvider, SettingsManager* settings);

    void setupConnections();
    void setupActions();

    void onColumnChanged(const PlaylistColumn& column);
    void onPresetChanged(const PlaylistPreset& preset);
    void changePreset(const PlaylistPreset& preset);

    void changePlaylist(Playlist* prevPlaylist, Playlist* playlist);

    void resetTree();
    [[nodiscard]] PlaylistViewState getState(Playlist* playlist) const;
    void saveState(Playlist* playlist) const;
    void restoreState(Playlist* playlist);
    void resetModel() const;

    [[nodiscard]] std::vector<int> selectedPlaylistIndexes() const;
    void restoreSelectedPlaylistIndexes(const std::vector<int>& indexes) const;

    [[nodiscard]] bool isHeaderHidden() const;
    [[nodiscard]] bool isScrollbarHidden() const;
    void setHeaderHidden(bool hide) const;
    void setScrollbarHidden(bool showScrollBar) const;

    void selectAll() const;
    void selectionChanged() const;
    void trackIndexesChanged(int playingIndex);
    void playSelectedTracks() const;
    void queueSelectedTracks() const;
    void dequeueSelectedTracks() const;

    void scanDroppedTracks(const QList<QUrl>& urls, int index);
    void tracksInserted(const TrackGroups& tracks) const;
    void tracksRemoved() const;
    void tracksMoved(const MoveOperation& operation) const;
    void clearTracks() const;

    void playlistTracksAdded(const TrackList& tracks, int index) const;
    void handleTracksChanged(const std::vector<int>& indexes, bool allNew);
    void handleQueueTracksChanged(const QueueTracks& removed, const QueueTracks& tracks);
    void handlePlayingTrackChanged(const PlaylistTrack& track) const;

    void setSingleMode(bool enabled);
    void updateSpans();
    void customHeaderMenuRequested(const QPoint& pos);

    void changeState(PlayState state) const;
    void doubleClicked(const QModelIndex& index) const;
    void followCurrentTrack() const;

    void sortTracks(const QString& script) const;
    void sortColumn(int column, Qt::SortOrder order);
    void resetSort(bool force = false);

    void addSortMenu(QMenu* parent, bool disabled);
    void addPresetMenu(QMenu* parent);

    PlaylistWidget* m_self;

    ActionManager* m_actionManager;
    PlaylistInteractor* m_playlistInteractor;
    PlaylistController* m_playlistController;
    PlayerController* m_playerController;
    TrackSelectionController* m_selectionController;
    MusicLibrary* m_library;
    SettingsManager* m_settings;
    SettingsDialogController* m_settingsDialog;

    PlaylistColumnRegistry m_columnRegistry;
    PresetRegistry m_presetRegistry;
    SortingRegistry m_sortRegistry;

    QHBoxLayout* m_layout;
    PlaylistModel* m_model;
    PlaylistView* m_playlistView;
    AutoHeaderView* m_header;

    PlaylistPreset m_currentPreset;
    bool m_singleMode;
    PlaylistColumnList m_columns;
    QByteArray m_headerState;

    WidgetContext* m_playlistContext;

    QAction* m_removeTrackAction;
    QAction* m_addToQueueAction;
    QAction* m_removeFromQueueAction;

    bool m_sorting;
    bool m_sortingColumn;
    bool m_showPlaying;
};
} // namespace Fooyin
