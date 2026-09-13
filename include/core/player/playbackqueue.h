/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <luket@pm.me>
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

#include <core/playlist/playlist.h>

#include <map>
#include <optional>
#include <span>

namespace Fooyin {
class PlaybackQueuePrivate;

// Queue positions for each playlist
using PlaylistIndexes = std::map<UId, std::vector<int>>;
// Queue positions of a playlist track index
using PlaylistTrackIndexes = std::map<int, std::vector<int>>;
using QueueTracks          = std::vector<PlaylistTrack>;
using PlaybackQueueItemId  = uint64_t;

enum class PlaybackQueueMode : uint8_t
{
    PlaylistWithOverrides = 0,
    QueueAsPlaybackSource,
};

enum class PlayNowAction : uint8_t
{
    SelectedTracks = 0,
    ContainingGroup,
    AllTracks,
    QueueNext,
};

enum class PlaybackQueueItemOrigin : uint8_t
{
    Manual = 0,
    PlaylistGenerated,
};

struct FYCORE_EXPORT PlaybackQueueItem
{
    PlaybackQueueItemId id{0};
    PlaylistTrack track;
    PlaybackQueueItemOrigin origin{PlaybackQueueItemOrigin::Manual};
    int sourceOrder{-1};

    bool operator==(const PlaybackQueueItem& other) const = default;
};

using PlaybackQueueItems = std::vector<PlaybackQueueItem>;

struct FYCORE_EXPORT PlaybackQueueSnapshot
{
    PlaybackQueueItems items;
    int currentIndex{-1};
};

class FYCORE_EXPORT PlaybackQueue
{
public:
    PlaybackQueue();
    virtual ~PlaybackQueue();

    [[nodiscard]] bool empty() const;

    [[nodiscard]] const PlaybackQueueItems& items() const;
    [[nodiscard]] QueueTracks tracks() const;
    [[nodiscard]] PlaylistTrack track(int index) const;
    [[nodiscard]] const PlaybackQueueItem* item(int index) const;
    [[nodiscard]] const PlaybackQueueItem* item(PlaybackQueueItemId id) const;
    [[nodiscard]] int trackCount() const;

    [[nodiscard]] PlaybackQueueItemId currentItemId() const;
    [[nodiscard]] int currentIndex() const;
    [[nodiscard]] const PlaybackQueueItem* currentItem() const;
    [[nodiscard]] int indexOf(PlaybackQueueItemId id) const;

    bool setCurrentItem(PlaybackQueueItemId id);
    void clearCurrentItem();

    [[nodiscard]] PlaylistIndexes playlistIndexes() const;
    [[nodiscard]] PlaylistTrackIndexes indexesForPlaylist(const UId& id) const;
    [[nodiscard]] std::vector<int> indexesForTrack(const UId& playlistId, int playlistTrackIndex) const;

    [[nodiscard]] PlaylistTrack nextTrack() const;
    [[nodiscard]] const PlaybackQueueItem* nextItem() const;
    [[nodiscard]] const PlaybackQueueItem* relativeItem(int delta, Playlist::PlayModes mode) const;
    PlaylistTrack nextTrackChange();

    [[nodiscard]] int getTrackIndex(const PlaylistTrack& track) const;
    [[nodiscard]] bool containsTrack(const PlaylistTrack& track) const;

    std::vector<PlaybackQueueItemId> addTracks(const QueueTracks& tracks, int index = -1,
                                               PlaybackQueueItemOrigin origin   = PlaybackQueueItemOrigin::Manual,
                                               std::span<const int> sourceOrder = {});
    void replaceTracks(const QueueTracks& tracks);
    void replaceSequence(const QueueTracks& tracks, int currentIndex,
                         PlaybackQueueItemOrigin origin   = PlaybackQueueItemOrigin::PlaylistGenerated,
                         std::span<const int> sourceOrder = {});

    void restore(PlaybackQueueSnapshot snapshot);
    [[nodiscard]] PlaybackQueueSnapshot snapshot() const;
    bool updateTracks(const QueueTracks& tracks);

    std::optional<PlaybackQueueItem> removeItem(PlaybackQueueItemId id, bool allowCurrent = false);
    PlaybackQueueItems removeItems(std::span<const PlaybackQueueItemId> ids, bool allowCurrent = false);
    PlaybackQueueItems pruneHistory(int maxTracks);
    bool moveItems(std::span<const PlaybackQueueItemId> ids, int targetIndex);
    bool reorderItems(std::span<const PlaybackQueueItemId> ids);
    std::optional<PlaylistTrack> removeFirstMatchingTrack(const PlaylistTrack& track);
    QueueTracks removeTracks(const QueueTracks& tracks);
    QueueTracks removePlaylistTracks(const UId& playlistId);

    void clear();

private:
    std::unique_ptr<PlaybackQueuePrivate> p;
};
} // namespace Fooyin
