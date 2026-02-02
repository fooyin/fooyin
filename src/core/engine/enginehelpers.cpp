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

#include "enginehelpers.h"

namespace Fooyin {
bool sameTrackIdentity(const Track& lhs, const Track& rhs)
{
    if(!lhs.isValid() || !rhs.isValid()) {
        return false;
    }

    if(lhs.id() >= 0 && rhs.id() >= 0) {
        return lhs.id() == rhs.id();
    }

    return lhs.uniqueFilepath() == rhs.uniqueFilepath() && lhs.subsong() == rhs.subsong()
        && lhs.offset() == rhs.offset() && lhs.duration() == rhs.duration();
}

bool sameTrackSegment(const Track& lhs, const Track& rhs)
{
    if(!lhs.isValid() || !rhs.isValid()) {
        return false;
    }

    return lhs.filepath() == rhs.filepath() && lhs.subsong() == rhs.subsong() && lhs.offset() == rhs.offset()
        && lhs.duration() == rhs.duration();
}

bool isContiguousSameFileSegment(const Track& currentTrack, const Track& nextTrack)
{
    if(!currentTrack.isValid() || !nextTrack.isValid()) {
        return false;
    }

    return nextTrack.filepath() == currentTrack.filepath() && nextTrack.subsong() == currentTrack.subsong()
        && nextTrack.offset() == currentTrack.offset() + currentTrack.duration();
}

bool isMultiTrackFileTransition(const Track& currentTrack, const Track& nextTrack)
{
    if(!currentTrack.isValid() || !nextTrack.isValid()) {
        return false;
    }

    if(nextTrack.filepath() != currentTrack.filepath()) {
        return false;
    }

    // Same physical file, but different logical segment metadata.
    return !sameTrackSegment(currentTrack, nextTrack);
}

uint64_t relativeTrackPositionMs(uint64_t absolutePosMs, uint64_t offsetMs)
{
    return absolutePosMs >= offsetMs ? absolutePosMs - offsetMs : 0;
}

uint64_t absoluteTrackPositionMs(uint64_t streamPosMs, uint64_t streamOriginMs)
{
    if(streamPosMs > std::numeric_limits<uint64_t>::max() - streamOriginMs) {
        return std::numeric_limits<uint64_t>::max();
    }

    return streamOriginMs + streamPosMs;
}

uint64_t relativeTrackPositionMs(uint64_t streamPosMs, uint64_t streamOriginMs, uint64_t trackOffsetMs)
{
    return relativeTrackPositionMs(absoluteTrackPositionMs(streamPosMs, streamOriginMs), trackOffsetMs);
}

bool shouldEmitTrackEndOnce(Track& lastEndedTrack, const Track& currentTrack)
{
    if(sameTrackSegment(lastEndedTrack, currentTrack)) {
        return false;
    }

    lastEndedTrack = currentTrack;
    return true;
}
} // namespace Fooyin
