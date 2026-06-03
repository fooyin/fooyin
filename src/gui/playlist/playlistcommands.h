/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <luket@pm.me>
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

#include <core/playlist/playlist.h>
#include <core/playlist/playlisthandler.h>

#include <QUndoCommand>

namespace Fooyin {
class PlaylistCommand : public QUndoCommand
{
public:
    PlaylistCommand(PlaylistHandler* handler, const UId& playlistId);

protected:
    PlaylistHandler* m_handler;
    UId m_playlistId;
};

class InsertTracks : public PlaylistCommand
{
public:
    InsertTracks(PlaylistHandler* handler, const UId& playlistId, TrackGroups groups);

    void undo() override;
    void redo() override;

private:
    PlaylistTrackList m_oldTracks;
    PlaylistTrackList m_newTracks;
};

class RemoveTracks : public PlaylistCommand
{
public:
    RemoveTracks(PlaylistHandler* handler, const UId& playlistId, std::vector<int> indexes);

    void undo() override;
    void redo() override;

private:
    PlaylistTrackList m_oldTracks;
    PlaylistTrackList m_newTracks;
};

class MoveTracks : public PlaylistCommand
{
public:
    MoveTracks(PlaylistHandler* handler, const UId& playlistId, MoveOperation operation);

    void undo() override;
    void redo() override;

private:
    PlaylistTrackList m_oldTracks;
    PlaylistTrackList m_newTracks;
};

class ResetTracks : public PlaylistCommand
{
public:
    ResetTracks(PlaylistHandler* handler, const UId& playlistId, PlaylistTrackList oldTracks,
                const PlaylistTrackList& newTracks);

    void undo() override;
    void redo() override;

private:
    PlaylistTrackList m_oldTracks;
    PlaylistTrackList m_newTracks;
};
} // namespace Fooyin
