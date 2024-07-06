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
class PlayerController;

class PlaylistCommand : public QUndoCommand
{
public:
    PlaylistCommand(PlayerController* playerController, PlaylistModel* model, const UId& playlistId);

protected:
    PlayerController* m_playerController;
    PlaylistModel* m_model;
    UId m_playlistId;
};

class InsertTracks : public PlaylistCommand
{
public:
    InsertTracks(PlayerController* playerController, PlaylistModel* model, const UId& playlistId, TrackGroups groups);

    void undo() override;
    void redo() override;

private:
    TrackGroups m_trackGroups;
};

class RemoveTracks : public PlaylistCommand
{
public:
    RemoveTracks(PlayerController* playerController, PlaylistModel* model, const UId& playlistId, TrackGroups groups);

    void undo() override;
    void redo() override;

private:
    TrackGroups m_trackGroups;
};

class MoveTracks : public PlaylistCommand
{
public:
    MoveTracks(PlayerController* playerController, PlaylistModel* model, const UId& playlistId,
               MoveOperation operation);

    void undo() override;
    void redo() override;

private:
    MoveOperation m_operation;
    MoveOperation m_undoOperation;
};

class ResetTracks : public PlaylistCommand
{
public:
    ResetTracks(PlayerController* playerController, PlaylistModel* model, const UId& playlistId, TrackList oldTracks,
                TrackList newTracks);

    void undo() override;
    void redo() override;

private:
    TrackList m_oldTracks;
    TrackList m_newTracks;
};
} // namespace Fooyin
