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
        std::scoped_lock lock{m_mutex};
        m_format      = format;
        m_initialised = format.isValid();
        return m_initialised;
    }

    void uninit() override
    {
        std::scoped_lock lock{m_mutex};
        m_initialised  = false;
        m_started      = false;
        m_queuedFrames = 0;
    }

    void reset() override
    {
        std::scoped_lock lock{m_mutex};
        m_queuedFrames = 0;
    }

    void drain() override
    {
        std::scoped_lock lock{m_mutex};
        m_queuedFrames = 0;
    }

    void start() override
    {
        std::scoped_lock lock{m_mutex};
        m_started = true;
    }

    [[nodiscard]] bool initialised() const override
    {
        std::scoped_lock lock{m_mutex};
        return m_initialised;
    }

    [[nodiscard]] QString device() const override
    {
        std::scoped_lock lock{m_mutex};
        return m_device;
    }

    OutputState currentState() override
    {
        std::scoped_lock lock{m_mutex};
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

    int write(const AudioBuffer& buffer) override
    {
        std::scoped_lock lock{m_mutex};
        if(!m_initialised) {
            return 0;
        }

        const int requestedFrames = std::max(0, buffer.frameCount());
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
        m_writeCv.notify_all();
        return writtenFrames;
    }

    void setPaused(bool pause) override
    {
        std::scoped_lock lock{m_mutex};
        m_paused = pause;
    }

    void setVolume(double volume) override
    {
        std::scoped_lock lock{m_mutex};
        m_volume = volume;
    }

    void setDevice(const QString& device) override
    {
        std::scoped_lock lock{m_mutex};
        m_device = device;
    }

    [[nodiscard]] AudioFormat format() const override
    {
        std::scoped_lock lock{m_mutex};
        return m_format;
    }

    [[nodiscard]] QString error() const override
    {
        std::scoped_lock lock{m_mutex};
        return m_error;
    }

    void setFreeFrames(int freeFrames)
    {
        std::scoped_lock lock{m_mutex};
        m_freeFrames = std::max(0, freeFrames);
    }

    void setMaxWriteFrames(int maxWriteFrames)
    {
        std::scoped_lock lock{m_mutex};
        m_maxWriteFrames = maxWriteFrames;
    }

    [[nodiscard]] int writeCallCount() const
    {
        std::scoped_lock lock{m_mutex};
        return m_writeCalls;
    }

    [[nodiscard]] int totalFramesWritten() const
    {
        std::scoped_lock lock{m_mutex};
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
    int m_totalFramesWritten{0};
    double m_delaySeconds{0.0};
    double m_volume{1.0};
    bool m_initialised{false};
    bool m_started{false};
    bool m_paused{false};
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

TEST(AudioPipelineTest, PublishesMappedPositionAfterAudioIsRendered)
{
    AudioPipeline pipeline;
    auto output   = std::make_unique<FakeAudioOutput>(64);
    auto* backend = output.get();
    backend->setFreeFrames(64);

    pipeline.setOutput(std::move(output));
    pipeline.start();
    ASSERT_TRUE(pipeline.init(testFormat()));

    auto stream = makeStream({1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0}, true);
    stream->setEndOfInput();

    const auto streamId = pipeline.registerStream(stream);
    pipeline.addStream(streamId);
    pipeline.play();

    ASSERT_TRUE(waitUntil(
        [&pipeline]() {
            const auto status = pipeline.currentStatus();
            return status.positionIsMapped;
        },
        1500ms));

    const auto status = pipeline.currentStatus();
    EXPECT_TRUE(status.positionIsMapped);
    EXPECT_GE(status.position, 0U);

    pipeline.stop();
    EXPECT_FALSE(pipeline.currentStatus().positionIsMapped);
}
} // namespace Fooyin::Testing
