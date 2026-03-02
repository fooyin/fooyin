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

#include "core/engine/pipeline/timedaudiofifo.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <numeric>
#include <span>
#include <vector>

namespace Fooyin::Testing {
namespace {
AudioBuffer makeF64Buffer(std::initializer_list<double> samples, uint64_t startTimeMs)
{
    std::vector<double> values{samples};
    const AudioFormat format{SampleFormat::F64, 10, 1};
    return {std::as_bytes(std::span<const double>{values}), format, startTimeMs};
}

AudioBuffer makeS16Buffer(std::initializer_list<int16_t> samples, uint64_t startTimeMs)
{
    std::vector<int16_t> values{samples};
    const AudioFormat format{SampleFormat::S16, 10, 1};
    return {std::as_bytes(std::span<const int16_t>{values}), format, startTimeMs};
}

AudioBuffer makeF64SequenceBuffer(size_t frameCount, uint64_t startTimeMs)
{
    std::vector<double> values(frameCount, 0.0);
    std::iota(values.begin(), values.end(), 0.0);
    const AudioFormat format{SampleFormat::F64, 10, 1};
    return {std::as_bytes(std::span<const double>{values}), format, startTimeMs};
}

std::span<const double> asF64Samples(const AudioBuffer& buffer)
{
    const auto bytes = buffer.constData();
    return {reinterpret_cast<const double*>(bytes.data()), bytes.size() / sizeof(double)};
}
} // namespace

TEST(TimedAudioFifoTest, AppendFormatMismatchDoesNotMutateExistingQueue)
{
    TimedAudioFifo fifo;
    const AudioBuffer initial = makeF64Buffer({0.1, 0.2}, 0);
    static constexpr std::array timeline{TimedAudioFifo::TimelineChunk{
        .valid           = true,
        .startNs         = 0,
        .frameDurationNs = 100'000'000ULL,
        .frames          = 2,
        .sampleRate      = 10,
        .streamId        = 11,
        .epoch           = 7,
    }};

    fifo.set(initial, timeline, 11, 7);
    ASSERT_TRUE(fifo.hasData());
    ASSERT_EQ(fifo.queuedFrames(), 2);
    ASSERT_EQ(fifo.streamId(), 11U);
    ASSERT_EQ(fifo.epoch(), 7U);

    const AudioBuffer incompatible = makeS16Buffer({100, 200}, 500);
    const auto appendResult        = fifo.append(incompatible, {}, 22, 9);

    EXPECT_FALSE(appendResult.appended);
    EXPECT_TRUE(appendResult.formatMismatch);
    EXPECT_EQ(appendResult.appendedFrames, 0);
    EXPECT_EQ(fifo.queuedFrames(), 2);
    EXPECT_EQ(fifo.streamId(), 11U);
    EXPECT_EQ(fifo.epoch(), 7U);
}

TEST(TimedAudioFifoTest, AppendOrReplaceReplacesQueueOnFormatMismatch)
{
    TimedAudioFifo fifo;
    const AudioBuffer initial     = makeF64Buffer({0.1, 0.2}, 0);
    const AudioBuffer replacement = makeS16Buffer({300, 400}, 900);
    static constexpr std::array replacementTimeline{TimedAudioFifo::TimelineChunk{
        .valid           = true,
        .startNs         = 900'000'000ULL,
        .frameDurationNs = 100'000'000ULL,
        .frames          = 2,
        .sampleRate      = 10,
        .streamId        = 22,
        .epoch           = 9,
    }};

    fifo.set(initial, {}, 11, 7);
    const auto appendResult = fifo.appendOrReplace(replacement, replacementTimeline, 22, 9);

    EXPECT_TRUE(appendResult.appended);
    EXPECT_TRUE(appendResult.formatMismatch);
    EXPECT_EQ(appendResult.appendedFrames, 2);
    EXPECT_EQ(fifo.queuedFrames(), 2);
    EXPECT_EQ(fifo.streamId(), 22U);
    EXPECT_EQ(fifo.epoch(), 9U);
    ASSERT_EQ(fifo.timeline().size(), 1U);
    EXPECT_TRUE(fifo.timeline().front().valid);
    EXPECT_EQ(fifo.timeline().front().startNs, 900'000'000ULL);
}

TEST(TimedAudioFifoTest, PartialWriteCompactPreservesRemainingDataAndQueueOffsets)
{
    TimedAudioFifo fifo;
    const AudioBuffer buffer = makeF64SequenceBuffer(10, 0);
    static constexpr std::array timeline{TimedAudioFifo::TimelineChunk{
        .valid           = true,
        .startNs         = 0,
        .frameDurationNs = 100'000'000ULL,
        .frames          = 10,
        .sampleRate      = 10,
        .streamId        = 11,
        .epoch           = 1,
    }};

    fifo.set(buffer, timeline, 11, 1);

    int firstOffset{std::numeric_limits<int>::min()};
    const auto firstWrite = fifo.write([&](const AudioBuffer&, int frameOffset) {
        firstOffset = frameOffset;
        return 3;
    });
    ASSERT_EQ(firstOffset, 0);
    EXPECT_EQ(firstWrite.queueOffsetStartFrames, 0);
    EXPECT_EQ(firstWrite.queueOffsetEndFrames, 3);
    EXPECT_EQ(firstWrite.writtenFrames, 3);

    fifo.compact();
    ASSERT_EQ(fifo.queuedFrames(), 7);

    int secondOffset{std::numeric_limits<int>::min()};
    std::vector<double> drained;
    const auto secondWrite = fifo.write([&](const AudioBuffer& queuedBuffer, int frameOffset) {
        secondOffset        = frameOffset;
        const auto samples  = asF64Samples(queuedBuffer);
        const auto frameCnt = queuedBuffer.frameCount();
        drained.assign(samples.begin() + frameOffset, samples.begin() + frameCnt);
        return frameCnt - frameOffset;
    });

    ASSERT_EQ(secondOffset, 3);
    EXPECT_EQ(secondWrite.queueOffsetStartFrames, 0);
    EXPECT_EQ(secondWrite.queueOffsetEndFrames, 7);
    EXPECT_EQ(secondWrite.writtenFrames, 7);
    ASSERT_EQ(drained.size(), 7U);
    for(size_t i{0}; i < drained.size(); ++i) {
        EXPECT_DOUBLE_EQ(drained[i], static_cast<double>(i + 3U));
    }
}

TEST(TimedAudioFifoTest, DropFrontFramesAdvancesTimelineWithoutResettingPhysicalOffset)
{
    TimedAudioFifo fifo;
    const AudioBuffer buffer = makeF64SequenceBuffer(8, 0);
    static constexpr std::array timeline{TimedAudioFifo::TimelineChunk{
        .valid           = true,
        .startNs         = 0,
        .frameDurationNs = 100'000'000ULL,
        .frames          = 8,
        .sampleRate      = 10,
        .streamId        = 12,
        .epoch           = 1,
    }};

    fifo.set(buffer, timeline, 12, 1);
    fifo.dropFrontFrames(2);

    ASSERT_EQ(fifo.queuedFrames(), 6);
    ASSERT_EQ(fifo.timeline().size(), 1U);
    EXPECT_EQ(fifo.timeline().front().frames, 6);
    EXPECT_EQ(fifo.timeline().front().startNs, 200'000'000ULL);

    int writeOffset{std::numeric_limits<int>::min()};
    double firstReadableSample{-1.0};
    const auto writeResult = fifo.write([&](const AudioBuffer& queuedBuffer, int frameOffset) {
        writeOffset         = frameOffset;
        const auto samples  = asF64Samples(queuedBuffer);
        firstReadableSample = samples[static_cast<size_t>(frameOffset)];
        return queuedBuffer.frameCount() - frameOffset;
    });

    EXPECT_EQ(writeOffset, 2);
    EXPECT_EQ(firstReadableSample, 2.0);
    EXPECT_EQ(writeResult.queueOffsetStartFrames, 0);
    EXPECT_EQ(writeResult.queueOffsetEndFrames, 6);
    EXPECT_EQ(writeResult.writtenFrames, 6);
}

TEST(TimedAudioFifoTest, CompactRepacksBufferAfterLargeLogicalConsumption)
{
    TimedAudioFifo fifo;
    constexpr int TotalFrames = 40000;
    constexpr int Consumed    = 17000;

    const AudioBuffer buffer = makeF64SequenceBuffer(static_cast<size_t>(TotalFrames), 0);
    static constexpr std::array timeline{TimedAudioFifo::TimelineChunk{
        .valid           = true,
        .startNs         = 0,
        .frameDurationNs = 100'000'000ULL,
        .frames          = TotalFrames,
        .sampleRate      = 10,
        .streamId        = 13,
        .epoch           = 1,
    }};

    fifo.set(buffer, timeline, 13, 1);

    const auto firstWrite = fifo.write([](const AudioBuffer&, const int) { return Consumed; });
    ASSERT_EQ(firstWrite.writtenFrames, Consumed);

    fifo.compact();
    ASSERT_EQ(fifo.queuedFrames(), TotalFrames - Consumed);

    int writeOffset{std::numeric_limits<int>::min()};
    double firstReadableSample{-1.0};
    const auto secondWrite = fifo.write([&](const AudioBuffer& queuedBuffer, int frameOffset) {
        writeOffset         = frameOffset;
        const auto samples  = asF64Samples(queuedBuffer);
        firstReadableSample = samples[static_cast<size_t>(frameOffset)];
        return 1;
    });

    EXPECT_EQ(writeOffset, 0);
    EXPECT_EQ(firstReadableSample, static_cast<double>(Consumed));
    EXPECT_EQ(secondWrite.queueOffsetStartFrames, 0);
    EXPECT_EQ(secondWrite.queueOffsetEndFrames, 1);
    EXPECT_EQ(secondWrite.writtenFrames, 1);
}

TEST(TimedAudioFifoTest, AppendMergesAdjacentCompatibleTimelineChunks)
{
    TimedAudioFifo fifo;
    const AudioBuffer firstBuffer  = makeF64SequenceBuffer(4, 0);
    const AudioBuffer secondBuffer = makeF64SequenceBuffer(3, 400);

    static constexpr std::array firstTimeline{TimedAudioFifo::TimelineChunk{
        .valid           = true,
        .startNs         = 0,
        .frameDurationNs = 100,
        .frames          = 4,
        .sampleRate      = 10,
        .streamId        = 21,
        .epoch           = 5,
    }};
    static constexpr std::array secondTimeline{TimedAudioFifo::TimelineChunk{
        .valid           = true,
        .startNs         = 400,
        .frameDurationNs = 100,
        .frames          = 3,
        .sampleRate      = 10,
        .streamId        = 21,
        .epoch           = 5,
    }};

    fifo.set(firstBuffer, firstTimeline, 21, 5);
    const auto appendResult = fifo.append(secondBuffer, secondTimeline, 21, 5);

    ASSERT_TRUE(appendResult.appended);
    ASSERT_EQ(fifo.timeline().size(), 1U);
    EXPECT_TRUE(fifo.timeline().front().valid);
    EXPECT_EQ(fifo.timeline().front().startNs, 0U);
    EXPECT_EQ(fifo.timeline().front().frameDurationNs, 100U);
    EXPECT_EQ(fifo.timeline().front().frames, 7);
}

TEST(TimedAudioFifoTest, AppendKeepsTimelineBoundaryAcrossEpochChange)
{
    TimedAudioFifo fifo;
    const AudioBuffer firstBuffer  = makeF64SequenceBuffer(2, 0);
    const AudioBuffer secondBuffer = makeF64SequenceBuffer(2, 200);

    fifo.set(firstBuffer, {}, 30, 1);
    const auto appendResult = fifo.append(secondBuffer, {}, 30, 2);

    ASSERT_TRUE(appendResult.appended);
    ASSERT_EQ(fifo.timeline().size(), 2U);
    EXPECT_FALSE(fifo.timeline().at(0).valid);
    EXPECT_FALSE(fifo.timeline().at(1).valid);
    EXPECT_EQ(fifo.timeline().at(0).frames, 2);
    EXPECT_EQ(fifo.timeline().at(0).epoch, 1U);
    EXPECT_EQ(fifo.timeline().at(1).frames, 2);
    EXPECT_EQ(fifo.timeline().at(1).epoch, 2U);
}
} // namespace Fooyin::Testing
