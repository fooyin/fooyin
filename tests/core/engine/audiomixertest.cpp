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

#include <core/engine/pipeline/audiomixer.h>

#include <gtest/gtest.h>

#include <memory>
#include <vector>

using namespace Qt::StringLiterals;

constexpr auto SampleRate      = 10;
constexpr auto Channels        = 1;
constexpr uint64_t NsPerMs     = 1'000'000ULL;
constexpr uint64_t NsPerSecond = 1'000'000'000ULL;

namespace Fooyin::Testing {
namespace {
AudioFormat testFormat()
{
    return {SampleFormat::F64, SampleRate, Channels};
}

AudioFormat highRateFormat()
{
    return {SampleFormat::F64, SampleRate * 2, Channels};
}

class LatencyNode final : public DspNode
{
public:
    explicit LatencyNode(int latencyFrames, int outputSampleRate = 0)
        : m_latencyFrames{latencyFrames}
        , m_outputSampleRate{outputSampleRate}
    { }

    QString name() const override
    {
        return u"LatencyNode"_s;
    }

    QString id() const override
    {
        return u"latency-node"_s;
    }

    void prepare(const AudioFormat&) override { }

    void process(ProcessingBufferList&) override { }

    AudioFormat outputFormat(const AudioFormat& input) const override
    {
        auto output = input;
        if(m_outputSampleRate > 0) {
            output.setSampleRate(m_outputSampleRate);
        }
        return output;
    }

    int latencyFrames() const override
    {
        return m_latencyFrames;
    }

private:
    int m_latencyFrames{0};
    int m_outputSampleRate{0};
};

class SkipMiddleFrameNode final : public DspNode
{
public:
    QString name() const override
    {
        return u"SkipMiddleFrameNode"_s;
    }

    QString id() const override
    {
        return u"skip-middle-frame-node"_s;
    }

    void prepare(const AudioFormat&) override { }

    void process(ProcessingBufferList& chunks) override
    {
        if(chunks.count() == 0) {
            return;
        }

        const auto* input = chunks.item(0);
        if(!input || !input->isValid() || input->frameCount() < 3 || input->format().channelCount() != 1) {
            return;
        }

        const auto data   = input->constData();
        const auto format = input->format();
        const uint64_t t0 = input->startTimeNs();
        const uint64_t t2 = t0 + (2ULL * NsPerSecond / static_cast<uint64_t>(format.sampleRate()));

        ProcessingBufferList output;
        if(auto* first = output.addItem(format, t0, 1)) {
            first->samples()[0] = data[0];
        }
        if(auto* second = output.addItem(format, t2, 1)) {
            second->samples()[0] = data[2];
        }

        chunks.clear();
        for(size_t i{0}; i < output.count(); ++i) {
            if(const auto* chunk = output.item(i)) {
                chunks.addChunk(*chunk);
            }
        }
    }
};

AudioStreamPtr makeStream(const std::vector<double>& samples, bool playing)
{
    auto stream        = StreamFactory::createStream(testFormat(), 128);
    auto writer        = stream->writer();
    const auto written = writer.write(samples.data(), samples.size());
    EXPECT_EQ(written, samples.size());
    if(playing) {
        stream->applyCommand(AudioStream::Command::Play);
    }
    return stream;
}

AudioStreamPtr makeStream(const AudioFormat& format, const std::vector<double>& samples, bool playing)
{
    auto stream        = StreamFactory::createStream(format, 256);
    auto writer        = stream->writer();
    const auto written = writer.write(samples.data(), samples.size());
    EXPECT_EQ(written, samples.size());
    if(playing) {
        stream->applyCommand(AudioStream::Command::Play);
    }
    return stream;
}

std::vector<double> readSamples(AudioMixer& mixer, int frames)
{
    ProcessingBuffer output{testFormat(), 0};
    const int readFrames = mixer.read(output, frames);
    EXPECT_EQ(readFrames, frames);
    const auto data = output.constData();
    return std::vector<double>(data.begin(), data.end());
}
} // namespace

TEST(AudioMixerTest, ReadsSinglePlayingStream)
{
    AudioMixer mixer{testFormat()};
    auto stream = makeStream({0.1, 0.2, 0.3, 0.4}, true);
    mixer.addStream(stream);

    const auto out = readSamples(mixer, 4);
    EXPECT_EQ(out, std::vector<double>({0.1, 0.2, 0.3, 0.4}));
    EXPECT_EQ(mixer.primaryStream(), stream);
}

TEST(AudioMixerTest, SplicesPendingStreamWhenActiveEndsMidBlock)
{
    AudioMixer mixer{testFormat()};

    auto active = makeStream({1.0, 1.0}, true);
    active->setEndOfInput();

    auto pending = makeStream({2.0, 2.0, 2.0}, false); // Pending by default

    mixer.addStream(active);
    mixer.addStream(pending);

    const auto out = readSamples(mixer, 4);
    EXPECT_EQ(out, std::vector<double>({1.0, 1.0, 2.0, 2.0}));
    EXPECT_EQ(pending->state(), AudioStream::State::Playing);
}

TEST(AudioMixerTest, PromotesPendingStreamWhenNoActiveStreamExists)
{
    AudioMixer mixer{testFormat()};
    auto pending = makeStream({0.5, 0.6}, false);
    mixer.addStream(pending);

    const auto out = readSamples(mixer, 2);
    EXPECT_EQ(out, std::vector<double>({0.5, 0.6}));
    EXPECT_EQ(pending->state(), AudioStream::State::Playing);
}

TEST(AudioMixerTest, ReportsNeedsMoreDataOnlyBeforeEndOfInput)
{
    AudioMixer mixer{testFormat()};
    auto stream = makeStream({0.25}, true);
    mixer.addStream(stream);

    EXPECT_TRUE(mixer.needsMoreData());

    stream->setEndOfInput();
    EXPECT_FALSE(mixer.needsMoreData());
}

TEST(AudioMixerTest, PauseAndResumeAllAffectPlayingStreams)
{
    AudioMixer mixer{testFormat()};
    auto first  = makeStream({0.1, 0.1}, true);
    auto second = makeStream({0.2, 0.2}, true);
    mixer.addStream(first);
    mixer.addStream(second);

    mixer.pauseAll();
    EXPECT_EQ(first->state(), AudioStream::State::Paused);
    EXPECT_EQ(second->state(), AudioStream::State::Paused);

    mixer.resumeAll();
    EXPECT_EQ(first->state(), AudioStream::State::Playing);
    EXPECT_EQ(second->state(), AudioStream::State::Playing);
}

TEST(AudioMixerTest, ReportsPerTrackLatencyInSecondsAcrossRateChanges)
{
    AudioMixer mixer{testFormat()};
    auto stream = makeStream({0.1, 0.2, 0.3}, true);

    std::vector<DspNodePtr> perTrack;
    perTrack.push_back(std::make_unique<LatencyNode>(5, 20)); // 5/10 seconds then switch to 20 Hz
    perTrack.push_back(std::make_unique<LatencyNode>(10));    // 10/20 seconds
    mixer.addStream(stream, std::move(perTrack));

    EXPECT_NEAR(mixer.primaryStreamDspLatencySeconds(), 1.0, 1e-12);
    EXPECT_EQ(mixer.primaryStreamDspLatencyFrames(), 10);
}

TEST(AudioMixerTest, SwitchToDuringCrossfadeDropsOldStreamsImmediately)
{
    AudioMixer mixer{testFormat()};
    auto first  = makeStream({1.0, 1.0, 1.0}, true);
    auto second = makeStream({3.0, 3.0, 3.0}, true);
    mixer.addStream(first);
    mixer.addStream(second);

    auto replacement             = makeStream({9.0, 9.0, 9.0}, true);
    const StreamId replacementId = replacement->id();
    const StreamId firstId       = first->id();
    const StreamId secondId      = second->id();
    mixer.switchTo(replacement);

    EXPECT_EQ(mixer.streamCount(), 1U);
    EXPECT_TRUE(mixer.hasStream(replacementId));
    EXPECT_FALSE(mixer.hasStream(firstId));
    EXPECT_FALSE(mixer.hasStream(secondId));
    EXPECT_EQ(mixer.primaryStream(), replacement);

    const auto out = readSamples(mixer, 3);
    EXPECT_EQ(out, std::vector<double>({9.0, 9.0, 9.0}));
}

TEST(AudioMixerTest, LongRunPendingHandoffsRemainGaplessWithRateConversion)
{
    AudioMixer mixer{testFormat()};
    mixer.setFormat(testFormat());
    mixer.setOutputFormat(testFormat());

    auto active = makeStream(testFormat(), std::vector<double>(8, 1.0), true);
    active->setEndOfInput();
    mixer.addStream(active);

    for(int i{0}; i < 200; ++i) {
        const AudioFormat pendingFormat = (i % 2 == 0) ? highRateFormat() : testFormat();
        auto pending                    = makeStream(pendingFormat, std::vector<double>(16, 2.0), false);
        pending->setEndOfInput();
        mixer.addStream(pending);

        ProcessingBuffer output{testFormat(), 0};
        const int framesRead = mixer.read(output, 8);
        ASSERT_EQ(framesRead, 8);

        const auto data = output.constData();
        ASSERT_FALSE(data.empty());
        EXPECT_EQ(data.size(), 8U);
    }
}

TEST(AudioMixerTest, OutputBufferCarriesSourceStartTimeForSuccessiveReads)
{
    AudioMixer mixer{testFormat()};
    auto stream = makeStream({0.1, 0.2, 0.3, 0.4}, true);
    mixer.addStream(stream);

    ProcessingBuffer first{testFormat(), 0};
    ASSERT_EQ(mixer.read(first, 2), 2);
    EXPECT_EQ(first.startTimeNs(), 0U);

    ProcessingBuffer second{testFormat(), 0};
    ASSERT_EQ(mixer.read(second, 2), 2);
    EXPECT_EQ(second.startTimeNs(), 200U * NsPerMs);
}

TEST(AudioMixerTest, OutputBufferStartTimeAdvancesAcrossLeftoverDrain)
{
    // When the mixer produces more frames than requested, leftover is cached.
    // The next read drains from that cache; its startTimeNs must continue from
    // where the first read ended.

    AudioMixer mixer{testFormat()};
    auto stream = makeStream({1.0, 1.0, 1.0, 1.0}, true);
    mixer.addStream(stream);

    ProcessingBuffer first{testFormat(), 0};
    ASSERT_EQ(mixer.read(first, 3), 3);
    EXPECT_EQ(first.startTimeNs(), 0U);

    ProcessingBuffer second{testFormat(), 0};
    ASSERT_EQ(mixer.read(second, 1), 1);
    EXPECT_EQ(second.startTimeNs(), 300U * NsPerMs); // 3 frames @ 10 Hz = 300 ms
}

TEST(AudioMixerTest, OutputBufferStartTimeReflectsPerTrackDspTimeJump)
{
    // SkipMiddleFrameNode removes the middle frame and adjusts startTime on its
    // second output chunk to reflect the source-time discontinuity.
    // The mixer must use the source time of frame[0] for the output buffer.

    AudioMixer mixer{testFormat()};
    auto stream = makeStream({10.0, 20.0, 30.0}, true);

    std::vector<DspNodePtr> perTrack;
    perTrack.push_back(std::make_unique<SkipMiddleFrameNode>());
    mixer.addStream(stream, std::move(perTrack));

    ProcessingBuffer out{testFormat(), 0};
    ASSERT_EQ(mixer.read(out, 2), 2);

    const auto samples = out.constData();
    ASSERT_EQ(samples.size(), 2U);
    EXPECT_EQ(samples[0], 10.0);
    EXPECT_EQ(samples[1], 30.0);

    EXPECT_EQ(out.startTimeNs(), 0U);
}
} // namespace Fooyin::Testing
