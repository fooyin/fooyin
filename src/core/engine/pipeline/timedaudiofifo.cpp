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

#include "timedaudiofifo.h"

#include <algorithm>

constexpr int ThresholdFrames = 16384;

namespace Fooyin {
TimedAudioFifo::TimedAudioFifo()
    : m_writeOffsetFrames{0}
    , m_bufferBaseOffsetFrames{0}
    , m_timelineBaseOffsetFrames{0}
    , m_streamId{InvalidStreamId}
    , m_epoch{0}
{ }

bool TimedAudioFifo::hasData() const
{
    const int physicalOffset = m_bufferBaseOffsetFrames + m_writeOffsetFrames;
    return m_buffer.isValid() && physicalOffset < m_buffer.frameCount();
}

int TimedAudioFifo::queuedFrames() const
{
    if(!hasData()) {
        return 0;
    }

    const int physicalOffset = m_bufferBaseOffsetFrames + m_writeOffsetFrames;
    return std::max(0, m_buffer.frameCount() - physicalOffset);
}

int TimedAudioFifo::writeOffsetFrames() const
{
    return m_writeOffsetFrames;
}

StreamId TimedAudioFifo::streamId() const
{
    return m_streamId;
}

uint64_t TimedAudioFifo::epoch() const
{
    return m_epoch;
}

const std::vector<TimedAudioFifo::TimelineChunk>& TimedAudioFifo::timeline() const
{
    return m_timeline;
}

void TimedAudioFifo::clear()
{
    m_buffer = {};
    m_timeline.clear();
    m_writeOffsetFrames        = 0;
    m_bufferBaseOffsetFrames   = 0;
    m_timelineBaseOffsetFrames = 0;
    m_streamId                 = InvalidStreamId;
    m_epoch                    = 0;
}

void TimedAudioFifo::compact()
{
    if(!hasData()) {
        clear();
        return;
    }

    const int consumedFrames = std::max(0, m_writeOffsetFrames);
    if(consumedFrames <= 0 && m_timelineBaseOffsetFrames <= 0 && m_bufferBaseOffsetFrames <= 0) {
        return;
    }

    m_bufferBaseOffsetFrames += consumedFrames;
    const int remainingFrames = std::max(0, m_buffer.frameCount() - m_bufferBaseOffsetFrames);
    if(remainingFrames <= 0) {
        clear();
        return;
    }

    discardTimelinePrefix(m_timeline, m_timelineBaseOffsetFrames + consumedFrames);

    m_writeOffsetFrames        = 0;
    m_timelineBaseOffsetFrames = 0;

    if(m_bufferBaseOffsetFrames >= ThresholdFrames || m_bufferBaseOffsetFrames >= remainingFrames) {
        m_buffer                 = sliceBufferFrames(m_buffer, m_bufferBaseOffsetFrames, remainingFrames);
        m_bufferBaseOffsetFrames = 0;
    }
}

void TimedAudioFifo::dropFrontFrames(int frames)
{
    if(!hasData() || frames <= 0) {
        return;
    }

    const int maxLogicalFrames = std::max(0, m_buffer.frameCount() - m_bufferBaseOffsetFrames);
    m_writeOffsetFrames        = std::min(maxLogicalFrames, m_writeOffsetFrames + std::max(0, frames));
    compact();
}

void TimedAudioFifo::set(const AudioBuffer& buffer, std::span<const TimelineChunk> timelineChunks, StreamId streamId,
                         uint64_t epoch)
{
    clear();

    if(!buffer.isValid() || buffer.frameCount() <= 0) {
        return;
    }

    m_buffer                 = buffer;
    m_streamId               = streamId;
    m_epoch                  = epoch;
    m_writeOffsetFrames      = 0;
    m_bufferBaseOffsetFrames = 0;

    appendTimeline(timelineChunks, buffer.frameCount());
}

TimedAudioFifo::AppendResult TimedAudioFifo::append(const AudioBuffer& buffer,
                                                    std::span<const TimelineChunk> timelineChunks,
                                                    const StreamId streamId, uint64_t epoch)
{
    if(!buffer.isValid() || buffer.frameCount() <= 0) {
        return {};
    }

    if(!hasData()) {
        set(buffer, timelineChunks, streamId, epoch);
        return {.appended = true, .formatMismatch = false, .appendedFrames = buffer.frameCount()};
    }

    if(m_buffer.format() != buffer.format()) {
        return {.appended = false, .formatMismatch = true, .appendedFrames = 0};
    }

    m_buffer.append(buffer.constData());
    m_epoch                    = epoch;
    m_timelineBaseOffsetFrames = 0;
    appendTimeline(timelineChunks, buffer.frameCount());
    return {.appended = true, .formatMismatch = false, .appendedFrames = buffer.frameCount()};
}

TimedAudioFifo::AppendResult TimedAudioFifo::appendOrReplace(const AudioBuffer& buffer,
                                                             std::span<const TimelineChunk> timelineChunks,
                                                             const StreamId streamId, uint64_t epoch)
{
    const auto appendResult = append(buffer, timelineChunks, streamId, epoch);
    if(!appendResult.formatMismatch) {
        return appendResult;
    }

    clear();
    set(buffer, timelineChunks, streamId, epoch);

    return {.appended = true, .formatMismatch = true, .appendedFrames = buffer.frameCount()};
}

AudioBuffer TimedAudioFifo::sliceBufferFrames(const AudioBuffer& buffer, int frameOffset, int frameCount)
{
    if(!buffer.isValid() || frameOffset < 0 || frameCount <= 0) {
        return {};
    }

    const auto format        = buffer.format();
    const int bytesPerFrame  = format.bytesPerFrame();
    const int totalFrames    = buffer.frameCount();
    const int clampedOffset  = std::min(frameOffset, totalFrames);
    const int available      = std::max(0, totalFrames - clampedOffset);
    const int clampedFrames  = std::min(frameCount, available);
    const int startByte      = clampedOffset * bytesPerFrame;
    const int subspanBytes   = clampedFrames * bytesPerFrame;
    const uint64_t startTime = buffer.startTime() + format.durationForFrames(clampedOffset);

    if(bytesPerFrame <= 0 || subspanBytes <= 0) {
        return {};
    }

    const auto sourceData = buffer.constData();
    if(startByte < 0 || (static_cast<size_t>(startByte) + subspanBytes) > sourceData.size()) {
        return {};
    }

    return {sourceData.subspan(static_cast<size_t>(startByte), static_cast<size_t>(subspanBytes)), format, startTime};
}

void TimedAudioFifo::discardTimelinePrefix(std::vector<TimelineChunk>& timelineChunks, int frames)
{
    frames = std::max(0, frames);
    if(frames <= 0 || timelineChunks.empty()) {
        return;
    }

    size_t eraseCount{0};
    while(frames > 0 && eraseCount < timelineChunks.size()) {
        auto& chunk = timelineChunks[eraseCount];

        if(chunk.frames <= 0) {
            ++eraseCount;
            continue;
        }

        const int take = std::min(chunk.frames, frames);
        if(take <= 0) {
            ++eraseCount;
            continue;
        }

        if(chunk.valid && chunk.frameDurationNs > 0) {
            chunk.startNs += chunk.frameDurationNs * static_cast<uint64_t>(take);
        }

        chunk.frames -= take;
        frames -= take;

        if(chunk.frames <= 0) {
            ++eraseCount;
        }
    }

    if(eraseCount > 0) {
        timelineChunks.erase(timelineChunks.begin(), timelineChunks.begin() + static_cast<ptrdiff_t>(eraseCount));
    }
}

bool TimedAudioFifo::canMergeTimelineChunks(const TimelineChunk& prev, const TimelineChunk& next)
{
    if(prev.frames <= 0 || next.frames <= 0 || prev.valid != next.valid || prev.streamId != next.streamId
       || prev.epoch != next.epoch || prev.sampleRate != next.sampleRate) {
        return false;
    }

    if(!prev.valid) {
        return true;
    }

    if(prev.frameDurationNs == 0 || next.frameDurationNs == 0 || prev.frameDurationNs != next.frameDurationNs) {
        return false;
    }

    const uint64_t expectedStartNs
        = prev.startNs + (prev.frameDurationNs * static_cast<uint64_t>(std::max(0, prev.frames)));
    return expectedStartNs == next.startNs;
}

void TimedAudioFifo::appendTimelineChunkMerged(std::vector<TimelineChunk>& timelineChunks, const TimelineChunk& chunk)
{
    if(chunk.frames <= 0) {
        return;
    }

    if(!timelineChunks.empty() && canMergeTimelineChunks(timelineChunks.back(), chunk)) {
        timelineChunks.back().frames += chunk.frames;
        return;
    }

    timelineChunks.push_back(chunk);
}

void TimedAudioFifo::appendTimeline(std::span<const TimelineChunk> timelineChunks, int frameCount)
{
    if(!timelineChunks.empty()) {
        m_timeline.reserve(m_timeline.size() + timelineChunks.size());

        for(const auto& chunk : timelineChunks) {
            appendTimelineChunkMerged(m_timeline, chunk);
        }
        return;
    }

    if(frameCount <= 0) {
        return;
    }

    appendTimelineChunkMerged(m_timeline, TimelineChunk{
                                              .valid           = false,
                                              .startNs         = 0,
                                              .frameDurationNs = 0,
                                              .frames          = frameCount,
                                              .sampleRate      = m_buffer.format().sampleRate(),
                                              .streamId        = m_streamId,
                                              .epoch           = m_epoch,
                                          });
}
} // namespace Fooyin
