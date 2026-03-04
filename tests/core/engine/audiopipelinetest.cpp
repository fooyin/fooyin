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

#include "core/engine/pipeline/audiopipeline.h"
#include "core/engine/dsp/dspregistry.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

constexpr auto SampleRate = 100;
constexpr auto Channels   = 1;

namespace Fooyin::Testing {
namespace {
AudioFormat testFormat()
{
    return {SampleFormat::F64, SampleRate, Channels};
}

class FakeAudioOutput : public AudioOutput
{
public:
    explicit FakeAudioOutput(int bufferFrames = 64)
        : m_bufferFrames{std::max(1, bufferFrames)}
    { }

    bool init(const AudioFormat& format) override
    {
        const std::scoped_lock lock{m_mutex};
        m_format      = format;
        m_initialised = format.isValid();
        return m_initialised;
    }

    void uninit() override
    {
        const std::scoped_lock lock{m_mutex};
        m_initialised  = false;
        m_started      = false;
        m_queuedFrames = 0;
    }

    void reset() override
    {
        const std::scoped_lock lock{m_mutex};
        m_queuedFrames = 0;
    }

    void drain() override
    {
        const std::scoped_lock lock{m_mutex};
        m_queuedFrames = 0;
    }

    void start() override
    {
        const std::scoped_lock lock{m_mutex};
        ++m_startCalls;
        if(m_writeCalls == 0) {
            m_startedBeforeFirstWrite = true;
        }
        m_started = true;
    }

    [[nodiscard]] bool initialised() const override
    {
        const std::scoped_lock lock{m_mutex};
        return m_initialised;
    }

    [[nodiscard]] QString device() const override
    {
        const std::scoped_lock lock{m_mutex};
        return m_device;
    }

    OutputState currentState() override
    {
        const std::scoped_lock lock{m_mutex};
        return {.freeFrames = m_freeFrames, .queuedFrames = m_queuedFrames, .delay = m_delaySeconds};
    }

    [[nodiscard]] int bufferSize() const override
    {
        return m_bufferFrames;
    }

    [[nodiscard]] OutputDevices getAllDevices(bool) override
    {
        return {};
    }

    int write(std::span<const std::byte> data, int frameCount) override
    {
        const std::scoped_lock lock{m_mutex};
        if(!m_initialised) {
            return 0;
        }

        const int bytesPerFrame = m_format.bytesPerFrame();
        if(bytesPerFrame <= 0) {
            return 0;
        }

        const int availableFrames = static_cast<int>(data.size() / static_cast<size_t>(bytesPerFrame));
        const int requestedFrames = std::max(0, std::min(frameCount, availableFrames));
        if(requestedFrames <= 0 || m_freeFrames <= 0) {
            return 0;
        }

        int writtenFrames = requestedFrames;
        writtenFrames     = std::min(writtenFrames, m_freeFrames);
        if(m_maxWriteFrames > 0) {
            writtenFrames = std::min(writtenFrames, m_maxWriteFrames);
        }

        if(writtenFrames <= 0) {
            return 0;
        }

        ++m_writeCalls;
        m_totalFramesWritten += writtenFrames;
        m_lastWriteFrames = writtenFrames;
        m_writeCv.notify_all();
        return writtenFrames;
    }

    void setPaused(bool pause) override
    {
        const std::scoped_lock lock{m_mutex};
        m_paused = pause;
    }

    void setVolume(double volume) override
    {
        const std::scoped_lock lock{m_mutex};
        m_volume = volume;
    }

    void setDevice(const QString& device) override
    {
        const std::scoped_lock lock{m_mutex};
        m_device = device;
    }

    [[nodiscard]] AudioFormat format() const override
    {
        const std::scoped_lock lock{m_mutex};
        return m_format;
    }

    [[nodiscard]] QString error() const override
    {
        const std::scoped_lock lock{m_mutex};
        return m_error;
    }

    void setFreeFrames(int freeFrames)
    {
        const std::scoped_lock lock{m_mutex};
        m_freeFrames = std::max(0, freeFrames);
    }

    void setMaxWriteFrames(int maxWriteFrames)
    {
        const std::scoped_lock lock{m_mutex};
        m_maxWriteFrames = maxWriteFrames;
    }

    [[nodiscard]] int writeCallCount() const
    {
        const std::scoped_lock lock{m_mutex};
        return m_writeCalls;
    }

    [[nodiscard]] int startCallCount() const
    {
        const std::scoped_lock lock{m_mutex};
        return m_startCalls;
    }

    [[nodiscard]] bool startedBeforeFirstWrite() const
    {
        const std::scoped_lock lock{m_mutex};
        return m_startedBeforeFirstWrite;
    }

    [[nodiscard]] int totalFramesWritten() const
    {
        const std::scoped_lock lock{m_mutex};
        return m_totalFramesWritten;
    }

    bool waitForWritesAtLeast(int minimumWrites, std::chrono::milliseconds timeout)
    {
        std::unique_lock lock{m_mutex};
        return m_writeCv.wait_for(lock, timeout, [this, minimumWrites]() { return m_writeCalls >= minimumWrites; });
    }

    bool waitForFramesWrittenAtLeast(int minimumFrames, std::chrono::milliseconds timeout)
    {
        std::unique_lock lock{m_mutex};
        return m_writeCv.wait_for(lock, timeout,
                                  [this, minimumFrames]() { return m_totalFramesWritten >= minimumFrames; });
    }

private:
    mutable std::mutex m_mutex;
    std::condition_variable m_writeCv;
    AudioFormat m_format;
    QString m_device;
    QString m_error;
    int m_bufferFrames{64};
    int m_freeFrames{64};
    int m_queuedFrames{0};
    int m_maxWriteFrames{0};
    int m_writeCalls{0};
    int m_startCalls{0};
    int m_totalFramesWritten{0};
    int m_lastWriteFrames{0};
    double m_delaySeconds{0.0};
    double m_volume{1.0};
    bool m_initialised{false};
    bool m_started{false};
    bool m_startedBeforeFirstWrite{false};
    bool m_paused{false};
};

class RateTagDsp final : public DspNode
{
public:
    explicit RateTagDsp(int outputSampleRate)
        : m_outputSampleRate{std::max(1, outputSampleRate)}
    { }

    QString name() const override
    {
        return QStringLiteral("RateTagDsp");
    }

    QString id() const override
    {
        return QStringLiteral("test.dsp.rate_tag");
    }

    void prepare(const AudioFormat&) override { }

    AudioFormat outputFormat(const AudioFormat& input) const override
    {
        AudioFormat output = input;
        if(output.isValid()) {
            output.setSampleFormat(SampleFormat::F64);
            output.setSampleRate(m_outputSampleRate);
        }
        return output;
    }

    void process(ProcessingBufferList& chunks) override
    {
        if(chunks.count() == 0) {
            return;
        }

        ProcessingBufferList output;
        for(size_t i{0}; i < chunks.count(); ++i) {
            const auto* chunk = chunks.item(i);
            if(!chunk || !chunk->isValid() || chunk->frameCount() <= 0) {
                continue;
            }

            const AudioFormat outputFormat = this->outputFormat(chunk->format());
            auto* outChunk                 = output.addItem(outputFormat, chunk->startTimeNs(), chunk->sampleCount());
            if(!outChunk) {
                continue;
            }

            const auto inputData = chunk->constData();
            auto outputData      = outChunk->data();
            std::ranges::copy(inputData, outputData.begin());
            outChunk->setSourceFrameDurationNs(0);
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
    int m_outputSampleRate{0};
};

class ObserveInputRateDsp final : public DspNode
{
public:
    explicit ObserveInputRateDsp(std::shared_ptr<std::atomic<int>> observedRate)
        : m_observedRate{std::move(observedRate)}
    { }

    QString name() const override
    {
        return QStringLiteral("ObserveInputRateDsp");
    }

    QString id() const override
    {
        return QStringLiteral("test.dsp.observe_input_rate");
    }

    void prepare(const AudioFormat&) override { }

    void process(ProcessingBufferList& chunks) override
    {
        if(!m_observedRate) {
            return;
        }

        for(size_t i{0}; i < chunks.count(); ++i) {
            const auto* chunk = chunks.item(i);
            if(!chunk || !chunk->isValid() || chunk->frameCount() <= 0) {
                continue;
            }

            m_observedRate->store(chunk->format().sampleRate(), std::memory_order_release);
            break;
        }
    }

private:
    std::shared_ptr<std::atomic<int>> m_observedRate;
};

AudioStreamPtr makeStream(const std::vector<double>& samples, bool playing)
{
    auto stream = StreamFactory::createStream(testFormat(), 256);
    auto writer = stream->writer();

    if(!samples.empty()) {
        const auto written = writer.write(samples.data(), samples.size());
        EXPECT_EQ(written, samples.size());
    }

    if(playing) {
        stream->applyCommand(AudioStream::Command::Play);
    }

    return stream;
}

bool waitUntil(const std::function<bool()>& predicate, std::chrono::milliseconds timeout = 1500ms)
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while(std::chrono::steady_clock::now() < deadline) {
        if(predicate()) {
            return true;
        }
        std::this_thread::sleep_for(2ms);
    }
    return predicate();
}
} // namespace

TEST(AudioPipelineTest, RaisesNeedsDataSignalWhenMixerUnderruns)
{
    AudioPipeline pipeline;
    auto output   = std::make_unique<FakeAudioOutput>(64);
    auto* backend = output.get();
    backend->setFreeFrames(64);

    pipeline.setOutput(std::move(output));
    pipeline.start();
    ASSERT_TRUE(pipeline.init(testFormat()));

    auto starvedStream  = makeStream({}, true);
    const auto streamId = pipeline.registerStream(starvedStream);

    pipeline.addStream(streamId);
    pipeline.play();

    bool sawNeedsData = false;
    ASSERT_TRUE(waitUntil(
        [&pipeline, &sawNeedsData]() {
            const auto pending = pipeline.drainPendingSignals();
            if(pending.needsData) {
                sawNeedsData = true;
                return true;
            }
            return false;
        },
        1500ms));
    EXPECT_TRUE(sawNeedsData);

    pipeline.stop();
}

TEST(AudioPipelineTest, DrainsPendingOutputWhenBackendWritesPartialFrames)
{
    AudioPipeline pipeline;
    auto output   = std::make_unique<FakeAudioOutput>(64);
    auto* backend = output.get();
    backend->setFreeFrames(64);
    backend->setMaxWriteFrames(1);

    pipeline.setOutput(std::move(output));
    pipeline.start();
    ASSERT_TRUE(pipeline.init(testFormat()));

    auto stream = makeStream({1.0, 1.0, 1.0, 1.0, 1.0, 1.0}, true);
    stream->setEndOfInput();

    const auto streamId = pipeline.registerStream(stream);
    pipeline.addStream(streamId);
    pipeline.play();

    ASSERT_TRUE(backend->waitForFramesWrittenAtLeast(6, 2000ms));
    EXPECT_GE(backend->writeCallCount(), 2);
    EXPECT_GE(backend->totalFramesWritten(), 6);

    pipeline.stop();
}

TEST(AudioPipelineTest, MappedPositionAdvancesWhileDrainingPendingOutput)
{
    AudioPipeline pipeline;
    auto output   = std::make_unique<FakeAudioOutput>(64);
    auto* backend = output.get();
    backend->setFreeFrames(64);
    backend->setMaxWriteFrames(1);

    pipeline.setOutput(std::move(output));
    pipeline.start();
    ASSERT_TRUE(pipeline.init(testFormat()));

    auto stream = makeStream({1.0, 1.0, 1.0, 1.0, 1.0, 1.0}, true);
    stream->setEndOfInput();

    const auto streamId = pipeline.registerStream(stream);
    pipeline.addStream(streamId);
    pipeline.play();

    ASSERT_TRUE(backend->waitForFramesWrittenAtLeast(6, 2000ms));
    ASSERT_TRUE(waitUntil(
        [&pipeline]() {
            const auto status = pipeline.currentStatus();
            return status.positionIsMapped && status.renderedSegment.valid && status.position >= 50;
        },
        2000ms));

    const auto status = pipeline.currentStatus();
    EXPECT_TRUE(status.positionIsMapped);
    EXPECT_TRUE(status.renderedSegment.valid);
    EXPECT_GE(status.position, 50U);
    EXPECT_GE(status.renderedSegment.sourceEndMs, status.renderedSegment.sourceStartMs);

    pipeline.stop();
}

TEST(AudioPipelineTest, ExecuteTransitionRejectsNullStream)
{
    AudioPipeline pipeline;
    const AudioPipeline::TransitionResult result = pipeline.executeTransition({});

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.streamId, InvalidStreamId);
}

TEST(AudioPipelineTest, ExecuteTransitionGaplessSucceedsForValidStream)
{
    AudioPipeline pipeline;

    AudioPipeline::TransitionRequest request;
    request.type   = AudioPipeline::TransitionType::Gapless;
    request.stream = makeStream({0.25, 0.5, 0.75}, false);

    const AudioPipeline::TransitionResult result = pipeline.executeTransition(request);
    EXPECT_TRUE(result.success);
    EXPECT_NE(result.streamId, InvalidStreamId);
}

TEST(AudioPipelineTest, ExecuteTransitionCanSkipStartingFadeOut)
{
    AudioPipeline pipeline;

    auto first             = makeStream({0.1, 0.1, 0.1, 0.1}, true);
    const StreamId firstId = pipeline.registerStream(first);
    pipeline.addStream(firstId);
    ASSERT_EQ(first->state(), AudioStream::State::Playing);

    AudioPipeline::TransitionRequest request;
    request.type             = AudioPipeline::TransitionType::Crossfade;
    request.stream           = makeStream({0.3, 0.3, 0.3, 0.3}, true);
    request.fadeOutMs        = 120;
    request.fadeInMs         = 120;
    request.skipFadeOutStart = true;

    const AudioPipeline::TransitionResult result = pipeline.executeTransition(request);
    EXPECT_TRUE(result.success);
    EXPECT_NE(result.streamId, InvalidStreamId);
    EXPECT_EQ(first->state(), AudioStream::State::Playing);
}

TEST(AudioPipelineTest, ExecuteTransitionSkipFadeOutPreservesExistingFadeCurve)
{
    AudioPipeline pipeline;

    auto first = makeStream({0.1, 0.1, 0.1, 0.1}, true);
    first->setFadeCurve(Engine::FadeCurve::Linear);
    first->applyCommand(AudioStream::Command::StartFadeOut, 1000);
    ASSERT_EQ(first->state(), AudioStream::State::FadingOut);

    const StreamId firstId = pipeline.registerStream(first);
    pipeline.addStream(firstId);

    AudioPipeline::TransitionRequest request;
    request.type             = AudioPipeline::TransitionType::Crossfade;
    request.stream           = makeStream({0.3, 0.3, 0.3, 0.3}, true);
    request.fadeOutMs        = 120;
    request.fadeInMs         = 120;
    request.skipFadeOutStart = true;

    const AudioPipeline::TransitionResult result = pipeline.executeTransition(request);
    EXPECT_TRUE(result.success);
    EXPECT_NE(result.streamId, InvalidStreamId);
    EXPECT_EQ(first->state(), AudioStream::State::FadingOut);

    // If skipFadeOutStart preserves the current fade curve, half-way through a
    // linear 1s fade at 100 Hz we expect ~0.5 gain (not ~0.707 from sine).
    const auto ramp = first->calculateFadeGainRamp(50, SampleRate);
    EXPECT_TRUE(ramp.active);
    EXPECT_NEAR(ramp.endGain, 0.5, 0.05);
}

TEST(AudioPipelineTest, ExecuteTransitionClearsExistingOrphanBeforeGaplessHandoff)
{
    AudioPipeline pipeline;
    auto output = std::make_unique<FakeAudioOutput>(64);
    pipeline.setOutput(std::move(output));
    pipeline.start();
    ASSERT_TRUE(pipeline.init(testFormat()));

    auto first             = makeStream({0.1, 0.1, 0.1, 0.1}, true);
    const StreamId firstId = pipeline.registerStream(first);
    pipeline.addStream(firstId);
    pipeline.play();

    AudioPipeline::TransitionRequest crossfade;
    crossfade.type                                        = AudioPipeline::TransitionType::Crossfade;
    crossfade.stream                                      = makeStream({0.2, 0.2, 0.2, 0.2}, true);
    crossfade.fadeOutMs                                   = 80;
    crossfade.fadeInMs                                    = 80;
    const AudioPipeline::TransitionResult crossfadeResult = pipeline.executeTransition(crossfade);
    ASSERT_TRUE(crossfadeResult.success);
    ASSERT_NE(crossfadeResult.streamId, InvalidStreamId);
    ASSERT_TRUE(pipeline.hasOrphanStream());

    AudioPipeline::TransitionRequest request;
    request.type                                 = AudioPipeline::TransitionType::Gapless;
    request.stream                               = makeStream({0.3, 0.3, 0.3, 0.3}, false);
    const AudioPipeline::TransitionResult result = pipeline.executeTransition(request);

    EXPECT_TRUE(result.success);
    EXPECT_NE(result.streamId, InvalidStreamId);
    EXPECT_FALSE(pipeline.hasOrphanStream());

    pipeline.stop();
}

TEST(AudioPipelineTest, MappedPositionIsMonotonicDuringSteadyDrain)
{
    AudioPipeline pipeline;
    auto output   = std::make_unique<FakeAudioOutput>(64);
    auto* backend = output.get();
    backend->setFreeFrames(64);
    backend->setMaxWriteFrames(1);

    pipeline.setOutput(std::move(output));
    pipeline.start();
    ASSERT_TRUE(pipeline.init(testFormat()));

    auto stream = makeStream(std::vector<double>(256, 0.25), true);
    stream->setEndOfInput();

    const auto streamId = pipeline.registerStream(stream);
    pipeline.addStream(streamId);
    pipeline.play();

    ASSERT_TRUE(backend->waitForWritesAtLeast(1, 1500ms));

    std::vector<uint64_t> mappedPositions;
    const auto deadline = std::chrono::steady_clock::now() + 1000ms;
    while(std::chrono::steady_clock::now() < deadline) {
        const auto status = pipeline.currentStatus();
        if(status.positionIsMapped && status.renderedSegment.valid) {
            mappedPositions.push_back(status.position);
        }
        std::this_thread::sleep_for(5ms);
    }

    ASSERT_GE(mappedPositions.size(), 5U);
    for(size_t i{1}; i < mappedPositions.size(); ++i) {
        EXPECT_GE(mappedPositions[i], mappedPositions[i - 1]);
    }

    pipeline.stop();
}

TEST(AudioPipelineTest, StartsOutputOnlyAfterFirstSuccessfulWrite)
{
    AudioPipeline pipeline;
    auto output   = std::make_unique<FakeAudioOutput>(64);
    auto* backend = output.get();
    backend->setFreeFrames(64);

    pipeline.setOutput(std::move(output));
    pipeline.start();
    ASSERT_TRUE(pipeline.init(testFormat()));

    auto stream = makeStream(std::vector<double>(256, 0.25), true);
    stream->setEndOfInput();
    const auto streamId = pipeline.registerStream(stream);
    pipeline.addStream(streamId);
    pipeline.play();

    ASSERT_TRUE(backend->waitForWritesAtLeast(1, 1500ms));
    ASSERT_TRUE(waitUntil([backend]() { return backend->startCallCount() >= 1; }, 1000ms));
    EXPECT_FALSE(backend->startedBeforeFirstWrite());

    pipeline.stop();
}

TEST(AudioPipelineTest, SwitchToStreamWhilePausedClearsMappedStateAndReanchorsPosition)
{
    AudioPipeline pipeline;
    auto output   = std::make_unique<FakeAudioOutput>(64);
    auto* backend = output.get();
    backend->setFreeFrames(64);

    pipeline.setOutput(std::move(output));
    pipeline.start();
    ASSERT_TRUE(pipeline.init(testFormat()));

    auto first = makeStream(std::vector<double>(256, 0.4), true);
    first->setEndOfInput();
    const auto firstId = pipeline.registerStream(first);
    pipeline.addStream(firstId);
    pipeline.play();

    ASSERT_TRUE(backend->waitForWritesAtLeast(1, 1500ms));

    pipeline.pause();
    ASSERT_TRUE(
        waitUntil([&pipeline]() { return pipeline.currentStatus().state == PipelinePlaybackState::Paused; }, 1000ms));

    auto replacement = makeStream(std::vector<double>(64, 0.7), false);
    replacement->setPosition(20); // 200 ms @ 100 Hz mono
    const uint64_t expectedPositionMs = replacement->positionMs();
    const auto replacementId          = pipeline.registerStream(replacement);

    pipeline.switchToStream(replacementId);

    ASSERT_TRUE(waitUntil(
        [&pipeline, expectedPositionMs]() {
            const auto status = pipeline.currentStatus();
            return status.state == PipelinePlaybackState::Paused && !status.positionIsMapped
                && !status.renderedSegment.valid && status.position >= expectedPositionMs;
        },
        1500ms));

    const auto status = pipeline.currentStatus();
    EXPECT_EQ(status.state, PipelinePlaybackState::Paused);
    EXPECT_FALSE(status.positionIsMapped);
    EXPECT_FALSE(status.renderedSegment.valid);
    EXPECT_EQ(status.position, expectedPositionMs);

    pipeline.stop();
}

TEST(AudioPipelineTest, ResetStreamForSeekClearsMappedSegmentAndPreservesPlayingState)
{
    AudioPipeline pipeline;
    auto output   = std::make_unique<FakeAudioOutput>(64);
    auto* backend = output.get();
    backend->setFreeFrames(64);

    pipeline.setOutput(std::move(output));
    pipeline.start();
    ASSERT_TRUE(pipeline.init(testFormat()));

    auto stream = makeStream(std::vector<double>(256, 0.4), true);
    stream->setEndOfInput();
    const auto streamId = pipeline.registerStream(stream);
    pipeline.addStream(streamId);
    pipeline.play();

    ASSERT_TRUE(waitUntil(
        [&pipeline]() {
            const auto status = pipeline.currentStatus();
            return status.state == PipelinePlaybackState::Playing && status.positionIsMapped
                && status.renderedSegment.valid;
        },
        1500ms));

    pipeline.resetStreamForSeek(streamId);

    const auto status = pipeline.currentStatus();
    EXPECT_EQ(status.state, PipelinePlaybackState::Playing);
    EXPECT_FALSE(status.positionIsMapped);
    EXPECT_FALSE(status.renderedSegment.valid);

    pipeline.stop();
}

TEST(AudioPipelineTest, CrossfadeTransitionWhilePausedClearsMappedStateAndAnchorsPosition)
{
    AudioPipeline pipeline;
    auto output   = std::make_unique<FakeAudioOutput>(64);
    auto* backend = output.get();
    backend->setFreeFrames(64);

    pipeline.setOutput(std::move(output));
    pipeline.start();
    ASSERT_TRUE(pipeline.init(testFormat()));

    auto first         = makeStream(std::vector<double>(256, 0.3), true);
    const auto firstId = pipeline.registerStream(first);
    pipeline.addStream(firstId);
    pipeline.play();

    ASSERT_TRUE(backend->waitForWritesAtLeast(1, 1500ms));

    pipeline.pause();
    ASSERT_TRUE(
        waitUntil([&pipeline]() { return pipeline.currentStatus().state == PipelinePlaybackState::Paused; }, 1000ms));

    auto replacement = makeStream(std::vector<double>(64, 0.8), true);
    replacement->setPosition(30); // 300 ms @ 100 Hz mono
    const uint64_t expectedPositionMs = replacement->positionMs();

    AudioPipeline::TransitionRequest request;
    request.type      = AudioPipeline::TransitionType::Crossfade;
    request.stream    = replacement;
    request.fadeOutMs = 80;
    request.fadeInMs  = 80;

    const auto result = pipeline.executeTransition(request);
    ASSERT_TRUE(result.success);
    ASSERT_NE(result.streamId, InvalidStreamId);

    const auto statusAfter = pipeline.currentStatus();
    EXPECT_EQ(statusAfter.state, PipelinePlaybackState::Paused);
    EXPECT_EQ(statusAfter.position, expectedPositionMs);
    EXPECT_FALSE(statusAfter.positionIsMapped);
    EXPECT_FALSE(statusAfter.renderedSegment.valid);

    pipeline.stop();
}

TEST(AudioPipelineTest, GaplessTransitionLiveHandoffAnchorsToAudibleStart)
{
    AudioPipeline pipeline;
    auto output   = std::make_unique<FakeAudioOutput>(64);
    auto* backend = output.get();
    backend->setFreeFrames(64);

    pipeline.setOutput(std::move(output));
    pipeline.start();
    ASSERT_TRUE(pipeline.init(testFormat()));

    auto first         = makeStream(std::vector<double>(256, 0.3), true);
    const auto firstId = pipeline.registerStream(first);
    pipeline.addStream(firstId);
    pipeline.play();
    ASSERT_TRUE(backend->waitForWritesAtLeast(1, 1500ms));

    backend->setFreeFrames(0);

    auto replacement = makeStream(std::vector<double>(200, 0.8), false);
    replacement->setPosition(200); // 2000 ms @ 100 Hz mono, fully prefilled in buffer

    AudioPipeline::TransitionRequest request;
    request.type   = AudioPipeline::TransitionType::Gapless;
    request.stream = replacement;

    const auto result = pipeline.executeTransition(request);
    ASSERT_TRUE(result.success);
    ASSERT_NE(result.streamId, InvalidStreamId);

    const auto statusAfter = pipeline.currentStatus();
    EXPECT_EQ(statusAfter.state, PipelinePlaybackState::Playing);
    EXPECT_EQ(statusAfter.position, 0U);
    EXPECT_FALSE(statusAfter.positionIsMapped);
    EXPECT_FALSE(statusAfter.renderedSegment.valid);

    pipeline.stop();
}

TEST(AudioPipelineTest, CrossfadeTransitionWithoutTimelineReanchorPreservesCurrentPositionSnapshot)
{
    AudioPipeline pipeline;
    auto output   = std::make_unique<FakeAudioOutput>(64);
    auto* backend = output.get();
    backend->setFreeFrames(64);

    pipeline.setOutput(std::move(output));
    pipeline.start();
    ASSERT_TRUE(pipeline.init(testFormat()));

    auto first = makeStream(std::vector<double>(256, 0.3), true);
    first->setEndOfInput();
    const auto firstId = pipeline.registerStream(first);
    pipeline.addStream(firstId);
    pipeline.play();

    ASSERT_TRUE(waitUntil(
        [&pipeline]() {
            const auto status = pipeline.currentStatus();
            return status.state == PipelinePlaybackState::Playing && status.positionIsMapped
                && status.renderedSegment.valid && status.position > 0;
        },
        1500ms));

    const auto statusBefore = pipeline.currentStatus();
    ASSERT_GT(statusBefore.position, 0U);
    ASSERT_TRUE(statusBefore.positionIsMapped);
    ASSERT_TRUE(statusBefore.renderedSegment.valid);

    auto replacement = makeStream(std::vector<double>(256, 0.8), false);
    replacement->setPosition(200); // 2000 ms @ 100 Hz mono
    const uint64_t replacementPosMs = replacement->positionMs();

    AudioPipeline::TransitionRequest request;
    request.type             = AudioPipeline::TransitionType::Crossfade;
    request.stream           = replacement;
    request.fadeOutMs        = 80;
    request.fadeInMs         = 80;
    request.reanchorTimeline = false;

    const auto result = pipeline.executeTransition(request);
    ASSERT_TRUE(result.success);
    ASSERT_NE(result.streamId, InvalidStreamId);

    const auto statusAfter = pipeline.currentStatus();
    EXPECT_EQ(statusAfter.state, PipelinePlaybackState::Playing);
    EXPECT_GT(statusAfter.position, 0U);
    EXPECT_TRUE(statusAfter.positionIsMapped);
    EXPECT_TRUE(statusAfter.renderedSegment.valid);
    EXPECT_GE(statusAfter.position, statusBefore.position);
    EXPECT_NE(statusAfter.position, replacementPosMs);

    pipeline.stop();
}

TEST(AudioPipelineTest, GaplessTransitionLiveHandoffMarksPreviousPrimaryEndOfInput)
{
    AudioPipeline pipeline;
    auto output   = std::make_unique<FakeAudioOutput>(64);
    auto* backend = output.get();
    backend->setFreeFrames(64);

    pipeline.setOutput(std::move(output));
    pipeline.start();
    ASSERT_TRUE(pipeline.init(testFormat()));

    auto first = makeStream(std::vector<double>(256, 0.3), true);
    ASSERT_FALSE(first->endOfInput());
    const auto firstId = pipeline.registerStream(first);
    pipeline.addStream(firstId);
    pipeline.play();
    ASSERT_TRUE(backend->waitForWritesAtLeast(1, 1500ms));

    auto replacement = makeStream(std::vector<double>(200, 0.8), false);

    AudioPipeline::TransitionRequest request;
    request.type   = AudioPipeline::TransitionType::Gapless;
    request.stream = replacement;

    const auto result = pipeline.executeTransition(request);
    ASSERT_TRUE(result.success);
    ASSERT_NE(result.streamId, InvalidStreamId);

    EXPECT_TRUE(first->endOfInput());

    pipeline.stop();
}

TEST(AudioPipelineTest, GaplessTransitionWithoutTimelineReanchorPreservesCurrentPositionSnapshot)
{
    AudioPipeline pipeline;
    auto output   = std::make_unique<FakeAudioOutput>(64);
    auto* backend = output.get();
    backend->setFreeFrames(64);

    pipeline.setOutput(std::move(output));
    pipeline.start();
    ASSERT_TRUE(pipeline.init(testFormat()));

    auto first = makeStream(std::vector<double>(256, 0.3), true);
    first->setEndOfInput();
    const auto firstId = pipeline.registerStream(first);
    pipeline.addStream(firstId);
    pipeline.play();

    ASSERT_TRUE(waitUntil(
        [&pipeline]() {
            const auto status = pipeline.currentStatus();
            return status.state == PipelinePlaybackState::Playing && status.positionIsMapped
                && status.renderedSegment.valid && status.position > 0;
        },
        1500ms));

    const auto statusBefore = pipeline.currentStatus();
    ASSERT_GT(statusBefore.position, 0U);
    ASSERT_TRUE(statusBefore.positionIsMapped);
    ASSERT_TRUE(statusBefore.renderedSegment.valid);

    auto replacement = makeStream(std::vector<double>(256, 0.8), false);
    replacement->setPosition(200); // 2000 ms @ 100 Hz mono

    AudioPipeline::TransitionRequest request;
    request.type                    = AudioPipeline::TransitionType::Gapless;
    request.stream                  = replacement;
    request.signalCurrentEndOfInput = false;
    request.reanchorTimeline        = false;

    const auto result = pipeline.executeTransition(request);
    ASSERT_TRUE(result.success);
    ASSERT_NE(result.streamId, InvalidStreamId);

    const auto statusAfter = pipeline.currentStatus();
    EXPECT_EQ(statusAfter.state, PipelinePlaybackState::Playing);
    EXPECT_GT(statusAfter.position, 0U);
    EXPECT_TRUE(statusAfter.positionIsMapped);
    EXPECT_TRUE(statusAfter.renderedSegment.valid);

    pipeline.stop();
}

TEST(AudioPipelineTest, StopPlaybackClearsMappedSegmentAndResetsPosition)
{
    AudioPipeline pipeline;
    auto output   = std::make_unique<FakeAudioOutput>(64);
    auto* backend = output.get();
    backend->setFreeFrames(64);

    pipeline.setOutput(std::move(output));
    pipeline.start();
    ASSERT_TRUE(pipeline.init(testFormat()));

    auto stream = makeStream(std::vector<double>(256, 0.2), true);
    stream->setEndOfInput();
    const auto streamId = pipeline.registerStream(stream);
    pipeline.addStream(streamId);
    pipeline.play();

    ASSERT_TRUE(waitUntil(
        [&pipeline]() {
            const auto status = pipeline.currentStatus();
            return status.positionIsMapped && status.renderedSegment.valid;
        },
        1500ms));

    pipeline.stopPlayback();
    ASSERT_TRUE(
        waitUntil([&pipeline]() { return pipeline.currentStatus().state == PipelinePlaybackState::Stopped; }, 1000ms));

    const auto status = pipeline.currentStatus();
    EXPECT_EQ(status.position, 0U);
    EXPECT_FALSE(status.positionIsMapped);
    EXPECT_FALSE(status.renderedSegment.valid);

    pipeline.stop();
}

TEST(AudioPipelineTest, UpdatesMasterInputRateWhenPerTrackChainChangesDuringPlayback)
{
    AudioPipeline pipeline;
    auto output   = std::make_unique<FakeAudioOutput>(64);
    auto* backend = output.get();
    backend->setFreeFrames(64);
    backend->setMaxWriteFrames(1);

    DspRegistry registry;
    registry.registerDsp({.id      = QStringLiteral("test.dsp.rate_tag"),
                          .name    = QStringLiteral("RateTag"),
                          .factory = []() { return std::make_unique<RateTagDsp>(SampleRate * 2); }});

    pipeline.setDspRegistry(&registry);
    pipeline.setOutput(std::move(output));
    pipeline.start();
    ASSERT_TRUE(pipeline.init(testFormat()));

    const auto observedRate    = std::make_shared<std::atomic<int>>(0);
    const auto makeMasterNodes = [observedRate]() {
        std::vector<DspNodePtr> nodes;
        nodes.push_back(std::make_unique<ObserveInputRateDsp>(observedRate));
        return nodes;
    };

    auto stream         = makeStream(std::vector<double>(64, 0.5), true);
    const auto streamId = pipeline.registerStream(stream);
    pipeline.addStream(streamId);
    pipeline.play();

    const auto pushSamples = [&stream](int frameCount) {
        const std::vector<double> refill(static_cast<size_t>(std::max(1, frameCount)), 0.5);
        const auto deadline = std::chrono::steady_clock::now() + 1000ms;

        while(std::chrono::steady_clock::now() < deadline) {
            auto writer = stream->writer();
            if(writer.write(refill.data(), refill.size()) > 0U) {
                return true;
            }

            std::this_thread::sleep_for(2ms);
        }

        return false;
    };

    Engine::DspDefinition perTrackRateTag;
    perTrackRateTag.id   = QStringLiteral("test.dsp.rate_tag");
    perTrackRateTag.name = QStringLiteral("RateTag");

    Engine::DspChain perTrack;
    perTrack.push_back(perTrackRateTag);

    pipeline.setDspChain(makeMasterNodes(), perTrack, testFormat());
    ASSERT_TRUE(pushSamples(64));
    const bool sawDoubledRate = waitUntil(
        [&observedRate]() { return observedRate->load(std::memory_order_acquire) == SampleRate * 2; }, 2000ms);
    ASSERT_TRUE(sawDoubledRate) << "expected doubled rate, observed=" << observedRate->load(std::memory_order_acquire)
                                << " streamState=" << static_cast<int>(stream->state())
                                << " bufferedSamples=" << stream->bufferedSamples();

    pipeline.setDspChain(makeMasterNodes(), {}, testFormat());
    ASSERT_TRUE(pushSamples(64));
    ASSERT_TRUE(
        waitUntil([&observedRate]() { return observedRate->load(std::memory_order_acquire) == SampleRate; }, 2000ms));

    pipeline.stop();
}

TEST(AudioPipelineTest, SetDspChainSoftUpdatePreservesMappedSegmentState)
{
    AudioPipeline pipeline;
    auto output   = std::make_unique<FakeAudioOutput>(64);
    auto* backend = output.get();
    backend->setFreeFrames(64);

    pipeline.setOutput(std::move(output));
    pipeline.start();
    ASSERT_TRUE(pipeline.init(testFormat()));

    auto stream = makeStream(std::vector<double>(256, 0.2), true);
    stream->setEndOfInput();

    const auto streamId = pipeline.registerStream(stream);
    pipeline.addStream(streamId);
    pipeline.play();

    ASSERT_TRUE(waitUntil(
        [&pipeline]() {
            const auto status = pipeline.currentStatus();
            return status.positionIsMapped && status.renderedSegment.valid;
        },
        2000ms));

    pipeline.pause();
    ASSERT_TRUE(
        waitUntil([&pipeline]() { return pipeline.currentStatus().state == PipelinePlaybackState::Paused; }, 1000ms));

    const auto statusBeforeReconfig = pipeline.currentStatus();
    ASSERT_TRUE(statusBeforeReconfig.positionIsMapped);
    ASSERT_TRUE(statusBeforeReconfig.renderedSegment.valid);

    std::vector<DspNodePtr> emptyMaster;
    const Engine::DspChain emptyPerTrack;
    pipeline.setDspChain(std::move(emptyMaster), emptyPerTrack, testFormat());

    const auto statusAfterReconfig = pipeline.currentStatus();
    EXPECT_TRUE(statusAfterReconfig.positionIsMapped);
    EXPECT_TRUE(statusAfterReconfig.renderedSegment.valid);
    EXPECT_EQ(statusAfterReconfig.timelineEpoch, statusBeforeReconfig.timelineEpoch);

    pipeline.stop();
}

TEST(AudioPipelineTest, SetDspChainHardUpdateClearsMappedSegmentState)
{
    AudioPipeline pipeline;
    auto output   = std::make_unique<FakeAudioOutput>(64);
    auto* backend = output.get();
    backend->setFreeFrames(64);

    DspRegistry registry;
    registry.registerDsp({.id      = QStringLiteral("test.dsp.rate_tag"),
                          .name    = QStringLiteral("RateTag"),
                          .factory = []() { return std::make_unique<RateTagDsp>(SampleRate * 2); }});
    pipeline.setDspRegistry(&registry);

    pipeline.setOutput(std::move(output));
    pipeline.start();
    ASSERT_TRUE(pipeline.init(testFormat()));

    auto stream = makeStream(std::vector<double>(256, 0.2), true);
    stream->setEndOfInput();

    const auto streamId = pipeline.registerStream(stream);
    pipeline.addStream(streamId);
    pipeline.play();

    ASSERT_TRUE(waitUntil(
        [&pipeline]() {
            const auto status = pipeline.currentStatus();
            return status.positionIsMapped && status.renderedSegment.valid;
        },
        2000ms));

    pipeline.pause();
    ASSERT_TRUE(
        waitUntil([&pipeline]() { return pipeline.currentStatus().state == PipelinePlaybackState::Paused; }, 1000ms));

    std::vector<DspNodePtr> emptyMaster;
    Engine::DspDefinition perTrackRateTag;
    perTrackRateTag.id   = QStringLiteral("test.dsp.rate_tag");
    perTrackRateTag.name = QStringLiteral("RateTag");
    Engine::DspChain perTrack;
    perTrack.push_back(perTrackRateTag);

    pipeline.setDspChain(std::move(emptyMaster), perTrack, testFormat());

    const auto statusAfterReconfig = pipeline.currentStatus();
    EXPECT_FALSE(statusAfterReconfig.positionIsMapped);
    EXPECT_FALSE(statusAfterReconfig.renderedSegment.valid);

    pipeline.stop();
}
} // namespace Fooyin::Testing
