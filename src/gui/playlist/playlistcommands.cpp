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

#include "playlistcommands.h"

#include <ranges>
#include <set>

namespace {
Fooyin::PlaylistTrackList playlistTracksFor(Fooyin::PlaylistHandler* handler, const Fooyin::UId& playlistId)
{
    if(!handler) {
        return {};
    }

    if(auto* playlist = handler->playlistById(playlistId)) {
        return playlist->playlistTracks();
    }

    return {};
}

Fooyin::PlaylistTrackList applyInsertions(Fooyin::PlaylistTrackList tracks, const Fooyin::TrackGroups& groups)
{
    int insertedTrackCount{0};

    for(const auto& [index, insertedTracks] : groups) {
        const int insertionIndex = index < 0
                                     ? static_cast<int>(tracks.size())
                                     : std::clamp(index + insertedTrackCount, 0, static_cast<int>(tracks.size()));
        tracks.insert(tracks.begin() + insertionIndex, insertedTracks.cbegin(), insertedTracks.cend());
        insertedTrackCount += static_cast<int>(insertedTracks.size());
    }

    return Fooyin::PlaylistTrack::updateIndexes(tracks);
}

Fooyin::PlaylistTrackList applyRemovals(Fooyin::PlaylistTrackList tracks, const std::vector<int>& indexes)
{
    std::set<int> indexesToRemove{indexes.cbegin(), indexes.cend()};
    for(const int index : indexesToRemove | std::views::reverse) {
        if(index >= 0 && std::cmp_less(index, tracks.size())) {
            tracks.erase(tracks.begin() + index);
        }
    }

    return Fooyin::PlaylistTrack::updateIndexes(tracks);
}

std::vector<int> flattenIndexes(const Fooyin::TrackIndexRangeList& ranges)
{
    std::vector<int> indexes;

    for(const auto& range : ranges) {
        for(int index{range.first}; index <= range.last; ++index) {
            indexes.push_back(index);
        }
    }

    std::ranges::sort(indexes);
    indexes.erase(std::ranges::unique(indexes).begin(), indexes.end());

    return indexes;
}

Fooyin::PlaylistTrackList applyMove(Fooyin::PlaylistTrackList tracks, const Fooyin::MoveOperationGroup& operation)
{
    const std::vector<int> indexesToMove = flattenIndexes(operation.tracksToMove);
    if(indexesToMove.empty()) {
        return tracks;
    }

    Fooyin::PlaylistTrackList movedTracks;
    movedTracks.reserve(indexesToMove.size());

    for(const int index : indexesToMove) {
        if(index >= 0 && std::cmp_less(index, tracks.size())) {
            movedTracks.push_back(tracks.at(index));
        }
    }

    if(movedTracks.empty()) {
        return tracks;
    }

    int targetIndex = operation.index < 0 ? static_cast<int>(tracks.size()) : operation.index;
    targetIndex -= static_cast<int>(
        std::ranges::count_if(indexesToMove, [targetIndex](int index) { return index < targetIndex; }));

    for(const int index : indexesToMove | std::views::reverse) {
        if(index >= 0 && std::cmp_less(index, tracks.size())) {
            tracks.erase(tracks.begin() + index);
        }
    }

    targetIndex = std::clamp(targetIndex, 0, static_cast<int>(tracks.size()));
    tracks.insert(tracks.begin() + targetIndex, movedTracks.cbegin(), movedTracks.cend());

    return Fooyin::PlaylistTrack::updateIndexes(tracks);
}

Fooyin::PlaylistTrackList applyMoves(Fooyin::PlaylistTrackList tracks, const Fooyin::MoveOperation& operation)
{
    for(const auto& group : operation) {
        tracks = applyMove(std::move(tracks), group);
    }

    return Fooyin::PlaylistTrack::updateIndexes(tracks);
}
} // namespace

namespace Fooyin {
PlaylistCommand::PlaylistCommand(PlaylistHandler* handler, const UId& playlistId)
    : QUndoCommand{nullptr}
    , m_handler{handler}
    , m_playlistId{playlistId}
{ }

InsertTracks::InsertTracks(PlaylistHandler* handler, const UId& playlistId, TrackGroups groups)
    : PlaylistCommand{handler, playlistId}
    , m_oldTracks{playlistTracksFor(handler, playlistId)}
    , m_newTracks{applyInsertions(m_oldTracks, groups)}
{ }

void InsertTracks::undo()
{
    if(m_handler) {
        m_handler->replacePlaylistTracks(m_playlistId, m_oldTracks, PlaylistTrackChangeSource::History);
    }
}

void InsertTracks::redo()
{
    if(m_handler) {
        m_handler->replacePlaylistTracks(m_playlistId, m_newTracks, PlaylistTrackChangeSource::History);
    }
}

RemoveTracks::RemoveTracks(PlaylistHandler* handler, const UId& playlistId, std::vector<int> indexes)
    : PlaylistCommand{handler, playlistId}
    , m_oldTracks{playlistTracksFor(handler, playlistId)}
    , m_newTracks{applyRemovals(m_oldTracks, indexes)}
{ }

void RemoveTracks::undo()
{
    if(m_handler) {
        m_handler->replacePlaylistTracks(m_playlistId, m_oldTracks, PlaylistTrackChangeSource::History);
    }
}

void RemoveTracks::redo()
{
    if(m_handler) {
        m_handler->replacePlaylistTracks(m_playlistId, m_newTracks, PlaylistTrackChangeSource::History);
    }
}

MoveTracks::MoveTracks(PlaylistHandler* handler, const UId& playlistId, MoveOperation operation)
    : PlaylistCommand{handler, playlistId}
    , m_oldTracks{playlistTracksFor(handler, playlistId)}
    , m_newTracks{applyMoves(m_oldTracks, operation)}
{ }

void MoveTracks::undo()
{
    if(m_handler) {
        m_handler->replacePlaylistTracks(m_playlistId, m_oldTracks, PlaylistTrackChangeSource::History);
    }
}

void MoveTracks::redo()
{
    if(m_handler) {
        m_handler->replacePlaylistTracks(m_playlistId, m_newTracks, PlaylistTrackChangeSource::History);
    }
}

ResetTracks::ResetTracks(PlaylistHandler* handler, const UId& playlistId, PlaylistTrackList oldTracks,
                         const PlaylistTrackList& newTracks)
    : PlaylistCommand{handler, playlistId}
    , m_oldTracks{PlaylistTrack::updateIndexes(oldTracks)}
    , m_newTracks{PlaylistTrack::updateIndexes(newTracks)}
{ }

void ResetTracks::undo()
{
    if(m_handler) {
        m_handler->replacePlaylistTracks(m_playlistId, m_oldTracks, PlaylistTrackChangeSource::History);
    }
}

void ResetTracks::redo()
{
    if(m_handler) {
        m_handler->replacePlaylistTracks(m_playlistId, m_newTracks, PlaylistTrackChangeSource::History);
    }
}
} // namespace Fooyin
