/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
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

#include "fygui_export.h"

#include <core/track.h>
#include <utils/id.h>

#include <vector>

namespace Fooyin {
/*!
 * Edits playlists by ID using the shared undo history.
 * Implementations are accessed on the GUI thread; editing does not change
 * the playlist currently selected in the UI.
 */
class FYGUI_EXPORT PlaylistEditController
{
public:
    virtual ~PlaylistEditController() = default;

    virtual bool insertPlaylistItems(const UId& playlistId, int base, const TrackList& tracks)          = 0;
    virtual bool replacePlaylistItem(const UId& playlistId, int index, const TrackList& tracks)         = 0;
    virtual bool removePlaylistItems(const UId& playlistId, const std::vector<int>& indexes)            = 0;
    virtual bool clearPlaylist(const UId& playlistId)                                                   = 0;
    virtual bool movePlaylistItems(const UId& playlistId, const std::vector<int>& indexes, int newBase) = 0;
    virtual bool reorderPlaylistItems(const UId& playlistId, const std::vector<int>& order)             = 0;

    [[nodiscard]] virtual bool canUndo(const UId& playlistId) const = 0;
    [[nodiscard]] virtual bool canRedo(const UId& playlistId) const = 0;
    virtual bool undo(const UId& playlistId)                        = 0;
    virtual bool redo(const UId& playlistId)                        = 0;
};
} // namespace Fooyin
