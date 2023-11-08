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

#include <core/track.h>

#include <QModelIndexList>
#include <QUndoCommand>

namespace Fy {

namespace Core::Playlist {
class Playlist;
} // namespace Core::Playlist

namespace Gui::Widgets::Playlist {
class PlaylistCommand : public QUndoCommand
{
public:
    explicit PlaylistCommand(PlaylistModel* model);

protected:
    PlaylistModel* m_model;
};

class InsertTracks : public PlaylistCommand
{
public:
    InsertTracks(PlaylistModel* model, TrackGroups groups);

    void undo() override;
    void redo() override;

private:
    TrackGroups m_trackGroups;
};

class RemoveTracks : public PlaylistCommand
{
public:
    RemoveTracks(PlaylistModel* model, TrackGroups groups);

    void undo() override;
    void redo() override;

private:
    TrackGroups m_trackGroups;
};

class MoveTracks : public PlaylistCommand
{
public:
    explicit MoveTracks(PlaylistModel* model, TrackGroups groups, int row);

    void undo() override;
    void redo() override;

private:
    TrackGroups m_trackGroups;
    int m_row;
};

// class SortTracks : public PlaylistCommand
// {
// public:
//     explicit SortTracks(PlaylistModel* model, int column,
//                         Qt::SortOrder order /*, const PlaylistItemPtrList &new_items*/);

//     void undo() override;
//     void redo() override;

// private:
// PlaylistItemPtrList old_items_;
// PlaylistItemPtrList new_items_;
// };
} // namespace Gui::Widgets::Playlist
} // namespace Fy
