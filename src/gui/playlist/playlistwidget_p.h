/*
 * Fooyin
 * Copyright 2022-2023, Luke Taylor <LukeT1@proton.me>
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

#include "playlistmodel.h"
#include "playlistpreset.h"

#include <core/player/playermanager.h>

#include <QCoroTask>

#include <QString>
#include <QUndoStack>

class QHBoxLayout;
class QMenu;

namespace Fooyin {
class ActionManager;
class SettingsManager;
class SettingsDialogController;
class HeaderView;
class WidgetContext;
class Playlist;
class TrackSelectionController;
class PlaylistWidget;
class PlaylistController;
class PlaylistModel;
class PlaylistView;

class PlaylistWidgetPrivate : public QObject
{
    Q_OBJECT

public:
    PlaylistWidgetPrivate(PlaylistWidget* self, ActionManager* actionManager, PlaylistController* playlistController,
                          SettingsManager* settings);

    void setupConnections();
    void setupActions();

    void onPresetChanged(const PlaylistPreset& preset);
    void changePreset(const PlaylistPreset& preset);

    void changePlaylist(Playlist* playlist) const;

    void resetTree() const;

    [[nodiscard]] bool isHeaderHidden() const;
    [[nodiscard]] bool isScrollbarHidden() const;

    void setHeaderHidden(bool showHeader) const;
    void setScrollbarHidden(bool showScrollBar) const;

    void selectionChanged() const;
    void playlistTracksChanged(int index) const;
    void tracksInserted(const TrackGroups& tracks) const;
    void tracksRemoved() const;
    void tracksMoved(const MoveOperation& operation) const;
    void playlistTracksAdded(Playlist* playlist, const TrackList& tracks, int index) const;

    void customHeaderMenuRequested(QPoint pos);

    void changeState(PlayState state) const;

    void doubleClicked(const QModelIndex& index) const;

    void followCurrentTrack(const Track& track, int index) const;

    void switchContextMenu(int section, QPoint pos);
    QCoro::Task<void> changeSort(QString script);
    void addSortMenu(QMenu* parent);

    PlaylistWidget* self;

    ActionManager* actionManager;
    TrackSelectionController* selectionController;
    SettingsManager* settings;
    SettingsDialogController* settingsDialog;

    PlaylistController* playlistController;

    QHBoxLayout* layout;
    PlaylistModel* model;
    PlaylistView* playlistView;
    HeaderView* header;

    PlaylistPreset currentPreset;

    WidgetContext* playlistContext;
};
} // namespace Fooyin
