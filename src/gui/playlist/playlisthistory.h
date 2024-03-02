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

#include "playlistmodel.h"

#include <core/track.h>

#include <QUndoCommand>

namespace Fooyin {
class Playlist;

class PlaylistCommand : public QUndoCommand
{
public:
    explicit PlaylistCommand(PlaylistModel* model, int playlistId);

protected:
    PlaylistModel* m_model;
    int m_playlistId;
};

class InsertTracks : public PlaylistCommand
{
public:
    InsertTracks(PlayerManager* playerManager, PlaylistModel* model, int playlistId, TrackGroups groups);

    void undo() override;
    void redo() override;

private:
    PlayerManager* m_playerManager;
    TrackGroups m_trackGroups;
};

class RemoveTracks : public PlaylistCommand
{
public:
    RemoveTracks(PlayerManager* playerManager, PlaylistModel* model, int playlistId, TrackGroups groups);

    void undo() override;
    void redo() override;

private:
    PlayerManager* m_playerManager;
    TrackGroups m_trackGroups;
};

class MoveTracks : public PlaylistCommand
{
public:
    explicit MoveTracks(PlayerManager* playerManager, PlaylistModel* model, int playlistId, MoveOperation operation);

    void undo() override;
    void redo() override;

private:
    PlayerManager* m_playerManager;
    MoveOperation m_operation;
    MoveOperation m_undoOperation;
};
} // namespace Fooyin
