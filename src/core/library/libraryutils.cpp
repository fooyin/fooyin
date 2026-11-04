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

#include <core/library/libraryutils.h>

#include <QDir>

#include <unordered_map>

namespace Fooyin::Utils {
namespace {
struct TrackSegment
{
    QString filepath;
    int subsong{0};
    uint64_t offset{0};
    uint64_t duration{0};

    bool operator==(const TrackSegment& other) const = default;
};

struct TrackSegmentHash
{
    size_t operator()(const TrackSegment& segment) const
    {
        return qHashMulti(0, segment.filepath, segment.subsong, segment.offset, segment.duration);
    }
};

TrackSegment trackSegment(const Track& track)
{
    return {.filepath = track.uniqueFilepath(),
            .subsong  = track.subsong(),
            .offset   = track.offset(),
            .duration = track.duration()};
}
} // namespace

std::vector<int> updateCommonTracks(TrackList& tracks, const TrackList& updatedTracks, CommonOperation operation)
{
    if(tracks.empty() || updatedTracks.empty()) {
        return {};
    }

    std::vector<int> indexes;

    // Preserve the original source when a caller updates a list from itself
    const TrackList copiedUpdates = &tracks == &updatedTracks ? updatedTracks : TrackList{};
    const TrackList& updates      = &tracks == &updatedTracks ? copiedUpdates : updatedTracks;

    std::unordered_map<int, size_t> idIndexes;
    idIndexes.reserve(updates.size());
    std::unordered_map<TrackSegment, size_t, TrackSegmentHash> segmentIndexes;
    segmentIndexes.reserve(updates.size());
    std::unordered_map<TrackSegment, size_t, TrackSegmentHash> unmanagedSegmentIndexes;

    for(size_t i{0}; i < updates.size(); ++i) {
        const auto& track = updates.at(i);
        if(!track.isValid()) {
            continue;
        }

        const auto segment = trackSegment(track);
        segmentIndexes.emplace(segment, i);
        if(track.id() >= 0) {
            idIndexes.emplace(track.id(), i);
        }
        else {
            unmanagedSegmentIndexes.emplace(segment, i);
        }
    }

    size_t retained{0};
    for(size_t i{0}; i < tracks.size(); ++i) {
        auto& track         = tracks[i];
        size_t updatedIndex = updates.size();
        if(track.isValid()) {
            if(track.id() >= 0) {
                if(const auto it = idIndexes.find(track.id()); it != idIndexes.end()) {
                    updatedIndex = it->second;
                }
                if(!unmanagedSegmentIndexes.empty()) {
                    if(const auto it = unmanagedSegmentIndexes.find(trackSegment(track));
                       it != unmanagedSegmentIndexes.end()) {
                        updatedIndex = std::min(updatedIndex, it->second);
                    }
                }
            }
            else if(const auto it = segmentIndexes.find(trackSegment(track)); it != segmentIndexes.end()) {
                updatedIndex = it->second;
            }
        }

        if(updatedIndex < updates.size()) {
            if(operation == CommonOperation::UpdateChanged && track.sameDataAs(updates.at(updatedIndex))) {
                continue;
            }
            indexes.push_back(static_cast<int>(i));
            if(operation != CommonOperation::Remove) {
                track = updates.at(updatedIndex);
            }
        }
        if(operation == CommonOperation::Remove && updatedIndex == updates.size()) {
            if(retained != i) {
                tracks[retained] = track;
            }
            ++retained;
        }
    }

    if(operation == CommonOperation::Remove) {
        tracks.resize(retained);
    }
    return indexes;
}

std::optional<QString> physicalSourceKey(const Track& track)
{
    if(!track.isValid() || track.isRemote()) {
        return {};
    }

    const QString filepath = track.isInArchive() ? track.archivePath() : track.filepath();
    if(filepath.isEmpty()) {
        return {};
    }

    const QString absolutePath = QDir::isAbsolutePath(filepath) ? filepath : QDir::current().absoluteFilePath(filepath);
    return QDir::cleanPath(absolutePath);
}
} // namespace Fooyin::Utils
