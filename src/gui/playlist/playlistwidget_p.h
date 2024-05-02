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

#include <QCoro/QCoroTask>

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
                          SettingsManager* settings);

    void setupConnections();
    void setupActions();

    void onPresetChanged(const PlaylistPreset& preset);
    void changePreset(const PlaylistPreset& preset);

    void changePlaylist(Playlist* prevPlaylist, Playlist* playlist);

    void resetTree() const;
    [[nodiscard]] PlaylistViewState getState(Playlist* playlist) const;
    void saveState(Playlist* playlist) const;
    void restoreState(Playlist* playlist) const;
    void resetModel() const;

    [[nodiscard]] std::vector<int> selectedPlaylistIndexes() const;
    void restoreSelectedPlaylistIndexes(const std::vector<int>& indexes) const;

    [[nodiscard]] bool isHeaderHidden() const;
    [[nodiscard]] bool isScrollbarHidden() const;
    void setHeaderHidden(bool hide) const;
    void setScrollbarHidden(bool showScrollBar) const;

    void selectionChanged() const;
    void trackIndexesChanged(int playingIndex) const;
    void queueSelectedTracks() const;
    void dequeueSelectedTracks() const;

    void scanDroppedTracks(const QList<QUrl>& urls, int index);
    void tracksInserted(const TrackGroups& tracks) const;
    void tracksRemoved() const;
    void tracksMoved(const MoveOperation& operation) const;

    void playlistTracksAdded(const TrackList& tracks, int index) const;
    void handleTracksChanged(const std::vector<int>& indexes, bool allNew);
    void handleQueueTracksChanged(const QueueTracks& removed, const QueueTracks& tracks);
    void handlePlayingTrackChanged(const PlaylistTrack& track) const;

    void setSingleMode(bool enabled);
    void updateSpans();
    void customHeaderMenuRequested(const QPoint& pos);

    void changeState(PlayState state) const;
    void doubleClicked(const QModelIndex& index) const;
    void followCurrentTrack(const Track& track, int index) const;

    QCoro::Task<void> sortTracks(QString script);
    QCoro::Task<void> sortColumn(int column, Qt::SortOrder order);

    void addSortMenu(QMenu* parent);
    void addPresetMenu(QMenu* parent);

    PlaylistWidget* self;

    ActionManager* actionManager;
    PlaylistInteractor* playlistInteractor;
    PlaylistController* playlistController;
    PlayerController* playerController;
    TrackSelectionController* selectionController;
    MusicLibrary* library;
    SettingsManager* settings;
    SettingsDialogController* settingsDialog;

    PlaylistColumnRegistry columnRegistry;
    PresetRegistry presetRegistry;
    SortingRegistry sortRegistry;

    QHBoxLayout* layout;
    PlaylistModel* model;
    PlaylistView* playlistView;
    AutoHeaderView* header;

    PlaylistPreset currentPreset;
    bool singleMode;
    PlaylistColumnList columns;
    QByteArray headerState;

    WidgetContext* playlistContext;

    QAction* removeTrackAction;
    QAction* addToQueueAction;
    QAction* removeFromQueueAction;
};
} // namespace Fooyin
