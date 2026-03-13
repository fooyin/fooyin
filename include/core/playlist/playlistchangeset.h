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

#include "fycore_export.h"

#include <core/track.h>

#include <optional>
#include <unordered_set>

namespace Fooyin {
/*!
 * Represents a single track move within a playlist patch.
 * The move is applied against the current playlist state at the time the
 * operation is executed.
 */
struct FYCORE_EXPORT PlaylistTrackMove
{
    /** Current playlist index of the track to move. */
    int sourceIndex{-1};
    /** Destination playlist index after the move is applied. */
    int targetIndex{-1};

    [[nodiscard]] bool isValid() const
    {
        return sourceIndex >= 0 && targetIndex >= 0 && sourceIndex != targetIndex;
    }
};

/*!
 * Represents a contiguous group of tracks to insert into a playlist patch.
 * @note @c index is the destination playlist index before the insertion is applied.
 */
struct FYCORE_EXPORT PlaylistTrackInsertion
{
    /** Destination playlist index for the first inserted track. */
    int index{-1};
    /** Tracks to insert, in final order. */
    TrackList tracks;

    [[nodiscard]] bool isValid() const
    {
        return index >= 0 && !tracks.empty();
    }
};

/*!
 * Describes an incremental playlist update.
 *
 * The operations are interpreted in this order:
 * 1. remove @c removedIndexes
 * 2. apply @c insertions
 * 3. apply @c moves
 * 4. refresh @c updatedIndexes
 *
 * If @c requiresReset is set, callers should ignore the incremental fields and
 * rebuild the playlist model from scratch instead.
 */
struct FYCORE_EXPORT PlaylistChangeset
{
    /** Playlist indexes to remove from the pre-change playlist. */
    std::vector<int> removedIndexes;
    /** Contiguous insertion groups to apply after removals. */
    std::vector<PlaylistTrackInsertion> insertions;
    /** Move operations to apply after insertions. */
    std::vector<PlaylistTrackMove> moves;
    /** Playlist indexes to refresh after structural changes are applied. */
    std::vector<int> updatedIndexes;
    /** Whether the change is too large or ambiguous for incremental application. */
    bool requiresReset{false};

    /** Returns @c true when there is no structural or metadata change to apply. */
    [[nodiscard]] bool isEmpty() const
    {
        return !requiresReset && removedIndexes.empty() && insertions.empty() && moves.empty()
            && updatedIndexes.empty();
    }
};

using TrackKeySet = std::unordered_set<QString>;

/*!
 * Builds a playlist patch that transforms @p oldTracks into @p newTracks.
 *
 * Tracks are matched by @c Track::uniqueFilepath(). If the track identity is
 * ambiguous, the function returns @c std::nullopt so callers can fall back to a
 * full reset.
 *
 * @param updatedTrackPaths Optional set of matched track paths that should be
 * treated as metadata updates even if the old and new track objects compare equal.
 */
[[nodiscard]] FYCORE_EXPORT std::optional<PlaylistChangeset>
buildPlaylistChangeset(const TrackList& oldTracks, const TrackList& newTracks,
                       const TrackKeySet& updatedTrackPaths = {});
} // namespace Fooyin

Q_DECLARE_METATYPE(Fooyin::PlaylistTrackMove)
Q_DECLARE_METATYPE(Fooyin::PlaylistTrackInsertion)
Q_DECLARE_METATYPE(Fooyin::PlaylistChangeset)
