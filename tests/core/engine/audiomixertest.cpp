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

#include <core/engine/pipeline/mixer/audiomixer.h>

#include <utils/timeconstants.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <memory>
#include <vector>

using namespace Qt::StringLiterals;

constexpr auto SampleRate     = 10;
constexpr auto Channels       = 1;
constexpr auto FastSampleRate = 1000;

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

AudioFormat fastFormat()
{
    return {SampleFormat::F64, FastSampleRate, Channels};
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

class VariableLatencyNode final : public DspNode
{
public:
    explicit VariableLatencyNode(int latencyFrames)
        : m_latencyFrames{latencyFrames}
    { }

    QString name() const override
    {
        return u"VariableLatencyNode"_s;
    }

    QString id() const override
    {
        return u"variable-latency-node"_s;
    }

    void prepare(const AudioFormat&) override { }

    void process(ProcessingBufferList&) override { }

    int latencyFrames() const override
    {
        return std::max(0, m_latencyFrames.load(std::memory_order_acquire));
    }

    void setLatencyFrames(int frames)
    {
        m_latencyFrames.store(std::max(0, frames), std::memory_order_release);
    }

private:
    std::atomic<int> m_latencyFrames{0};
};

class HalfRateSkewNode final : public DspNode
{
public:
    QString name() const override
    {
        return u"HalfRateSkewNode"_s;
    }

    QString id() const override
    {
        return u"half-rate-skew-node"_s;
    }

    void prepare(const AudioFormat&) override { }

    void process(ProcessingBufferList& chunks) override
    {
        if(chunks.count() == 0) {
            return;
        }

        ProcessingBufferList output;
        const size_t count = chunks.count();
        for(size_t i{0}; i < count; ++i) {
            const auto* input = chunks.item(i);
            if(!input || !input->isValid()) {
                continue;
            }

            const int channels = input->format().channelCount();
            const int frames   = input->frameCount();
            if(channels <= 0 || frames <= 0) {
                continue;
            }

            const int keptFrames = std::max(1, frames / 2);
            auto* outChunk       = output.addItem(input->format(), input->startTimeNs(),
                                                  static_cast<size_t>(keptFrames) * static_cast<size_t>(channels));
            if(!outChunk) {
                continue;
            }

            const auto inData = input->constData();
            auto outData      = outChunk->data();
            for(int frame{0}; frame < keptFrames; ++frame) {
                const int inFrame = frame * 2;
                if(inFrame >= frames) {
                    break;
                }
                for(int ch{0}; ch < channels; ++ch) {
                    const size_t inIdx
                        = (static_cast<size_t>(inFrame) * static_cast<size_t>(channels)) + static_cast<size_t>(ch);
                    const size_t outIdx
                        = (static_cast<size_t>(frame) * static_cast<size_t>(channels)) + static_cast<size_t>(ch);
                    outData[outIdx] = inData[inIdx];
                }
            }
        }

        chunks.clear();
        for(size_t i{0}; i < output.count(); ++i) {
            const auto* chunk = output.item(i);
            if(chunk && chunk->isValid()) {
                chunks.addChunk(*chunk);
            }
        }
    }

    int latencyFrames() const override
    {
        return 2;
    }
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
        const uint64_t t2 = t0 + (2ULL * Time::NsPerSecond / static_cast<uint64_t>(format.sampleRate()));

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

class UnreportedWarmupNode final : public DspNode
{
public:
    explicit UnreportedWarmupNode(int warmupFrames)
        : m_warmupFrames{std::max(0, warmupFrames)}
    { }

    QString name() const override
    {
        return u"UnreportedWarmupNode"_s;
    }

    QString id() const override
    {
        return u"unreported-warmup-node"_s;
    }

    void prepare(const AudioFormat&) override
    {
        m_seenFrames = 0;
    }

    void process(ProcessingBufferList& chunks) override
    {
        ProcessingBufferList output;
        for(size_t i{0}; i < chunks.count(); ++i) {
            const auto* chunk = chunks.item(i);
            if(!chunk || !chunk->isValid() || chunk->frameCount() <= 0) {
                continue;
            }

            m_seenFrames += chunk->frameCount();
            if(m_seenFrames < m_warmupFrames) {
                continue;
            }

            output.addChunk(*chunk);
        }

        chunks.clear();
        for(size_t i{0}; i < output.count(); ++i) {
            const auto* chunk = output.item(i);
            if(chunk && chunk->isValid()) {
                chunks.addChunk(*chunk);
            }
        }
    }

private:
    int m_warmupFrames{0};
    int m_seenFrames{0};
};

class DuplicateFramesNode final : public DspNode
{
public:
    QString name() const override
    {
        return u"DuplicateFramesNode"_s;
    }

    QString id() const override
    {
        return u"duplicate-frames-node"_s;
    }

    void prepare(const AudioFormat&) override { }

    void process(ProcessingBufferList& chunks) override
    {
        ProcessingBufferList output;
        for(size_t i{0}; i < chunks.count(); ++i) {
            const auto* chunk = chunks.item(i);
            if(!chunk || !chunk->isValid()) {
                continue;
            }

            const auto format   = chunk->format();
            const int channels  = format.channelCount();
            const int inFrames  = chunk->frameCount();
            const auto inValues = chunk->constData();
            if(channels <= 0 || inFrames <= 0 || inValues.empty()) {
                continue;
            }

            const int outFrames = inFrames * 2;
            auto* outChunk      = output.addItem(format, chunk->startTimeNs(),
                                                 static_cast<size_t>(outFrames) * static_cast<size_t>(channels));
            if(!outChunk) {
                continue;
            }

            auto outValues = outChunk->data();
            for(int frame{0}; frame < inFrames; ++frame) {
                for(int ch{0}; ch < channels; ++ch) {
                    const size_t inIdx  = static_cast<size_t>(frame) * static_cast<size_t>(channels) + ch;
                    const size_t outIdx = static_cast<size_t>(frame * 2) * static_cast<size_t>(channels) + ch;
                    outValues[outIdx]   = inValues[inIdx];
                    outValues[outIdx + static_cast<size_t>(channels)] = inValues[inIdx];
                }
            }
        }

        chunks.clear();
        for(size_t i{0}; i < output.count(); ++i) {
            const auto* chunk = output.item(i);
            if(chunk && chunk->isValid()) {
                chunks.addChunk(*chunk);
            }
        }
    }
};

class FlushTailGainNode final : public DspNode
{
public:
    FlushTailGainNode(QString nodeId, double processGain, double flushSample, bool emitOnFlush = true)
        : m_id{std::move(nodeId)}
        , m_processGain{processGain}
        , m_flushSample{flushSample}
        , m_emitOnFlush{emitOnFlush}
    { }

    QString name() const override
    {
        return m_id;
    }

    QString id() const override
    {
        return m_id;
    }

    void prepare(const AudioFormat& format) override
    {
        m_format = format;
    }

    void process(ProcessingBufferList& chunks) override
    {
        for(size_t i{0}; i < chunks.count(); ++i) {
            auto* chunk = chunks.item(i);
            if(!chunk) {
                continue;
            }

            auto data = chunk->data();
            for(auto& sample : data) {
                sample *= m_processGain;
            }
        }
    }

    void flush(ProcessingBufferList& chunks, FlushMode /*mode*/) override
    {
        if(!m_emitOnFlush) {
            return;
        }

        const AudioFormat format = m_format.isValid() ? m_format : AudioFormat{SampleFormat::F64, SampleRate, 1};
        auto* chunk              = chunks.addItem(format, 0, static_cast<size_t>(format.channelCount()));
        if(!chunk) {
            return;
        }

        auto data = chunk->data();
        if(!data.empty()) {
            data[0] = m_flushSample;
        }
    }

private:
    QString m_id;
    double m_processGain{1.0};
    double m_flushSample{0.0};
    bool m_emitOnFlush{true};
    AudioFormat m_format;
};

class ResetCountingNode final : public DspNode
{
public:
    explicit ResetCountingNode(std::shared_ptr<std::atomic<int>> resetCount)
        : m_resetCount{std::move(resetCount)}
    { }

    QString name() const override
    {
        return u"ResetCountingNode"_s;
    }

    QString id() const override
    {
        return u"reset-counting-node"_s;
    }

    void prepare(const AudioFormat&) override { }

    void process(ProcessingBufferList&) override { }

    void reset() override
    {
        if(m_resetCount) {
            m_resetCount->fetch_add(1, std::memory_order_acq_rel);
        }
    }

private:
    std::shared_ptr<std::atomic<int>> m_resetCount;
};

class GainNode final : public DspNode
{
public:
    GainNode(QString nodeId, double gain)
        : m_id{std::move(nodeId)}
        , m_gain{gain}
    { }

    QString name() const override
    {
        return m_id;
    }

    QString id() const override
    {
        return m_id;
    }

    void prepare(const AudioFormat& format) override
    {
        Q_UNUSED(format)
    }

    void process(ProcessingBufferList& chunks) override
    {
        for(size_t i{0}; i < chunks.count(); ++i) {
            auto* chunk = chunks.item(i);
            if(!chunk) {
                continue;
            }

            auto data = chunk->data();
            for(auto& sample : data) {
                sample *= m_gain;
            }
        }
    }

private:
    QString m_id;
    double m_gain{1.0};
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

TEST(AudioMixerTest, UpdatesPerTrackLatencyAtRuntimeFromNodeReports)
{
    AudioMixer mixer{testFormat()};
    auto stream = makeStream(std::vector<double>(32, 0.5), true);

    std::vector<DspNodePtr> perTrack;
    auto node     = std::make_unique<VariableLatencyNode>(0);
    auto* nodePtr = node.get();
    perTrack.push_back(std::move(node));
    mixer.addStream(stream, std::move(perTrack));

    EXPECT_DOUBLE_EQ(mixer.primaryStreamDspLatencySeconds(), 0.0);
    EXPECT_EQ(mixer.primaryStreamDspLatencyFrames(), 0);

    nodePtr->setLatencyFrames(5);

    ProcessingBuffer output{testFormat(), 0};
    ASSERT_EQ(mixer.read(output, 2), 2);

    EXPECT_NEAR(mixer.primaryStreamDspLatencySeconds(), 0.5, 1e-6);
    EXPECT_EQ(mixer.primaryStreamDspLatencyFrames(), 5);
}

TEST(AudioMixerTest, ResetStreamForSeekClearsSmoothedLatencyState)
{
    AudioMixer mixer{testFormat()};
    auto stream = makeStream(std::vector<double>(64, 0.25), true);

    std::vector<DspNodePtr> perTrack;
    auto node     = std::make_unique<VariableLatencyNode>(8);
    auto* nodePtr = node.get();
    perTrack.push_back(std::move(node));
    mixer.addStream(stream, std::move(perTrack));

    ProcessingBuffer first{testFormat(), 0};
    ASSERT_EQ(mixer.read(first, 2), 2);
    EXPECT_GT(mixer.primaryStreamDspLatencySeconds(), 0.0);

    nodePtr->setLatencyFrames(0);
    mixer.resetStreamForSeek(stream->id());
    {
        const std::vector<double> refill(64, 0.25);
        auto writer        = stream->writer();
        const auto written = writer.write(refill.data(), refill.size());
        EXPECT_EQ(written, refill.size());
    }

    ProcessingBuffer second{testFormat(), 0};
    ASSERT_EQ(mixer.read(second, 2), 2);

    EXPECT_DOUBLE_EQ(mixer.primaryStreamDspLatencySeconds(), 0.0);
    EXPECT_EQ(mixer.primaryStreamDspLatencyFrames(), 0);
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

TEST(AudioMixerTest, PerTrackInflightIsBoundedByNodeLatencyWhenTemporalDrifts)
{
    AudioMixer mixer{testFormat()};
    auto stream = makeStream(std::vector<double>(120, 0.25), true);

    std::vector<DspNodePtr> perTrack;
    perTrack.push_back(std::make_unique<HalfRateSkewNode>());
    mixer.addStream(stream, std::move(perTrack));

    uint64_t maxInflightNs{0};
    for(int i{0}; i < 10; ++i) {
        ProcessingBuffer output{testFormat(), 0};
        ASSERT_GT(mixer.read(output, 4), 0);
        maxInflightNs = std::max(maxInflightNs, mixer.primaryStreamPerTrackDspInflightNs());
    }

    EXPECT_GT(maxInflightNs, 0);
    EXPECT_LE(maxInflightNs, 500ULL * Time::NsPerMs);
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
    EXPECT_EQ(second.startTimeNs(), 200U * Time::NsPerMs);
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
    EXPECT_EQ(second.startTimeNs(), 300U * Time::NsPerMs); // 3 frames @ 10 Hz = 300 ms
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

TEST(AudioMixerTest, DurationBudgetTopUpHandlesUnreportedWarmup)
{
    AudioMixer mixer{fastFormat()};
    mixer.setFormat(fastFormat());
    mixer.setOutputFormat(fastFormat());

    auto stream = makeStream(fastFormat(), std::vector<double>(256, 1.0), true);

    std::vector<DspNodePtr> perTrack;
    perTrack.push_back(std::make_unique<UnreportedWarmupNode>(40));
    mixer.addStream(stream, std::move(perTrack));

    ProcessingBuffer output{fastFormat(), 0};
    EXPECT_EQ(mixer.read(output, 1), 1);
}

TEST(AudioMixerTest, DurationBudgetTopUpTraversesLongWarmupForLargeReadTarget)
{
    AudioMixer mixer{fastFormat()};
    mixer.setFormat(fastFormat());
    mixer.setOutputFormat(fastFormat());

    auto stream = StreamFactory::createStream(fastFormat(), 8192);
    ASSERT_TRUE(stream);
    {
        auto writer        = stream->writer();
        const auto samples = std::vector<double>(8000, 1.0);
        const auto written = writer.write(samples.data(), samples.size());
        ASSERT_EQ(written, samples.size());
    }
    stream->applyCommand(AudioStream::Command::Play);

    std::vector<DspNodePtr> perTrack;
    // Requires reading > 2200 input frames before any output appears.
    perTrack.push_back(std::make_unique<UnreportedWarmupNode>(2200));
    mixer.addStream(stream, std::move(perTrack));

    ProcessingBuffer output{fastFormat(), 0};
    EXPECT_GT(mixer.read(output, 256), 0);
}

TEST(AudioMixerTest, StreamResetWithoutMixerResetDrainsStalePerTrackOutput)
{
    AudioMixer mixer{testFormat()};
    auto stream = makeStream({1.0, 2.0, 3.0, 4.0}, true);

    std::vector<DspNodePtr> perTrack;
    perTrack.push_back(std::make_unique<DuplicateFramesNode>());
    mixer.addStream(stream, std::move(perTrack));

    ProcessingBuffer first{testFormat(), 0};
    ASSERT_EQ(mixer.read(first, 1), 1);
    ASSERT_EQ(first.constData().size(), 1U);
    EXPECT_EQ(first.constData()[0], 1.0);

    // Stream source is reset, but mixer per-track pending output is not.
    stream->applyCommand(AudioStream::Command::ResetForSeek);

    ProcessingBuffer second{testFormat(), 0};
    ASSERT_EQ(mixer.read(second, 1), 1);
    ASSERT_EQ(second.constData().size(), 1U);
    EXPECT_EQ(second.constData()[0], 1.0);
}

TEST(AudioMixerTest, ResetStreamForSeekClearsStalePerTrackOutputAfterStreamReset)
{
    AudioMixer mixer{testFormat()};
    auto stream = makeStream({1.0, 2.0, 3.0, 4.0}, true);

    std::vector<DspNodePtr> perTrack;
    perTrack.push_back(std::make_unique<DuplicateFramesNode>());
    mixer.addStream(stream, std::move(perTrack));

    ProcessingBuffer first{testFormat(), 0};
    ASSERT_EQ(mixer.read(first, 1), 1);

    stream->applyCommand(AudioStream::Command::ResetForSeek);
    mixer.resetStreamForSeek(stream->id());

    ProcessingBuffer second{testFormat(), 0};
    EXPECT_EQ(mixer.read(second, 1), 0);
}

TEST(AudioMixerTest, SwitchToClearsPendingPerTrackOutputFromReplacedStream)
{
    AudioMixer mixer{testFormat()};
    auto original = makeStream({1.0, 2.0, 3.0, 4.0}, true);

    std::vector<DspNodePtr> perTrack;
    perTrack.push_back(std::make_unique<DuplicateFramesNode>());
    mixer.addStream(original, std::move(perTrack));

    ProcessingBuffer first{testFormat(), 0};
    ASSERT_EQ(mixer.read(first, 1), 1);
    EXPECT_EQ(first.constData()[0], 1.0);

    auto replacement = makeStream({9.0, 9.0, 9.0}, true);
    mixer.switchTo(replacement);

    ProcessingBuffer afterSwitch{testFormat(), 0};
    ASSERT_EQ(mixer.read(afterSwitch, 1), 1);
    ASSERT_EQ(afterSwitch.constData().size(), 1U);
    EXPECT_EQ(afterSwitch.constData()[0], 9.0);
}

TEST(AudioMixerTest, SetOutputFormatClearsPendingPerTrackOutput)
{
    AudioMixer mixer{testFormat()};
    auto stream = makeStream({1.0, 2.0, 3.0, 4.0}, true);

    std::vector<DspNodePtr> perTrack;
    perTrack.push_back(std::make_unique<DuplicateFramesNode>());
    mixer.addStream(stream, std::move(perTrack));

    ProcessingBuffer first{testFormat(), 0};
    ASSERT_EQ(mixer.read(first, 1), 1);

    stream->applyCommand(AudioStream::Command::ResetForSeek);
    mixer.setOutputFormat(testFormat());

    ProcessingBuffer second{testFormat(), 0};
    EXPECT_EQ(mixer.read(second, 1), 0);
}

TEST(AudioMixerTest, EndOfTrackFlushRoutesPerTrackTailThroughDownstreamNodesOnly)
{
    AudioMixer mixer{testFormat()};
    auto stream = makeStream(std::vector<double>{}, true);
    stream->setEndOfInput();

    std::vector<DspNodePtr> perTrack;
    perTrack.push_back(std::make_unique<FlushTailGainNode>(u"first"_s, 2.0, 1.0));
    perTrack.push_back(std::make_unique<FlushTailGainNode>(u"second"_s, 3.0, 10.0));
    mixer.addStream(stream, std::move(perTrack));

    ProcessingBuffer output{testFormat(), 0};
    ASSERT_EQ(mixer.read(output, 2), 2);

    const auto data = output.constData();
    ASSERT_EQ(data.size(), 2U);
    EXPECT_DOUBLE_EQ(data[0], 3.0);
    EXPECT_DOUBLE_EQ(data[1], 10.0);
}

TEST(AudioMixerTest, ResetStreamForSeekResetsOnlyMatchingPerTrackNodes)
{
    AudioMixer mixer{testFormat()};

    auto first  = makeStream({0.1, 0.2, 0.3}, true);
    auto second = makeStream({0.4, 0.5, 0.6}, true);

    const auto firstResetCount  = std::make_shared<std::atomic<int>>(0);
    const auto secondResetCount = std::make_shared<std::atomic<int>>(0);

    std::vector<DspNodePtr> firstDsps;
    firstDsps.push_back(std::make_unique<ResetCountingNode>(firstResetCount));
    mixer.addStream(first, std::move(firstDsps));

    std::vector<DspNodePtr> secondDsps;
    secondDsps.push_back(std::make_unique<ResetCountingNode>(secondResetCount));
    mixer.addStream(second, std::move(secondDsps));

    mixer.resetStreamForSeek(first->id());

    EXPECT_EQ(firstResetCount->load(std::memory_order_acquire), 1);
    EXPECT_EQ(secondResetCount->load(std::memory_order_acquire), 0);
}

TEST(AudioMixerTest, IncrementalPerTrackPatchReplacesChangedNodes)
{
    AudioMixer mixer{testFormat()};
    auto stream = makeStream(std::vector<double>(8, 1.0), true);

    std::vector<DspNodePtr> perTrack;
    perTrack.push_back(std::make_unique<GainNode>(u"gain-2"_s, 2.0));
    perTrack.push_back(std::make_unique<GainNode>(u"gain-3"_s, 3.0));
    mixer.addStream(stream, std::move(perTrack));

    ProcessingBuffer before{testFormat(), 0};
    ASSERT_EQ(mixer.read(before, 1), 1);
    ASSERT_EQ(before.constData().size(), 1);
    EXPECT_DOUBLE_EQ(before.constData()[0], 6.0);

    const AudioMixer::PerTrackPatchPlan patchPlan{
        .prefixKeep     = 1,
        .oldMiddleCount = 1,
        .newMiddleCount = 1,
        .hasChanges     = true,
    };

    const bool applied = mixer.applyIncrementalPerTrackPatch(patchPlan, []() {
        std::vector<DspNodePtr> replacement;
        replacement.push_back(std::make_unique<GainNode>(u"gain-4"_s, 4.0));
        return replacement;
    });

    EXPECT_TRUE(applied);
    mixer.setOutputFormat(testFormat());

    ProcessingBuffer after{testFormat(), 0};
    ASSERT_EQ(mixer.read(after, 1), 1);
    ASSERT_EQ(after.constData().size(), 1);
    EXPECT_DOUBLE_EQ(after.constData()[0], 8.0);
}

TEST(AudioMixerTest, IncrementalPerTrackPatchRejectsInvalidRange)
{
    AudioMixer mixer{testFormat()};
    auto stream = makeStream(std::vector<double>(4, 1.0), true);

    std::vector<DspNodePtr> perTrack;
    perTrack.push_back(std::make_unique<GainNode>(u"gain-1"_s, 1.0));
    mixer.addStream(stream, std::move(perTrack));

    const AudioMixer::PerTrackPatchPlan invalidPlan{
        .prefixKeep     = 2,
        .oldMiddleCount = 1,
        .newMiddleCount = 0,
        .hasChanges     = true,
    };

    EXPECT_FALSE(mixer.canApplyIncrementalPerTrackPatch(invalidPlan));
    EXPECT_FALSE(mixer.applyIncrementalPerTrackPatch(invalidPlan, []() { return std::vector<DspNodePtr>{}; }));
}

TEST(AudioMixerTest, RebuildAllPerTrackDspsClearsPendingPerTrackOutputAfterStreamReset)
{
    AudioMixer mixer{testFormat()};
    auto stream = makeStream({1.0, 2.0, 3.0, 4.0}, true);

    std::vector<DspNodePtr> perTrack;
    perTrack.push_back(std::make_unique<DuplicateFramesNode>());
    mixer.addStream(stream, std::move(perTrack));

    ProcessingBuffer first{testFormat(), 0};
    ASSERT_EQ(mixer.read(first, 1), 1);

    stream->applyCommand(AudioStream::Command::ResetForSeek);

    mixer.rebuildAllPerTrackDsps([]() {
        std::vector<DspNodePtr> nodes;
        nodes.push_back(std::make_unique<DuplicateFramesNode>());
        return nodes;
    });

    ProcessingBuffer afterRebuild{testFormat(), 0};
    EXPECT_EQ(mixer.read(afterRebuild, 1), 0);
}

TEST(AudioMixerTest, IncrementalPerTrackPatchClearsPendingPerTrackOutputAfterStreamReset)
{
    AudioMixer mixer{testFormat()};
    auto stream = makeStream({1.0, 2.0, 3.0, 4.0}, true);

    std::vector<DspNodePtr> perTrack;
    perTrack.push_back(std::make_unique<DuplicateFramesNode>());
    mixer.addStream(stream, std::move(perTrack));

    ProcessingBuffer first{testFormat(), 0};
    ASSERT_EQ(mixer.read(first, 1), 1);

    stream->applyCommand(AudioStream::Command::ResetForSeek);

    const AudioMixer::PerTrackPatchPlan patchPlan{
        .prefixKeep     = 0,
        .oldMiddleCount = 1,
        .newMiddleCount = 1,
        .hasChanges     = true,
    };

    ASSERT_TRUE(mixer.applyIncrementalPerTrackPatch(patchPlan, []() {
        std::vector<DspNodePtr> nodes;
        nodes.push_back(std::make_unique<GainNode>(u"gain-2"_s, 2.0));
        return nodes;
    }));

    ProcessingBuffer afterPatch{testFormat(), 0};
    EXPECT_EQ(mixer.read(afterPatch, 1), 0);
}
} // namespace Fooyin::Testing
