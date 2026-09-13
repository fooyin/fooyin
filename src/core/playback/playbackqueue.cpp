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

#include <core/player/playbackqueue.h>

#include <core/track.h>

#include <iterator>
#include <numeric>
#include <ranges>
#include <set>
#include <unordered_map>

namespace Fooyin {
class PlaybackQueuePrivate
{
public:
    explicit PlaybackQueuePrivate(PlaybackQueue* self)
        : m_self{self}
    { }

    PlaybackQueueItem makeItem(const PlaylistTrack& track, PlaybackQueueItemOrigin origin, int sourceOrder)
    {
        PlaybackQueueItemId id = m_nextItemId++;

        if(id == 0) {
            id = m_nextItemId++;
        }
        if(m_nextItemId == 0) {
            m_nextItemId = 1;
        }

        return {.id = id, .track = track, .origin = origin, .sourceOrder = sourceOrder};
    }

    void restoreCurrentPosition(PlaybackQueueItemId currentId, int previousIndex)
    {
        if(currentId != 0 && m_self->setCurrentItem(currentId)) {
            return;
        }

        m_currentItemId = 0;
        m_cursorIndex   = std::clamp(previousIndex, -1, m_self->trackCount() - 1);
    }

    PlaybackQueue* m_self;

    PlaybackQueueItems m_items;
    PlaybackQueueItemId m_currentItemId{0};
    int m_cursorIndex{-1};
    PlaybackQueueItemId m_nextItemId{1};
};

PlaybackQueue::PlaybackQueue()
    : p{std::make_unique<PlaybackQueuePrivate>(this)}
{ }

PlaybackQueue::~PlaybackQueue() = default;

bool PlaybackQueue::empty() const
{
    return p->m_items.empty();
}

const PlaybackQueueItems& PlaybackQueue::items() const
{
    return p->m_items;
}

QueueTracks PlaybackQueue::tracks() const
{
    QueueTracks tracks;
    tracks.reserve(p->m_items.size());
    std::ranges::transform(p->m_items, std::back_inserter(tracks), &PlaybackQueueItem::track);
    return tracks;
}

PlaylistTrack PlaybackQueue::track(int index) const
{
    if(const auto* queueItem = item(index)) {
        return queueItem->track;
    }
    return {};
}

const PlaybackQueueItem* PlaybackQueue::item(int index) const
{
    if(p->m_items.empty() || index < 0 || index >= trackCount()) {
        return {};
    }

    return &p->m_items.at(index);
}

const PlaybackQueueItem* PlaybackQueue::item(PlaybackQueueItemId id) const
{
    const int index = indexOf(id);
    return index >= 0 ? &p->m_items.at(index) : nullptr;
}

int PlaybackQueue::trackCount() const
{
    return static_cast<int>(p->m_items.size());
}

PlaybackQueueItemId PlaybackQueue::currentItemId() const
{
    return p->m_currentItemId;
}

int PlaybackQueue::currentIndex() const
{
    if(p->m_currentItemId != 0) {
        return indexOf(p->m_currentItemId);
    }
    return p->m_cursorIndex;
}

const PlaybackQueueItem* PlaybackQueue::currentItem() const
{
    return item(p->m_currentItemId);
}

int PlaybackQueue::indexOf(PlaybackQueueItemId id) const
{
    if(id == 0) {
        return -1;
    }

    const auto it = std::ranges::find(p->m_items, id, &PlaybackQueueItem::id);
    return it != p->m_items.cend() ? static_cast<int>(std::distance(p->m_items.begin(), it)) : -1;
}

bool PlaybackQueue::setCurrentItem(PlaybackQueueItemId id)
{
    const int index = indexOf(id);
    if(index < 0) {
        return false;
    }

    p->m_currentItemId = id;
    p->m_cursorIndex   = index;
    return true;
}

void PlaybackQueue::clearCurrentItem()
{
    p->m_currentItemId = 0;
    p->m_cursorIndex   = -1;
}

PlaylistIndexes PlaybackQueue::playlistIndexes() const
{
    PlaylistIndexes indexes;

    for(const auto& item : p->m_items) {
        if(item.origin != PlaybackQueueItemOrigin::Manual) {
            continue;
        }
        const auto& track = item.track;
        indexes[track.playlistId].emplace_back(track.indexInPlaylist);
    }

    return indexes;
}

PlaylistTrackIndexes PlaybackQueue::indexesForPlaylist(const UId& id) const
{
    PlaylistTrackIndexes indexes;

    for(auto queueIndex{0}; const auto& item : p->m_items) {
        if(item.origin != PlaybackQueueItemOrigin::Manual) {
            continue;
        }
        const auto& track = item.track;
        if(track.playlistId == id) {
            indexes[track.indexInPlaylist].emplace_back(queueIndex);
        }
        ++queueIndex;
    }

    return indexes;
}

std::vector<int> PlaybackQueue::indexesForTrack(const UId& playlistId, const int playlistTrackIndex) const
{
    std::vector<int> indexes;

    for(auto queueIndex{0}; const auto& item : p->m_items) {
        if(item.origin != PlaybackQueueItemOrigin::Manual) {
            continue;
        }
        const auto& track = item.track;
        if(track.playlistId == playlistId && track.indexInPlaylist == playlistTrackIndex) {
            indexes.emplace_back(queueIndex);
        }
        ++queueIndex;
    }

    return indexes;
}

PlaylistTrack PlaybackQueue::nextTrack() const
{
    const auto* queueItem = nextItem();
    return queueItem ? queueItem->track : PlaylistTrack{};
}

const PlaybackQueueItem* PlaybackQueue::nextItem() const
{
    if(p->m_items.empty()) {
        return nullptr;
    }

    const int nextIndex = currentIndex() + 1;
    return item(nextIndex);
}

const PlaybackQueueItem* PlaybackQueue::relativeItem(int delta, Playlist::PlayModes mode) const
{
    if(p->m_items.empty()) {
        return nullptr;
    }

    int target{currentIndex()};
    if(mode & Playlist::RepeatTrack) {
        return item(target);
    }

    target += delta;
    if(mode & Playlist::RepeatPlaylist) {
        const int count = trackCount();
        target %= count;
        if(target < 0) {
            target += count;
        }
    }

    return item(target);
}

PlaylistTrack PlaybackQueue::nextTrackChange()
{
    if(p->m_items.empty()) {
        return {};
    }

    const auto track = p->m_items.front().track;
    removeItem(p->m_items.front().id);
    return track;
}

int PlaybackQueue::getTrackIndex(const PlaylistTrack& track) const
{
    const auto it = std::ranges::find_if(
        p->m_items, [&track](const PlaybackQueueItem& other) { return track.sameIdentityAs(other.track); });

    if(it == p->m_items.cend()) {
        return -1;
    }

    return static_cast<int>(std::distance(p->m_items.begin(), it));
}

bool PlaybackQueue::containsTrack(const PlaylistTrack& track) const
{
    return getTrackIndex(track) >= 0;
}

std::vector<PlaybackQueueItemId> PlaybackQueue::addTracks(const QueueTracks& tracks, int index,
                                                          PlaybackQueueItemOrigin origin,
                                                          std::span<const int> sourceOrder)
{
    const auto currentId{p->m_currentItemId};
    const int previousIndex{currentIndex()};

    PlaybackQueueItems items;
    items.reserve(tracks.size());
    std::vector<PlaybackQueueItemId> ids;
    ids.reserve(tracks.size());

    for(size_t i{0}; i < tracks.size(); ++i) {
        const int order = std::cmp_less(i, sourceOrder.size()) ? sourceOrder[i] : -1;
        auto item       = p->makeItem(tracks[i], origin, order);
        ids.push_back(item.id);
        items.push_back(std::move(item));
    }

    if(index >= 0 && index <= trackCount()) {
        p->m_items.insert(p->m_items.begin() + index, std::make_move_iterator(items.begin()),
                          std::make_move_iterator(items.end()));
    }
    else {
        p->m_items.insert(p->m_items.end(), std::make_move_iterator(items.begin()),
                          std::make_move_iterator(items.end()));
    }

    p->restoreCurrentPosition(currentId, previousIndex);
    return ids;
}

void PlaybackQueue::replaceTracks(const QueueTracks& tracks)
{
    p->m_items.clear();
    p->m_currentItemId = 0;
    p->m_cursorIndex   = -1;

    addTracks(tracks);
}

void PlaybackQueue::replaceSequence(const QueueTracks& tracks, int currentIndex, PlaybackQueueItemOrigin origin,
                                    std::span<const int> sourceOrder)
{
    p->m_items.clear();
    p->m_currentItemId = 0;
    p->m_cursorIndex   = -1;

    std::vector<int> defaultSourceOrder;
    if(sourceOrder.empty()) {
        defaultSourceOrder.resize(tracks.size());
        std::iota(defaultSourceOrder.begin(), defaultSourceOrder.end(), 0);
        sourceOrder = defaultSourceOrder;
    }

    const auto ids = addTracks(tracks, -1, origin, sourceOrder);
    if(currentIndex >= 0 && std::cmp_less(currentIndex, ids.size())) {
        setCurrentItem(ids.at(currentIndex));
    }
}

void PlaybackQueue::restore(PlaybackQueueSnapshot snapshot)
{
    p->m_items         = std::move(snapshot.items);
    p->m_currentItemId = 0;
    p->m_cursorIndex   = -1;

    PlaybackQueueItemId maxId{0};
    for(const auto& item : p->m_items) {
        maxId = std::max(maxId, item.id);
    }

    std::set<PlaybackQueueItemId> usedIds;
    for(auto& item : p->m_items) {
        if(item.id == 0 || usedIds.contains(item.id)) {
            ++maxId;
            while(maxId == 0 || usedIds.contains(maxId)) {
                ++maxId;
            }
            item.id = maxId;
        }
        usedIds.insert(item.id);
    }

    p->m_nextItemId = maxId + 1;

    if(p->m_nextItemId == 0) {
        p->m_nextItemId = 1;
    }

    if(snapshot.currentIndex >= 0 && std::cmp_less(snapshot.currentIndex, p->m_items.size())) {
        setCurrentItem(p->m_items.at(snapshot.currentIndex).id);
    }
}

PlaybackQueueSnapshot PlaybackQueue::snapshot() const
{
    return {.items = p->m_items, .currentIndex = currentIndex()};
}

bool PlaybackQueue::updateTracks(const QueueTracks& tracks)
{
    if(tracks.size() != p->m_items.size()) {
        return false;
    }

    bool changed{false};
    for(size_t i{0}; i < tracks.size(); ++i) {
        if(p->m_items[i].track != tracks[i]) {
            p->m_items[i].track = tracks[i];
            changed             = true;
        }
    }

    return changed;
}

std::optional<PlaybackQueueItem> PlaybackQueue::removeItem(PlaybackQueueItemId id, bool allowCurrent)
{
    const int index = indexOf(id);
    if(index < 0 || (!allowCurrent && id == p->m_currentItemId)) {
        return {};
    }

    const auto currentId{p->m_currentItemId};
    const int previousIndex{currentIndex()};

    PlaybackQueueItem removed = std::move(p->m_items.at(index));
    p->m_items.erase(p->m_items.begin() + index);

    if(id == currentId) {
        p->m_currentItemId = 0;
        p->m_cursorIndex   = std::clamp(index - 1, -1, trackCount() - 1);
    }
    else {
        p->restoreCurrentPosition(currentId, previousIndex);
    }

    return removed;
}

PlaybackQueueItems PlaybackQueue::removeItems(std::span<const PlaybackQueueItemId> ids, bool allowCurrent)
{
    PlaybackQueueItems removed;

    for(const PlaybackQueueItemId id : ids) {
        if(auto item = removeItem(id, allowCurrent)) {
            removed.push_back(std::move(*item));
        }
    }

    return removed;
}

PlaybackQueueItems PlaybackQueue::pruneHistory(int maxTracks)
{
    const int removeCount = maxTracks >= 0 ? currentIndex() - maxTracks : 0;
    if(removeCount <= 0) {
        return {};
    }

    PlaybackQueueItems removed;
    removed.reserve(removeCount);

    std::move(p->m_items.begin(), p->m_items.begin() + removeCount, std::back_inserter(removed));
    p->m_items.erase(p->m_items.begin(), p->m_items.begin() + removeCount);

    setCurrentItem(p->m_currentItemId);
    return removed;
}

bool PlaybackQueue::moveItems(std::span<const PlaybackQueueItemId> ids, int targetIndex)
{
    if(ids.empty() || p->m_items.empty()) {
        return false;
    }

    const auto currentId{p->m_currentItemId};
    const int previousIndex{currentIndex()};

    std::vector<int> indexes;
    indexes.reserve(ids.size());
    for(const auto id : ids) {
        if(const int index = indexOf(id); index >= 0) {
            indexes.push_back(index);
        }
    }
    std::ranges::sort(indexes);
    indexes.erase(std::ranges::unique(indexes).begin(), indexes.end());
    if(indexes.empty()) {
        return false;
    }

    PlaybackQueueItems moved;
    moved.reserve(indexes.size());
    for(const int index : indexes) {
        moved.push_back(p->m_items.at(index));
    }

    targetIndex = std::clamp(targetIndex, 0, trackCount());
    targetIndex
        -= static_cast<int>(std::ranges::count_if(indexes, [targetIndex](int index) { return index < targetIndex; }));

    for(const int index : indexes | std::views::reverse) {
        p->m_items.erase(p->m_items.begin() + index);
    }

    p->m_items.insert(p->m_items.begin() + std::clamp(targetIndex, 0, trackCount()),
                      std::make_move_iterator(moved.begin()), std::make_move_iterator(moved.end()));

    p->restoreCurrentPosition(currentId, previousIndex);
    return true;
}

bool PlaybackQueue::reorderItems(std::span<const PlaybackQueueItemId> ids)
{
    if(ids.size() != p->m_items.size()) {
        return false;
    }

    PlaybackQueueItems reordered;
    reordered.reserve(ids.size());

    std::unordered_map<PlaybackQueueItemId, size_t> itemIndexes;
    itemIndexes.reserve(p->m_items.size());
    for(size_t i{0}; i < p->m_items.size(); ++i) {
        if(!itemIndexes.emplace(p->m_items[i].id, i).second) {
            return false;
        }
    }

    std::vector used(p->m_items.size(), false);
    for(const auto id : ids) {
        const auto itemIt = itemIndexes.find(id);
        if(itemIt == itemIndexes.cend() || used[itemIt->second]) {
            return false;
        }
        used[itemIt->second] = true;
        reordered.push_back(p->m_items[itemIt->second]);
    }

    const auto currentId{p->m_currentItemId};
    const int previousIndex = currentIndex();
    p->m_items              = std::move(reordered);
    p->restoreCurrentPosition(currentId, previousIndex);
    return true;
}

std::optional<PlaylistTrack> PlaybackQueue::removeFirstMatchingTrack(const PlaylistTrack& track)
{
    const auto index = getTrackIndex(track);
    if(index == -1) {
        return {};
    }

    auto removed = removeItem(p->m_items.at(index).id);
    return removed ? std::optional<PlaylistTrack>{std::move(removed->track)} : std::nullopt;
}

QueueTracks PlaybackQueue::removeTracks(const QueueTracks& tracks)
{
    QueueTracks removedTracks;

    std::set<PlaylistTrack> tracksToRemove{tracks.cbegin(), tracks.cend()};

    auto matchingTrack = [&tracksToRemove](const PlaylistTrack& track) {
        return std::ranges::find_if(tracksToRemove,
                                    [&track](const PlaylistTrack& other) { return track.sameIdentityAs(other); })
            != tracksToRemove.cend();
    };

    std::vector<PlaybackQueueItemId> ids;
    for(const auto& item : p->m_items) {
        if(item.origin == PlaybackQueueItemOrigin::Manual && matchingTrack(item.track)) {
            ids.push_back(item.id);
        }
    }

    for(auto& item : removeItems(ids)) {
        removedTracks.push_back(std::move(item.track));
    }

    return removedTracks;
}

QueueTracks PlaybackQueue::removePlaylistTracks(const UId& playlistId)
{
    QueueTracks removedTracks;

    std::vector<PlaybackQueueItemId> ids;
    for(const auto& item : p->m_items) {
        if(item.origin == PlaybackQueueItemOrigin::Manual && item.track.playlistId == playlistId) {
            ids.push_back(item.id);
        }
    }

    for(auto& item : removeItems(ids)) {
        removedTracks.push_back(std::move(item.track));
    }

    return removedTracks;
}

void PlaybackQueue::clear()
{
    p->m_items.clear();
    p->m_currentItemId = 0;
    p->m_cursorIndex   = -1;
}
} // namespace Fooyin
