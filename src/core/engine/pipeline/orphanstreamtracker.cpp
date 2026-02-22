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

#include "orphanstreamtracker.h"

#include "audiostream.h"

#include <algorithm>
#include <cstdint>
#include <limits>

namespace Fooyin {
bool OrphanStreamTracker::hasOrphan() const
{
    return m_streamId != InvalidStreamId;
}

StreamId OrphanStreamTracker::streamId() const
{
    return m_streamId;
}

StreamId OrphanStreamTracker::replace(const StreamId streamId)
{
    if(streamId == InvalidStreamId) {
        clear();
        return InvalidStreamId;
    }

    const StreamId displacedStreamId
        = (m_streamId != InvalidStreamId && m_streamId != streamId) ? m_streamId : InvalidStreamId;

    m_streamId = streamId;
    m_drainDeadline.reset();

    return displacedStreamId;
}

void OrphanStreamTracker::clear()
{
    m_streamId = InvalidStreamId;
    m_drainDeadline.reset();
}

void OrphanStreamTracker::armDrainDeadline(const uint64_t bufferedDurationMs, const uint64_t playbackDelayMs,
                                           const std::chrono::milliseconds minTimeout,
                                           const std::chrono::milliseconds maxTimeout)
{
    if(!hasOrphan()) {
        return;
    }

    uint64_t baseTimeoutMs = bufferedDurationMs;
    if(baseTimeoutMs > std::numeric_limits<uint64_t>::max() - playbackDelayMs) {
        baseTimeoutMs = std::numeric_limits<uint64_t>::max();
    }
    else {
        baseTimeoutMs += playbackDelayMs;
    }

    constexpr uint64_t CleanupGraceMs{1000};
    if(baseTimeoutMs > std::numeric_limits<uint64_t>::max() - CleanupGraceMs) {
        baseTimeoutMs = std::numeric_limits<uint64_t>::max();
    }
    else {
        baseTimeoutMs += CleanupGraceMs;
    }

    const uint64_t minTimeoutMs     = static_cast<uint64_t>(std::max<int64_t>(0, minTimeout.count()));
    const uint64_t maxTimeoutMs     = static_cast<uint64_t>(std::max<int64_t>(0, maxTimeout.count()));
    const uint64_t clampedTimeoutMs = std::clamp(baseTimeoutMs, minTimeoutMs, std::max(minTimeoutMs, maxTimeoutMs));

    m_drainDeadline = std::chrono::steady_clock::now() + std::chrono::milliseconds{clampedTimeoutMs};
}

OrphanStreamTracker::Evaluation OrphanStreamTracker::evaluate(const AudioStream* orphanStream) const
{
    Evaluation evaluation;

    if(!hasOrphan()) {
        return evaluation;
    }

    if(!orphanStream) {
        evaluation.shouldCleanup = true;
        return evaluation;
    }

    const bool orphanFinished = orphanStream->state() == AudioStream::State::Stopped
                             || (orphanStream->bufferEmpty() && orphanStream->fadeGain() < 0.01);
    const bool orphanTimedOut = m_drainDeadline.has_value() && std::chrono::steady_clock::now() >= *m_drainDeadline;

    evaluation.shouldCleanup = orphanFinished || orphanTimedOut;
    evaluation.timedOut      = orphanTimedOut && !orphanFinished;
    return evaluation;
}
} // namespace Fooyin
