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

#include "playlisthistory.h"

#include "playlistmodel.h"

#include <stack>

namespace Fy::Gui::Widgets::Playlist {
PlaylistCommand::PlaylistCommand(PlaylistModel* model)
    : QUndoCommand{nullptr}
    , m_model{model}
{ }

InsertTracks::InsertTracks(PlaylistModel* model, TrackGroups groups)
    : PlaylistCommand{model}
    , m_trackGroups{std::move(groups)}
{ }

void InsertTracks::undo()
{
    m_model->removeTracks(m_trackGroups);
}

void InsertTracks::redo()
{
    m_model->insertTracks(m_trackGroups);
}

RemoveTracks::RemoveTracks(PlaylistModel* model, TrackGroups groups)
    : PlaylistCommand{model}
    , m_trackGroups{std::move(groups)}
{ }

void RemoveTracks::undo()
{
    m_model->insertTracks(m_trackGroups);
}

void RemoveTracks::redo()
{
    m_model->removeTracks(m_trackGroups);
}

MoveTracks::MoveTracks(PlaylistModel* model, TrackGroups groups, int row)
    : PlaylistCommand{model}
    , m_trackGroups{std::move(groups)}
    , m_row{row}
{
    Q_UNUSED(m_row)
}

void MoveTracks::undo() { }

void MoveTracks::redo() { }

} // namespace Fy::Gui::Widgets::Playlist
