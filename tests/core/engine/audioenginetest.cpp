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

#include "core/engine/audioengine.h"
#include "core/engine/decode/decodercontext.h"
#include "core/engine/visualisationbackend.h"

#include "core/engine/dsp/dspregistry.h"
#include "core/internalcoresettings.h"

#include <core/coresettings.h>
#include <core/engine/audioloader.h>
#include <utils/settings/settingsmanager.h>

#include <QCoreApplication>
#include <QPointer>
#include <QTemporaryDir>
#include <QVariant>

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

using namespace std::chrono_literals;
using namespace Qt::StringLiterals;

#ifdef FOOYIN_AUDIOENGINE_INCLUDE_SENSITIVE
#define FOOYIN_AUDIOENGINE_REGULAR_TEST(test_suite, test_name) [[maybe_unused]] static void test_suite##_##test_name()
#define FOOYIN_AUDIOENGINE_SENSITIVE_TEST(test_suite, test_name) TEST(test_suite, test_name)
#else
#define FOOYIN_AUDIOENGINE_REGULAR_TEST(test_suite, test_name) TEST(test_suite, test_name)
#define FOOYIN_AUDIOENGINE_SENSITIVE_TEST(test_suite, test_name) [[maybe_unused]] static void test_suite##_##test_name()
#endif

namespace {
struct DecoderStats
{
    std::atomic<int> initCalls{0};
    std::atomic<int> startCalls{0};
    std::atomic<int> stopCalls{0};
    std::atomic<int> seekCalls{0};
    std::atomic<int> bitrate{320};
};

struct OutputStats
{
    std::atomic<int> initCalls{0};
    std::atomic<int> uninitCalls{0};
    std::atomic<int> resetCalls{0};
    std::atomic<int> startCalls{0};
    std::atomic<int> writeCalls{0};
    std::atomic<int> setVolumeCalls{0};
    std::atomic<int> setDeviceCalls{0};
    std::atomic<int> setPausedCalls{0};
    std::atomic<int> disconnectSignals{0};
    std::atomic<int> freeFrames{256};
    std::atomic<int> queuedFrames{0};
    std::atomic<int> delayMs{0};

    void setOutputState(int free, int queued, int delay)
    {
        freeFrames.store(std::max(0, free), std::memory_order_relaxed);
        queuedFrames.store(std::max(0, queued), std::memory_order_relaxed);
        delayMs.store(std::max(0, delay), std::memory_order_relaxed);
    }

    void recordVolume(double volume)
    {
        ++setVolumeCalls;
        const std::scoped_lock lock{mutex};
        lastVolume = volume;
    }

    void recordDevice(const QString& device)
    {
        ++setDeviceCalls;
        const std::scoped_lock lock{mutex};
        lastDevice = device;
    }

    [[nodiscard]] double volume() const
    {
        const std::scoped_lock lock{mutex};
        return lastVolume;
    }

    [[nodiscard]] QString device() const
    {
        const std::scoped_lock lock{mutex};
        return lastDevice;
    }

    void setDisconnectEmitter(std::function<void()> emitter)
    {
        const std::scoped_lock lock{mutex};
        disconnectEmitter = std::move(emitter);
    }

    void emitDisconnect()
    {
        std::function<void()> emitter;
        {
            const std::scoped_lock lock{mutex};
            emitter = disconnectEmitter;
        }

        if(emitter) {
            emitter();
        }
    }

    mutable std::mutex mutex;
    double lastVolume{1.0};
    QString lastDevice;
    std::function<void()> disconnectEmitter;
};

QCoreApplication* ensureCoreApplication()
{
    if(auto* app = QCoreApplication::instance()) {
        return app;
    }

    static int argc{1};
    static char appName[] = "fooyin-audioengine-test";
    static char* argv[]   = {appName, nullptr};
    static QCoreApplication app{argc, argv};
    return &app;
}

bool pumpUntil(const std::function<bool()>& predicate, std::chrono::milliseconds timeout = 2000ms)
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while(std::chrono::steady_clock::now() < deadline) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
        if(predicate()) {
            return true;
        }
        std::this_thread::sleep_for(2ms);
    }
    QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
    return predicate();
}

Fooyin::Engine::PlaybackItem makePlaybackItem(const Fooyin::Track& track, uint64_t itemId)
{
    return {.track = track, .itemId = itemId};
}

QString createDummyAudioFile(QTemporaryDir& tempDir, const QString& fileName)
{
    const QString filePath = tempDir.filePath(fileName);
    QFile file{filePath};
    if(file.open(QIODevice::WriteOnly)) {
        file.write("fooyin-test");
        file.close();
    }
    return filePath;
}

Fooyin::Track makeTrack(const QString& filePath, uint64_t offsetMs, uint64_t durationMs)
{
    Fooyin::Track track{filePath};
    track.setOffset(offsetMs);
    track.setDuration(durationMs);
    track.setBitrate(320);
    return track;
}
} // namespace

namespace Fooyin::Testing {
namespace {
void registerMinimalEngineSettings(SettingsManager& settings, bool enablePauseStopFade, bool enableManualCrossfade)
{
    qRegisterMetaType<Engine::FadingValues>("FadingValues");
    qRegisterMetaType<Engine::CrossfadingValues>("CrossfadingValues");

    settings.createSetting<Settings::Core::PlayMode>(0, QString::fromLatin1(Settings::Core::PlayModeKey));
    settings.createSetting<Settings::Core::StopAfterCurrent>(false, u"Playback/StopAfterCurrent"_s);
    settings.createSetting<Settings::Core::ResetStopAfterCurrent>(false, u"Playback/ResetStopAfterCurrent"_s);
    settings.createSetting<Settings::Core::OutputVolume>(1.0, u"Engine/OutputVolume"_s);
    settings.createSetting<Settings::Core::GaplessPlayback>(false, u"Engine/GaplessPlayback"_s);
    settings.createSetting<Settings::Core::BufferLength>(4000, u"Engine/BufferLength"_s);
    settings.createSetting<Settings::Core::OutputBitDepth>(static_cast<int>(SampleFormat::Unknown),
                                                           u"Engine/OutputBitDepth"_s);
    settings.createSetting<Settings::Core::RGMode>(0, u"Engine/ReplayGainMode"_s);
    settings.createSetting<Settings::Core::RGType>(0, u"Engine/ReplayGainType"_s);
    settings.createSetting<Settings::Core::RGPreAmp>(0.0F, u"Engine/ReplayGainPreAmp"_s);
    settings.createSetting<Settings::Core::NonRGPreAmp>(0.0F, u"Engine/NonReplayGainPreAmp"_s);

    Engine::FadingValues fadingValues;
    fadingValues.pause.out = enablePauseStopFade ? 30 : 0;
    fadingValues.pause.in  = enablePauseStopFade ? 30 : 0;
    fadingValues.stop.out  = enablePauseStopFade ? 30 : 0;
    fadingValues.stop.in   = enablePauseStopFade ? 30 : 0;

    settings.createSetting<Settings::Core::Internal::EngineFading>(enablePauseStopFade, u"Engine/Fading"_s);
    settings.createSetting<Settings::Core::Internal::FadingValues>(QVariant::fromValue(fadingValues),
                                                                   u"Engine/FadingValues"_s);
    Engine::CrossfadingValues crossfadingValues;
    if(enableManualCrossfade) {
        crossfadingValues.manualChange.out = 120;
        crossfadingValues.manualChange.in  = 120;
    }

    settings.createSetting<Settings::Core::Internal::EngineCrossfading>(enableManualCrossfade, u"Engine/Crossfading"_s);
    settings.createSetting<Settings::Core::Internal::CrossfadingValues>(QVariant::fromValue(crossfadingValues),
                                                                        u"Engine/CrossfadingValues"_s);
    settings.createSetting<Settings::Core::Internal::VBRUpdateInterval>(1000, u"Engine/VBRUpdateInterval"_s);
}

class FakeDecoder final : public AudioDecoder
{
public:
    explicit FakeDecoder(std::shared_ptr<DecoderStats> stats)
        : m_stats{std::move(stats)}
    { }

    [[nodiscard]] QStringList extensions() const override
    {
        return {u"fyt"_s};
    }

    [[nodiscard]] bool isSeekable() const override
    {
        return true;
    }

    [[nodiscard]] int bitrate() const override
    {
        return m_stats->bitrate.load(std::memory_order_relaxed);
    }

    std::optional<AudioFormat> init(const AudioSource& source, const Track& track, DecoderOptions options) override
    {
        Q_UNUSED(source)
        Q_UNUSED(options)

        ++m_stats->initCalls;
        m_track = track;
        m_format.setSampleRate(track.filenameExt().contains(u"sr2000"_s) ? 2000 : 1000);
        m_position  = track.offset();
        m_started   = false;
        m_initValid = true;
        return m_format;
    }

    void start() override
    {
        ++m_stats->startCalls;
        m_started = true;
    }

    void stop() override
    {
        ++m_stats->stopCalls;
        m_started = false;
    }

    void seek(uint64_t pos) override
    {
        ++m_stats->seekCalls;
        m_position = pos;
    }

    AudioBuffer readBuffer(size_t bytes) override
    {
        if(!m_initValid || !m_started || bytes == 0) {
            return {};
        }

        const int bytesPerFrame = m_format.bytesPerFrame();
        if(bytesPerFrame <= 0) {
            return {};
        }

        const size_t frameCount = bytes / static_cast<size_t>(bytesPerFrame);
        if(frameCount == 0) {
            return {};
        }

        AudioBuffer buffer{m_format, m_position};
        buffer.resize(frameCount * static_cast<size_t>(bytesPerFrame));

        auto* raw         = reinterpret_cast<double*>(buffer.data());
        const int samples = static_cast<int>(frameCount) * m_format.channelCount();
        for(int i{0}; i < samples; ++i) {
            raw[i] = 0.1;
        }

        m_position += m_format.durationForFrames(static_cast<int>(frameCount));
        return buffer;
    }

private:
    std::shared_ptr<DecoderStats> m_stats;
    Track m_track;
    AudioFormat m_format{SampleFormat::F64, 1000, 2};
    uint64_t m_position{0};
    bool m_started{false};
    bool m_initValid{false};
};

class FakeAudioOutput : public AudioOutput
{
public:
    explicit FakeAudioOutput(std::shared_ptr<OutputStats> stats, int bufferFrames = 256)
        : m_stats{std::move(stats)}
        , m_bufferFrames{std::max(1, bufferFrames)}
    {
        m_stats->setDisconnectEmitter([self = QPointer(this)]() {
            if(self) {
                self->simulateDisconnect();
            }
        });
    }

    bool init(const AudioFormat& format) override
    {
        ++m_stats->initCalls;
        m_format      = format;
        m_initialised = format.isValid();
        return m_initialised;
    }

    void uninit() override
    {
        ++m_stats->uninitCalls;
        m_initialised = false;
    }

    void reset() override
    {
        ++m_stats->resetCalls;
    }

    void drain() override { }

    void start() override
    {
        ++m_stats->startCalls;
    }

    [[nodiscard]] bool initialised() const override
    {
        return m_initialised;
    }

    [[nodiscard]] QString device() const override
    {
        return m_device;
    }

    OutputState currentState() override
    {
        return {.freeFrames   = m_stats->freeFrames.load(std::memory_order_relaxed),
                .queuedFrames = m_stats->queuedFrames.load(std::memory_order_relaxed),
                .delay        = static_cast<double>(m_stats->delayMs.load(std::memory_order_relaxed)) / 1000.0};
    }

    [[nodiscard]] int bufferSize() const override
    {
        return m_bufferFrames;
    }

    [[nodiscard]] OutputDevices getAllDevices(bool isCurrentOutput) override
    {
        Q_UNUSED(isCurrentOutput)
        return {};
    }

    int write(std::span<const std::byte> data, int frameCount) override
    {
        if(!m_initialised || frameCount <= 0) {
            return 0;
        }
        const int bytesPerFrame = m_format.bytesPerFrame();
        if(bytesPerFrame <= 0 || data.size() < static_cast<size_t>(frameCount) * static_cast<size_t>(bytesPerFrame)) {
            return 0;
        }
        ++m_stats->writeCalls;
        return frameCount;
    }

    void setPaused(bool pause) override
    {
        m_paused = pause;
        ++m_stats->setPausedCalls;
    }

    void setVolume(double volume) override
    {
        m_volume = volume;
        m_stats->recordVolume(volume);
    }

    void setDevice(const QString& device) override
    {
        m_device = device;
        m_stats->recordDevice(device);
    }

    [[nodiscard]] AudioFormat format() const override
    {
        return m_format;
    }

    [[nodiscard]] QString error() const override
    {
        return {};
    }

    void simulateDisconnect()
    {
        ++m_stats->disconnectSignals;
        Q_EMIT stateChanged(State::Disconnected);
    }

private:
    std::shared_ptr<OutputStats> m_stats;
    AudioFormat m_format;
    QString m_device;
    int m_bufferFrames{256};
    double m_volume{1.0};
    bool m_initialised{false};
    bool m_paused{false};
};

class FormatShiftDsp final : public DspNode
{
public:
    QString name() const override
    {
        return u"FormatShiftDsp"_s;
    }

    QString id() const override
    {
        return u"test.dsp.format_shift"_s;
    }

    void prepare(const AudioFormat& format) override
    {
        Q_UNUSED(format)
    }

    AudioFormat outputFormat(const AudioFormat& input) const override
    {
        AudioFormat output = input;
        if(output.isValid()) {
            output.setSampleRate(input.sampleRate() * 2);
        }
        return output;
    }

    void process(ProcessingBufferList& chunks) override
    {
        Q_UNUSED(chunks)
    }
};

struct EngineHarness
{
    explicit EngineHarness(bool enablePauseStopFade, bool enableManualCrossfade = false)
        : settingsPath{tempDir.filePath(u"settings.ini"_s)}
        , settings{settingsPath}
        , decoderStats{std::make_shared<DecoderStats>()}
        , outputStats{std::make_shared<OutputStats>()}
        , loader{std::make_shared<AudioLoader>()}
        , visualisationBackend{std::make_shared<VisualisationBackend>()}
        , engine{loader, &settings, &registry, visualisationBackend}
    {
        registerMinimalEngineSettings(settings, enablePauseStopFade, enableManualCrossfade);

        loader->addDecoder(u"FakeDecoder"_s, [stats = decoderStats]() { return std::make_unique<FakeDecoder>(stats); });

        engine.setAudioOutput([stats = outputStats]() { return std::make_unique<FakeAudioOutput>(stats); }, QString{});
    }

    Track createTrack(const QString& fileName, uint64_t offsetMs, uint64_t durationMs)
    {
        const QString filePath = createDummyAudioFile(tempDir, fileName);
        return makeTrack(filePath, offsetMs, durationMs);
    }

    QTemporaryDir tempDir;
    QString settingsPath;
    SettingsManager settings;
    std::shared_ptr<DecoderStats> decoderStats;
    std::shared_ptr<OutputStats> outputStats;
    std::shared_ptr<AudioLoader> loader;
    DspRegistry registry;
    std::shared_ptr<VisualisationBackend> visualisationBackend;
    AudioEngine engine;
};

} // namespace

class AudioEngineTestAccessor
{
public:
    static bool prepareNextTrackImmediate(AudioEngine& engine, const Engine::PlaybackItem& item,
                                          uint64_t prefillTargetMs)
    {
        return engine.prepareNextTrackImmediate(item, prefillTargetMs);
    }

    static void activatePreparedCrossfade(AudioEngine& engine, const Engine::PlaybackItem& item, uint64_t generation,
                                          uint64_t boundaryLeadMs, uint64_t overlapMidpointLeadMs)
    {
        EXPECT_TRUE(engine.m_preparedNext.has_value());
        ASSERT_TRUE(engine.m_preparedNext.has_value());
        ASSERT_EQ(engine.m_preparedNext->item.itemId, item.itemId);
        ASSERT_EQ(engine.m_preparedNext->item.track.filepath(), item.track.filepath());
        ASSERT_TRUE(engine.m_preparedNext->preparedStream);

        engine.m_preparedCrossfadeTransition.active                = true;
        engine.m_preparedCrossfadeTransition.targetTrack           = item.track;
        engine.m_preparedCrossfadeTransition.targetItemId          = item.itemId;
        engine.m_preparedCrossfadeTransition.sourceGeneration      = generation;
        engine.m_preparedCrossfadeTransition.streamId              = engine.m_preparedNext->preparedStream->id();
        engine.m_preparedCrossfadeTransition.boundaryLeadMs        = boundaryLeadMs;
        engine.m_preparedCrossfadeTransition.overlapMidpointLeadMs = overlapMidpointLeadMs;
        engine.m_preparedCrossfadeTransition.bufferedAtArmMs
            = engine.m_preparedNext->preparedStream->bufferedDurationMs();
        engine.m_preparedCrossfadeTransition.boundarySignalled = false;
    }

    static void setUpcomingTrackCandidate(AudioEngine& engine, const Engine::PlaybackItem& item)
    {
        engine.m_upcomingTrackCandidate       = item.track;
        engine.m_upcomingTrackCandidateItemId = item.itemId;
    }

    static void setCrossfadeConfig(AudioEngine& engine, bool enabled, const Engine::CrossfadingValues& values,
                                   Engine::CrossfadeSwitchPolicy switchPolicy)
    {
        engine.m_crossfadeEnabled      = enabled;
        engine.m_crossfadingValues     = values;
        engine.m_crossfadeSwitchPolicy = switchPolicy;
    }

    static void setGaplessEnabled(AudioEngine& engine, bool enabled)
    {
        engine.m_gaplessEnabled = enabled;
    }

    static void setAutoAdvanceState(AudioEngine& engine, uint64_t generation, AutoTransitionMode mode,
                                    bool overlapStartAnchorSeen, bool overlapMidpointAnchorSeen,
                                    bool boundaryAnchorSeen)
    {
        engine.m_autoAdvanceState.generation                  = generation;
        engine.m_autoAdvanceState.mode                        = mode;
        engine.m_autoAdvanceState.overlapStartAnchorSeen      = overlapStartAnchorSeen;
        engine.m_autoAdvanceState.overlapMidpointAnchorSeen   = overlapMidpointAnchorSeen;
        engine.m_autoAdvanceState.boundaryAnchorSeen          = boundaryAnchorSeen;
        engine.m_autoAdvanceState.boundaryPendingUntilAudible = false;
        engine.m_autoAdvanceState.drainPrepareRequested       = false;
    }

    static void tryAutoAdvanceCommit(AudioEngine& engine)
    {
        engine.tryAutoAdvanceCommit();
    }

    static int currentTrackId(const AudioEngine& engine)
    {
        return engine.m_currentTrack.id();
    }

    static uint64_t currentTrackItemId(const AudioEngine& engine)
    {
        return engine.m_currentTrackItemId;
    }

    static uint64_t trackGeneration(const AudioEngine& engine)
    {
        return engine.m_trackGeneration;
    }

    static uint64_t overlapMidpointLeadMs(const AudioEngine& engine)
    {
        return engine.m_preparedCrossfadeTransition.overlapMidpointLeadMs;
    }

    static void deliverPcmFrame(AudioEngine& engine, const PcmFrame& frame)
    {
        engine.onPcmFrameReady(frame);
    }

    static void publishPosition(AudioEngine& engine, uint64_t sourcePositionMs, uint64_t outputDelayMs,
                                double delayToSourceScale, AudioClock::UpdateMode mode, bool emitNow)
    {
        engine.publishPosition(sourcePositionMs, outputDelayMs, delayToSourceScale, mode, emitNow);
    }

    static uint64_t visualisationCurrentTimeMs(const AudioEngine& engine)
    {
        return engine.m_visualisationBackend ? engine.m_visualisationBackend->currentTimeMs() : 0;
    }

    static void installPreparedCrossfade(AudioEngine& engine, const Engine::PlaybackItem& item, uint64_t generation,
                                         const AudioStreamPtr& preparedStream, const AudioFormat& format)
    {
        NextTrackPreparationState preparedState;
        preparedState.item           = item;
        preparedState.format         = format;
        preparedState.preparedStream = preparedStream;

        engine.m_preparedNext                                 = std::move(preparedState);
        engine.m_preparedCrossfadeTransition.active           = true;
        engine.m_preparedCrossfadeTransition.targetTrack      = item.track;
        engine.m_preparedCrossfadeTransition.targetItemId     = item.itemId;
        engine.m_preparedCrossfadeTransition.sourceGeneration = generation;
        engine.m_preparedCrossfadeTransition.streamId         = preparedStream ? preparedStream->id() : InvalidStreamId;
        engine.m_preparedCrossfadeTransition.boundaryLeadMs   = 250;
        engine.m_preparedCrossfadeTransition.overlapMidpointLeadMs = 125;
        engine.m_preparedCrossfadeTransition.bufferedAtArmMs       = 250;
        engine.m_preparedCrossfadeTransition.boundarySignalled     = false;
    }

    static bool hasPreparedNext(const AudioEngine& engine)
    {
        return engine.m_preparedNext.has_value();
    }

    static bool preparedNextHasStream(const AudioEngine& engine)
    {
        return engine.m_preparedNext.has_value() && engine.m_preparedNext->preparedStream != nullptr;
    }

    static uint64_t preparedDecodePositionMs(const AudioEngine& engine)
    {
        return engine.m_preparedNext.has_value() ? engine.m_preparedNext->preparedDecodePositionMs : 0;
    }

    static bool preparedCrossfadeActive(const AudioEngine& engine)
    {
        return engine.m_preparedCrossfadeTransition.active;
    }
};

FOOYIN_AUDIOENGINE_SENSITIVE_TEST(AudioEngineTest, LoadTrackFullReinitInitialisesDecoderAndOutput)
{
    ensureCoreApplication();
    EngineHarness harness{false};

    const Track track = harness.createTrack(u"full-reinit.fyt"_s, 0, 100000);
    harness.engine.loadTrack(makePlaybackItem(track, 1), false);

    ASSERT_TRUE(pumpUntil([&harness]() { return harness.engine.trackStatus() == Engine::TrackStatus::Loaded; }));
    EXPECT_EQ(harness.engine.playbackState(), Engine::PlaybackState::Stopped);
    EXPECT_GE(harness.decoderStats->initCalls.load(), 1);
    EXPECT_EQ(harness.outputStats->initCalls.load(), 1);
    EXPECT_EQ(harness.outputStats->uninitCalls.load(), 0);
}

FOOYIN_AUDIOENGINE_REGULAR_TEST(AudioEngineTest, AdoptPreparedDecoderPreservesDecodeStateAcrossTransfer)
{
    auto stats = std::make_shared<DecoderStats>();

    auto decoder      = std::make_unique<FakeDecoder>(stats);
    const Track track = makeTrack(u"prepared-decoder-state.fyt"_s, 0, 1000);

    LoadedDecoder loaded;
    loaded.format  = decoder->init(AudioSource{}, track, AudioDecoder::None);
    loaded.decoder = std::move(decoder);
    ASSERT_TRUE(loaded.format.has_value());

    DecoderContext preparingContext;
    ASSERT_TRUE(preparingContext.init(std::move(loaded), track));

    preparingContext.start();
    EXPECT_TRUE(preparingContext.isDecoding());
    EXPECT_EQ(1, stats->startCalls.load());
    EXPECT_EQ(0, stats->stopCalls.load());

    LoadedDecoder prepared = preparingContext.takeLoadedDecoder();
    EXPECT_TRUE(prepared.isDecoding);

    DecoderContext adoptedContext;
    ASSERT_TRUE(adoptedContext.adoptPreparedDecoder(std::move(prepared), track));
    EXPECT_TRUE(adoptedContext.isDecoding());

    adoptedContext.start();
    EXPECT_EQ(1, stats->startCalls.load());

    adoptedContext.stop();
    EXPECT_EQ(1, stats->stopCalls.load());
}

FOOYIN_AUDIOENGINE_SENSITIVE_TEST(AudioEngineTest, ContiguousSegmentSwitchDoesNotReinitDecoderOrOutput)
{
    ensureCoreApplication();
    EngineHarness harness{false};

    Track firstTrack = harness.createTrack(u"segments.fyt"_s, 0, 100000);
    firstTrack.setIsChapter(true);
    harness.engine.loadTrack(makePlaybackItem(firstTrack, 1), false);
    ASSERT_TRUE(pumpUntil([&harness]() { return harness.engine.trackStatus() == Engine::TrackStatus::Loaded; }));

    harness.engine.play();
    ASSERT_TRUE(pumpUntil([&harness]() { return harness.engine.playbackState() == Engine::PlaybackState::Playing; }));

    const int decoderInitBefore  = harness.decoderStats->initCalls.load();
    const int outputInitBefore   = harness.outputStats->initCalls.load();
    const int outputUninitBefore = harness.outputStats->uninitCalls.load();

    Track nextSegment = firstTrack;
    nextSegment.setIsChapter(true);
    nextSegment.setOffset(firstTrack.offset() + firstTrack.duration());
    nextSegment.setDuration(firstTrack.duration());

    harness.engine.loadTrack(makePlaybackItem(nextSegment, 2), false);
    QCoreApplication::processEvents(QEventLoop::AllEvents, 10);

    EXPECT_EQ(harness.decoderStats->initCalls.load(), decoderInitBefore);
    EXPECT_EQ(harness.outputStats->initCalls.load(), outputInitBefore);
    EXPECT_EQ(harness.outputStats->uninitCalls.load(), outputUninitBefore);
    EXPECT_EQ(harness.engine.playbackState(), Engine::PlaybackState::Playing);
    EXPECT_NE(harness.engine.trackStatus(), Engine::TrackStatus::Invalid);
}

FOOYIN_AUDIOENGINE_SENSITIVE_TEST(AudioEngineTest, SeekAfterContiguousSegmentSwitchPublishesTargetPosition)
{
    ensureCoreApplication();
    EngineHarness harness{false};

    Track firstTrack = harness.createTrack(u"segments-seek.fyt"_s, 0, 10000);
    firstTrack.setIsChapter(true);
    harness.engine.loadTrack(makePlaybackItem(firstTrack, 1), false);
    ASSERT_TRUE(pumpUntil([&harness]() { return harness.engine.trackStatus() == Engine::TrackStatus::Loaded; }));

    harness.engine.play();
    ASSERT_TRUE(pumpUntil([&harness]() { return harness.engine.playbackState() == Engine::PlaybackState::Playing; }));

    Track nextSegment = firstTrack;
    nextSegment.setIsChapter(true);
    nextSegment.setOffset(firstTrack.offset() + firstTrack.duration());
    nextSegment.setDuration(12000);

    harness.engine.loadTrack(makePlaybackItem(nextSegment, 2), false);
    ASSERT_TRUE(pumpUntil([&harness]() { return AudioEngineTestAccessor::currentTrackItemId(harness.engine) == 2; }));

    static constexpr uint64_t seekPositionMs = 5000;
    harness.engine.seek(seekPositionMs);

    ASSERT_TRUE(pumpUntil([&harness]() { return harness.decoderStats->seekCalls.load() >= 1; }, 2000ms));
    ASSERT_TRUE(pumpUntil(
        [&harness]() {
            const uint64_t positionMs = harness.engine.position();
            return positionMs >= (seekPositionMs > 1000 ? seekPositionMs - 1000 : 0)
                && positionMs <= seekPositionMs + 1800;
        },
        700ms));

    const uint64_t postSeekPositionMs = harness.engine.position();
    EXPECT_GE(postSeekPositionMs, seekPositionMs > 1000 ? seekPositionMs - 1000 : 0);
    EXPECT_LE(postSeekPositionMs, seekPositionMs + 1800);
}

FOOYIN_AUDIOENGINE_SENSITIVE_TEST(AudioEngineTest, PlayPauseStopWithFadeCompletesStateTransitions)
{
    ensureCoreApplication();
    EngineHarness harness{/*enablePauseStopFade=*/true};

    const Track track = harness.createTrack(u"fade-smoke.fyt"_s, 0, 100000);
    harness.engine.loadTrack(makePlaybackItem(track, 1), false);
    ASSERT_TRUE(pumpUntil([&harness]() { return harness.engine.trackStatus() == Engine::TrackStatus::Loaded; }));

    harness.engine.play();
    ASSERT_TRUE(pumpUntil([&harness]() { return harness.engine.playbackState() == Engine::PlaybackState::Playing; }));

    harness.engine.pause();
    ASSERT_TRUE(
        pumpUntil([&harness]() { return harness.engine.playbackState() == Engine::PlaybackState::Paused; }, 3000ms));

    harness.engine.play();
    ASSERT_TRUE(pumpUntil([&harness]() { return harness.engine.playbackState() == Engine::PlaybackState::Playing; }));

    harness.engine.stop();
    ASSERT_TRUE(
        pumpUntil([&harness]() { return harness.engine.playbackState() == Engine::PlaybackState::Stopped; }, 3000ms));
    EXPECT_GE(harness.outputStats->uninitCalls.load(), 1);
    EXPECT_EQ(harness.engine.trackStatus(), Engine::TrackStatus::NoTrack);
    EXPECT_TRUE(pumpUntil([&harness]() { return harness.engine.position() == 0; }, 1000ms));
    EXPECT_EQ(harness.engine.position(), 0U);
}

FOOYIN_AUDIOENGINE_SENSITIVE_TEST(AudioEngineTest, PauseDoesNotResetOutputQueue)
{
    ensureCoreApplication();
    EngineHarness harness{false};

    const Track track = harness.createTrack(u"pause-preserves-output.fyt"_s, 0, 100000);
    harness.engine.loadTrack(makePlaybackItem(track, 1), false);
    ASSERT_TRUE(pumpUntil([&harness]() { return harness.engine.trackStatus() == Engine::TrackStatus::Loaded; }));

    harness.engine.play();
    ASSERT_TRUE(pumpUntil([&harness]() { return harness.engine.playbackState() == Engine::PlaybackState::Playing; }));

    const int resetCallsBeforePause = harness.outputStats->resetCalls.load();

    harness.engine.pause();
    ASSERT_TRUE(
        pumpUntil([&harness]() { return harness.engine.playbackState() == Engine::PlaybackState::Paused; }, 3000ms));

    EXPECT_EQ(harness.outputStats->resetCalls.load(), resetCallsBeforePause);
}

FOOYIN_AUDIOENGINE_SENSITIVE_TEST(AudioEngineTest, FadePauseDoesNotResetOutputQueueAfterDrain)
{
    ensureCoreApplication();
    EngineHarness harness{/*enablePauseStopFade=*/true};

    const Track track = harness.createTrack(u"fade-pause-preserves-output.fyt"_s, 0, 100000);
    harness.engine.loadTrack(makePlaybackItem(track, 1), false);
    ASSERT_TRUE(pumpUntil([&harness]() { return harness.engine.trackStatus() == Engine::TrackStatus::Loaded; }));

    harness.engine.play();
    ASSERT_TRUE(pumpUntil([&harness]() { return harness.engine.playbackState() == Engine::PlaybackState::Playing; }));

    const int resetCallsBeforePause     = harness.outputStats->resetCalls.load();
    const int setPausedCallsBeforePause = harness.outputStats->setPausedCalls.load();
    harness.outputStats->setOutputState(256, 256, 120);

    harness.engine.pause();
    ASSERT_TRUE(
        pumpUntil([&harness]() { return harness.engine.playbackState() == Engine::PlaybackState::Paused; }, 3000ms));

    EXPECT_EQ(harness.engine.playbackState(), Engine::PlaybackState::Paused);
    EXPECT_EQ(harness.outputStats->resetCalls.load(), resetCallsBeforePause);

    harness.outputStats->setOutputState(256, 0, 0);
    ASSERT_TRUE(pumpUntil(
        [&harness, setPausedCallsBeforePause]() {
            return harness.outputStats->setPausedCalls.load() > setPausedCallsBeforePause;
        },
        1500ms));

    EXPECT_EQ(harness.engine.playbackState(), Engine::PlaybackState::Paused);
    EXPECT_EQ(harness.outputStats->resetCalls.load(), resetCallsBeforePause);
}

FOOYIN_AUDIOENGINE_SENSITIVE_TEST(AudioEngineTest, FadePauseFinalisesWhenQueuedFramesDrainEvenIfDelayRemains)
{
    ensureCoreApplication();
    EngineHarness harness{/*enablePauseStopFade=*/true};

    const Track track = harness.createTrack(u"fade-pause-delay-residual.fyt"_s, 0, 100000);
    harness.engine.loadTrack(makePlaybackItem(track, 1), false);
    ASSERT_TRUE(pumpUntil([&harness]() { return harness.engine.trackStatus() == Engine::TrackStatus::Loaded; }));

    harness.engine.play();
    ASSERT_TRUE(pumpUntil([&harness]() { return harness.engine.playbackState() == Engine::PlaybackState::Playing; }));

    const int setPausedCallsBeforePause = harness.outputStats->setPausedCalls.load();
    harness.outputStats->setOutputState(256, 256, 120);

    harness.engine.pause();
    ASSERT_TRUE(pumpUntil([&harness]() { return harness.engine.playbackState() == Engine::PlaybackState::Paused; }));

    harness.outputStats->setOutputState(256, 0, 53);
    ASSERT_TRUE(pumpUntil(
        [&harness, setPausedCallsBeforePause]() {
            return harness.outputStats->setPausedCalls.load() > setPausedCallsBeforePause;
        },
        500ms));
}

FOOYIN_AUDIOENGINE_SENSITIVE_TEST(AudioEngineTest, PlayCancelsPendingAudiblePauseCompletion)
{
    ensureCoreApplication();
    EngineHarness harness{/*enablePauseStopFade=*/true};

    const Track track = harness.createTrack(u"fade-pause-cancelled-by-play.fyt"_s, 0, 100000);
    harness.engine.loadTrack(makePlaybackItem(track, 1), false);
    ASSERT_TRUE(pumpUntil([&harness]() { return harness.engine.trackStatus() == Engine::TrackStatus::Loaded; }));

    harness.engine.play();
    ASSERT_TRUE(pumpUntil([&harness]() { return harness.engine.playbackState() == Engine::PlaybackState::Playing; }));

    const int resetCallsBeforePause = harness.outputStats->resetCalls.load();
    harness.outputStats->setOutputState(256, 256, 120);

    harness.engine.pause();
    ASSERT_TRUE(
        pumpUntil([&harness]() { return harness.engine.playbackState() == Engine::PlaybackState::Paused; }, 3000ms));

    harness.engine.play();
    harness.outputStats->setOutputState(256, 0, 0);

    ASSERT_TRUE(
        pumpUntil([&harness]() { return harness.engine.playbackState() == Engine::PlaybackState::Playing; }, 3000ms));
    EXPECT_FALSE(pumpUntil(
        [&harness, resetCallsBeforePause]() { return harness.outputStats->resetCalls.load() != resetCallsBeforePause; },
        300ms));
    EXPECT_EQ(harness.engine.playbackState(), Engine::PlaybackState::Playing);
    EXPECT_EQ(harness.outputStats->resetCalls.load(), resetCallsBeforePause);
}

FOOYIN_AUDIOENGINE_SENSITIVE_TEST(AudioEngineTest, FadePauseWatchdogFinalisesPauseWhenDelaySticks)
{
    ensureCoreApplication();
    EngineHarness harness{/*enablePauseStopFade=*/true};

    const Track track = harness.createTrack(u"fade-pause-watchdog.fyt"_s, 0, 100000);
    harness.engine.loadTrack(makePlaybackItem(track, 1), false);
    ASSERT_TRUE(pumpUntil([&harness]() { return harness.engine.trackStatus() == Engine::TrackStatus::Loaded; }));

    harness.engine.play();
    ASSERT_TRUE(pumpUntil([&harness]() { return harness.engine.playbackState() == Engine::PlaybackState::Playing; }));

    const int setPausedCallsBeforePause = harness.outputStats->setPausedCalls.load();
    harness.outputStats->setOutputState(256, 256, 120);

    harness.engine.pause();
    ASSERT_TRUE(pumpUntil([&harness]() { return harness.engine.playbackState() == Engine::PlaybackState::Paused; }));

    ASSERT_TRUE(pumpUntil(
        [&harness, setPausedCallsBeforePause]() {
            return harness.outputStats->setPausedCalls.load() > setPausedCallsBeforePause;
        },
        2500ms));
}

FOOYIN_AUDIOENGINE_SENSITIVE_TEST(AudioEngineTest, DisconnectedOutputReinitialisesAndPlaybackCanResume)
{
    ensureCoreApplication();
    EngineHarness harness{false};

    const Track track = harness.createTrack(u"output-disconnect-reinit.fyt"_s, 0, 100000);
    harness.engine.loadTrack(makePlaybackItem(track, 1), false);
    ASSERT_TRUE(pumpUntil([&harness]() { return harness.engine.trackStatus() == Engine::TrackStatus::Loaded; }));

    harness.engine.play();
    ASSERT_TRUE(pumpUntil([&harness]() { return harness.engine.playbackState() == Engine::PlaybackState::Playing; }));

    harness.engine.pause();
    ASSERT_TRUE(
        pumpUntil([&harness]() { return harness.engine.playbackState() == Engine::PlaybackState::Paused; }, 3000ms));

    const int initCallsBeforeReconnect   = harness.outputStats->initCalls.load();
    const int uninitCallsBeforeReconnect = harness.outputStats->uninitCalls.load();

    harness.outputStats->emitDisconnect();

    ASSERT_TRUE(pumpUntil(
        [&harness, initCallsBeforeReconnect, uninitCallsBeforeReconnect]() {
            return harness.outputStats->disconnectSignals.load() >= 1
                && harness.outputStats->initCalls.load() > initCallsBeforeReconnect
                && harness.outputStats->uninitCalls.load() > uninitCallsBeforeReconnect;
        },
        3000ms));

    EXPECT_EQ(harness.engine.playbackState(), Engine::PlaybackState::Paused);

    harness.engine.play();

    ASSERT_TRUE(pumpUntil([&harness]() { return harness.engine.playbackState() == Engine::PlaybackState::Playing; }));
}

FOOYIN_AUDIOENGINE_SENSITIVE_TEST(AudioEngineTest, SeekDiscontinuityPublishesNearTargetQuickly)
{
    ensureCoreApplication();
    EngineHarness harness{false};

    const Track track = harness.createTrack(u"seek-quick-discontinuity.fyt"_s, 0, 120000);
    harness.engine.loadTrack(makePlaybackItem(track, 1), false);
    ASSERT_TRUE(pumpUntil([&harness]() { return harness.engine.trackStatus() == Engine::TrackStatus::Loaded; }));

    harness.engine.play();
    ASSERT_TRUE(pumpUntil([&harness]() { return harness.engine.playbackState() == Engine::PlaybackState::Playing; }));
    ASSERT_TRUE(pumpUntil([&harness]() { return harness.engine.position() > 0; }, 3000ms));

    const uint64_t preSeekPositionMs         = harness.engine.position();
    static constexpr uint64_t seekPositionMs = 8000;
    harness.engine.seek(seekPositionMs);

    ASSERT_TRUE(pumpUntil([&harness]() { return harness.decoderStats->seekCalls.load() >= 1; }, 2000ms));
    ASSERT_TRUE(pumpUntil(
        [&harness]() {
            const uint64_t positionMs = harness.engine.position();
            return positionMs >= (seekPositionMs > 1000 ? seekPositionMs - 1000 : 0)
                && positionMs <= seekPositionMs + 1800;
        },
        700ms));

    const uint64_t postSeekPositionMs = harness.engine.position();
    EXPECT_NE(postSeekPositionMs, preSeekPositionMs);
    EXPECT_GE(postSeekPositionMs, seekPositionMs > 1000 ? seekPositionMs - 1000 : 0);
    EXPECT_LE(postSeekPositionMs, seekPositionMs + 1800);
}

FOOYIN_AUDIOENGINE_SENSITIVE_TEST(AudioEngineTest, SeekWithRequestPublishesMatchingRequestIds)
{
    ensureCoreApplication();
    EngineHarness harness{false};

    const Track track = harness.createTrack(u"seek-request-id.fyt"_s, 0, 120000);
    harness.engine.loadTrack(makePlaybackItem(track, 1), false);
    ASSERT_TRUE(pumpUntil([&harness]() { return harness.engine.trackStatus() == Engine::TrackStatus::Loaded; }));

    harness.engine.play();
    ASSERT_TRUE(pumpUntil([&harness]() { return harness.engine.playbackState() == Engine::PlaybackState::Playing; }));
    ASSERT_TRUE(pumpUntil([&harness]() { return harness.engine.position() > 0; }, 3000ms));

    std::vector<std::pair<uint64_t, uint64_t>> applied;
    QObject::connect(
        &harness.engine, &AudioEngine::seekPositionApplied, &harness.engine,
        [&applied](uint64_t positionMs, uint64_t requestId) { applied.emplace_back(positionMs, requestId); });

    harness.engine.seekWithRequest(4000, 11);
    harness.engine.seekWithRequest(4200, 12);

    ASSERT_TRUE(pumpUntil([&applied]() { return applied.size() >= 2; }, 3000ms));
    EXPECT_EQ(applied[0].second, 11U);
    EXPECT_EQ(applied[1].second, 12U);
}

FOOYIN_AUDIOENGINE_SENSITIVE_TEST(AudioEngineTest, SeekInvalidatesArmedPreparedCrossfadeTransition)
{
    ensureCoreApplication();

    QTemporaryDir tempDir;
    SettingsManager settings{tempDir.filePath(u"settings.ini"_s)};
    registerMinimalEngineSettings(settings, false, false);

    Engine::CrossfadingValues crossfadingValues;
    crossfadingValues.autoChange.in  = 120;
    crossfadingValues.autoChange.out = 120;
    settings.set<Settings::Core::Internal::EngineCrossfading>(true);
    settings.set<Settings::Core::Internal::CrossfadingValues>(QVariant::fromValue(crossfadingValues));
    settings.set<Settings::Core::Internal::CrossfadeSwitchPolicy>(
        static_cast<int>(Engine::CrossfadeSwitchPolicy::OverlapStart));

    auto decoderStats         = std::make_shared<DecoderStats>();
    auto outputStats          = std::make_shared<OutputStats>();
    auto loader               = std::make_shared<AudioLoader>();
    auto visualisationBackend = std::make_shared<VisualisationBackend>();
    DspRegistry registry;
    AudioEngine engine{loader, &settings, &registry, visualisationBackend};

    loader->addDecoder(u"FakeDecoder"_s, [decoderStats]() { return std::make_unique<FakeDecoder>(decoderStats); });
    engine.setAudioOutput([outputStats]() { return std::make_unique<FakeAudioOutput>(outputStats); }, QString{});

    const Track currentTrack
        = makeTrack(createDummyAudioFile(tempDir, u"seek-invalidates-armed-current.fyt"_s), 0, 120000);
    const Track nextTrack = makeTrack(createDummyAudioFile(tempDir, u"seek-invalidates-armed-next.fyt"_s), 0, 120000);
    const auto nextItem   = makePlaybackItem(nextTrack, 2);

    uint64_t currentGeneration{0};
    QObject::connect(&engine, &AudioEngine::trackStatusContextChanged, &engine,
                     [&currentGeneration, &currentTrack](Engine::TrackStatus, const Track& track, uint64_t generation) {
                         if(track.filepath() == currentTrack.filepath()) {
                             currentGeneration = generation;
                         }
                     });

    engine.loadTrack(makePlaybackItem(currentTrack, 1), false);
    ASSERT_TRUE(pumpUntil([&engine]() { return engine.trackStatus() == Engine::TrackStatus::Loaded; }));
    ASSERT_TRUE(pumpUntil([&currentGeneration]() { return currentGeneration != 0; }));

    engine.play();
    ASSERT_TRUE(pumpUntil([&engine]() { return engine.playbackState() == Engine::PlaybackState::Playing; }));
    ASSERT_TRUE(pumpUntil([&engine]() { return engine.position() > 0; }, 3000ms));

    const AudioFormat preparedFormat{SampleFormat::F64, 1000, 2};
    auto preparedStream = StreamFactory::createStream(preparedFormat, 4000);
    ASSERT_TRUE(preparedStream);
    AudioEngineTestAccessor::installPreparedCrossfade(engine, nextItem, currentGeneration, preparedStream,
                                                      preparedFormat);
    ASSERT_TRUE(AudioEngineTestAccessor::preparedCrossfadeActive(engine));
    ASSERT_TRUE(AudioEngineTestAccessor::hasPreparedNext(engine));

    engine.seek(4000);

    ASSERT_TRUE(pumpUntil([&decoderStats]() { return decoderStats->seekCalls.load() >= 1; }, 3000ms));
    ASSERT_TRUE(pumpUntil([&engine]() { return engine.position() >= 3000; }, 3000ms));
    EXPECT_FALSE(AudioEngineTestAccessor::preparedCrossfadeActive(engine));
    EXPECT_FALSE(AudioEngineTestAccessor::hasPreparedNext(engine));
    EXPECT_FALSE(engine.commitPreparedCrossfadeTransition(nextItem));
}

FOOYIN_AUDIOENGINE_SENSITIVE_TEST(AudioEngineTest, OverlapMidpointSwitchPolicyCommitsAfterIncomingAnchor)
{
    ensureCoreApplication();
    EngineHarness harness{false};

    Engine::CrossfadingValues crossfadingValues;
    crossfadingValues.autoChange.in  = 400;
    crossfadingValues.autoChange.out = 400;
    harness.settings.set<Settings::Core::Internal::EngineCrossfading>(true);
    harness.settings.set<Settings::Core::Internal::CrossfadingValues>(QVariant::fromValue(crossfadingValues));
    harness.settings.set<Settings::Core::Internal::CrossfadeSwitchPolicy>(
        static_cast<int>(Engine::CrossfadeSwitchPolicy::OverlapMidpoint));
    AudioEngineTestAccessor::setCrossfadeConfig(harness.engine, true, crossfadingValues,
                                                Engine::CrossfadeSwitchPolicy::OverlapMidpoint);

    const Track currentTrack = harness.createTrack(u"overlap-midpoint-current.fyt"_s, 0, 120000);
    const Track nextTrack    = harness.createTrack(u"overlap-midpoint-next.fyt"_s, 0, 120000);
    const auto currentItem   = makePlaybackItem(currentTrack, 1);
    const auto nextItem      = makePlaybackItem(nextTrack, 2);

    harness.engine.loadTrack(currentItem, false);
    ASSERT_TRUE(pumpUntil([&harness]() { return harness.engine.trackStatus() == Engine::TrackStatus::Loaded; }));

    harness.engine.play();
    ASSERT_TRUE(pumpUntil([&harness]() { return harness.engine.playbackState() == Engine::PlaybackState::Playing; }));
    ASSERT_TRUE(pumpUntil([&harness]() { return harness.engine.position() > 0; }, 3000ms));

    ASSERT_TRUE(AudioEngineTestAccessor::prepareNextTrackImmediate(harness.engine, nextItem, 0));
    AudioEngineTestAccessor::setUpcomingTrackCandidate(harness.engine, nextItem);

    const uint64_t initialGeneration = AudioEngineTestAccessor::trackGeneration(harness.engine);
    std::vector<Engine::TrackCommitContext> commits;
    const auto commitConn
        = QObject::connect(&harness.engine, &AudioEngine::trackCommitted, &harness.engine,
                           [&commits](const Engine::TrackCommitContext& context) { commits.push_back(context); });

    AudioEngineTestAccessor::activatePreparedCrossfade(harness.engine, nextItem, initialGeneration, 250, 125);
    ASSERT_TRUE(AudioEngineTestAccessor::preparedCrossfadeActive(harness.engine));
    EXPECT_GT(AudioEngineTestAccessor::overlapMidpointLeadMs(harness.engine), 0);

    AudioEngineTestAccessor::setAutoAdvanceState(harness.engine, initialGeneration, AutoTransitionMode::Crossfade, true,
                                                 false, false);
    AudioEngineTestAccessor::tryAutoAdvanceCommit(harness.engine);

    EXPECT_TRUE(commits.empty());
    EXPECT_EQ(AudioEngineTestAccessor::currentTrackId(harness.engine), currentTrack.id());
    EXPECT_EQ(AudioEngineTestAccessor::currentTrackItemId(harness.engine), currentItem.itemId);

    AudioEngineTestAccessor::setAutoAdvanceState(harness.engine, initialGeneration, AutoTransitionMode::Crossfade, true,
                                                 true, false);
    AudioEngineTestAccessor::tryAutoAdvanceCommit(harness.engine);

    ASSERT_EQ(commits.size(), 1U);
    EXPECT_EQ(commits.back().mode, Engine::TransitionMode::Crossfade);
    EXPECT_EQ(commits.back().track.id(), nextTrack.id());
    EXPECT_EQ(commits.back().itemId, nextItem.itemId);
    EXPECT_EQ(AudioEngineTestAccessor::currentTrackId(harness.engine), nextTrack.id());
    EXPECT_EQ(AudioEngineTestAccessor::currentTrackItemId(harness.engine), nextItem.itemId);
    EXPECT_FALSE(AudioEngineTestAccessor::preparedCrossfadeActive(harness.engine));

    QObject::disconnect(commitConn);
}

FOOYIN_AUDIOENGINE_SENSITIVE_TEST(AudioEngineTest, SeekWithRequestFromStoppedLoadsTrackAndStartsPlayback)
{
    ensureCoreApplication();
    EngineHarness harness{false};

    const Track track = harness.createTrack(u"seek-from-stopped.fyt"_s, 0, 120000);
    harness.engine.loadTrack(makePlaybackItem(track, 1), false);
    ASSERT_TRUE(pumpUntil([&harness]() { return harness.engine.trackStatus() == Engine::TrackStatus::Loaded; }));

    harness.engine.play();
    ASSERT_TRUE(pumpUntil([&harness]() { return harness.engine.playbackState() == Engine::PlaybackState::Playing; }));
    harness.engine.stop();
    ASSERT_TRUE(
        pumpUntil([&harness]() { return harness.engine.playbackState() == Engine::PlaybackState::Stopped; }, 3000ms));

    const int seekCallsBefore = harness.decoderStats->seekCalls.load();
    std::vector<uint64_t> appliedRequestIds;
    QObject::connect(&harness.engine, &AudioEngine::seekPositionApplied, &harness.engine,
                     [&appliedRequestIds](uint64_t, uint64_t requestId) { appliedRequestIds.push_back(requestId); });

    harness.engine.seekWithRequest(6000, 42);

    ASSERT_TRUE(pumpUntil([&harness]() { return harness.engine.playbackState() == Engine::PlaybackState::Playing; }));
    ASSERT_TRUE(pumpUntil(
        [&harness, seekCallsBefore]() { return harness.decoderStats->seekCalls.load() > seekCallsBefore; }, 3000ms));
    ASSERT_TRUE(pumpUntil([&appliedRequestIds]() { return !appliedRequestIds.empty(); }, 3000ms));
    EXPECT_EQ(appliedRequestIds.back(), 42U);
}

FOOYIN_AUDIOENGINE_REGULAR_TEST(AudioEngineTest, AudioStreamBitrateSpansAdvanceWithPlaybackPosition)
{
    const AudioFormat format{SampleFormat::F64, 1000, 2};
    auto stream = StreamFactory::createStream(format, 8000);
    ASSERT_TRUE(stream);

    std::vector<double> samples(4000, 0.1);
    auto writer = stream->writer();
    ASSERT_EQ(writer.write(samples.data(), samples.size()), samples.size());

    stream->appendBitrateSpan(0, 2000, 192);
    stream->appendBitrateSpan(2000, 4000, 256);

    EXPECT_EQ(stream->audibleWindowBitrate(1000), 192);

    std::vector<double> output(2000, 0.0);
    EXPECT_EQ(stream->read(output.data(), 1000), 1000);
    EXPECT_EQ(stream->audibleWindowBitrate(1000), 256);
}

FOOYIN_AUDIOENGINE_REGULAR_TEST(AudioEngineTest, AudioStreamRollingBitrateSmoothsShortOutliers)
{
    const AudioFormat format{SampleFormat::F64, 1000, 2};
    auto stream = StreamFactory::createStream(format, 8000);
    ASSERT_TRUE(stream);

    stream->appendBitrateSpan(0, 20, 2);
    stream->appendBitrateSpan(20, 2000, 192);

    EXPECT_GE(stream->audibleWindowBitrate(1000), 180);
}

FOOYIN_AUDIOENGINE_SENSITIVE_TEST(AudioEngineTest, ZeroVbrIntervalDisablesLiveBitrateUpdates)
{
    ensureCoreApplication();
    QTemporaryDir tempDir;
    SettingsManager settings{tempDir.filePath(u"settings.ini"_s)};
    registerMinimalEngineSettings(settings, false, false);

    auto decoderStats         = std::make_shared<DecoderStats>();
    auto outputStats          = std::make_shared<OutputStats>();
    auto loader               = std::make_shared<AudioLoader>();
    auto visualisationBackend = std::make_shared<VisualisationBackend>();
    DspRegistry registry;
    AudioEngine engine{loader, &settings, &registry, visualisationBackend};

    loader->addDecoder(u"FakeDecoder"_s, [stats = decoderStats]() { return std::make_unique<FakeDecoder>(stats); });
    engine.setAudioOutput([stats = outputStats]() { return std::make_unique<FakeAudioOutput>(stats); }, QString{});

    settings.set<Settings::Core::Internal::VBRUpdateInterval>(0);
    decoderStats->bitrate.store(192, std::memory_order_relaxed);

    Track track = makeTrack(createDummyAudioFile(tempDir, u"vbr-disabled.fyt"_s), 0, 120000);
    track.setBitrate(0);

    std::vector<int> updates;
    const auto bitrateConn = QObject::connect(&engine, &AudioEngine::bitrateChanged, &engine,
                                              [&updates](int bitrate) { updates.push_back(bitrate); });

    engine.loadTrack(makePlaybackItem(track, 1), false);
    ASSERT_TRUE(pumpUntil([&engine]() { return engine.trackStatus() == Engine::TrackStatus::Loaded; }));

    engine.play();
    ASSERT_TRUE(pumpUntil([&engine]() { return engine.playbackState() == Engine::PlaybackState::Playing; }));

    EXPECT_FALSE(pumpUntil([&updates]() { return !updates.empty(); }, 300ms));
    QObject::disconnect(bitrateConn);
    engine.stopImmediate();
}

FOOYIN_AUDIOENGINE_SENSITIVE_TEST(AudioEngineTest, PositiveVbrIntervalAllowsLiveBitrateUpdates)
{
    ensureCoreApplication();
    QTemporaryDir tempDir;
    SettingsManager settings{tempDir.filePath(u"settings.ini"_s)};
    registerMinimalEngineSettings(settings, false, false);

    auto decoderStats         = std::make_shared<DecoderStats>();
    auto outputStats          = std::make_shared<OutputStats>();
    auto loader               = std::make_shared<AudioLoader>();
    auto visualisationBackend = std::make_shared<VisualisationBackend>();
    DspRegistry registry;
    AudioEngine engine{loader, &settings, &registry, visualisationBackend};

    loader->addDecoder(u"FakeDecoder"_s, [stats = decoderStats]() { return std::make_unique<FakeDecoder>(stats); });
    engine.setAudioOutput([stats = outputStats]() { return std::make_unique<FakeAudioOutput>(stats); }, QString{});

    settings.set<Settings::Core::Internal::VBRUpdateInterval>(1);
    decoderStats->bitrate.store(192, std::memory_order_relaxed);

    Track track = makeTrack(createDummyAudioFile(tempDir, u"vbr-enabled.fyt"_s), 0, 120000);
    track.setBitrate(0);

    std::vector<int> updates;
    const auto bitrateConn = QObject::connect(&engine, &AudioEngine::bitrateChanged, &engine,
                                              [&updates](int bitrate) { updates.push_back(bitrate); });

    engine.loadTrack(makePlaybackItem(track, 1), false);
    ASSERT_TRUE(pumpUntil([&engine]() { return engine.trackStatus() == Engine::TrackStatus::Loaded; }));

    engine.play();
    ASSERT_TRUE(pumpUntil([&engine]() { return engine.playbackState() == Engine::PlaybackState::Playing; }));

    ASSERT_TRUE(pumpUntil([&updates]() { return !updates.empty(); }, 1000ms));
    EXPECT_EQ(updates.back(), 192);
    QObject::disconnect(bitrateConn);
    engine.stopImmediate();
}

FOOYIN_AUDIOENGINE_SENSITIVE_TEST(AudioEngineTest, DisablingVbrUpdatesRestoresStaticTrackBitrate)
{
    ensureCoreApplication();
    QTemporaryDir tempDir;
    SettingsManager settings{tempDir.filePath(u"settings.ini"_s)};
    registerMinimalEngineSettings(settings, false, false);

    auto decoderStats         = std::make_shared<DecoderStats>();
    auto outputStats          = std::make_shared<OutputStats>();
    auto loader               = std::make_shared<AudioLoader>();
    auto visualisationBackend = std::make_shared<VisualisationBackend>();
    DspRegistry registry;
    AudioEngine engine{loader, &settings, &registry, visualisationBackend};

    loader->addDecoder(u"FakeDecoder"_s, [stats = decoderStats]() { return std::make_unique<FakeDecoder>(stats); });
    engine.setAudioOutput([stats = outputStats]() { return std::make_unique<FakeAudioOutput>(stats); }, QString{});

    settings.set<Settings::Core::Internal::VBRUpdateInterval>(1);
    decoderStats->bitrate.store(192, std::memory_order_relaxed);

    Track track = makeTrack(createDummyAudioFile(tempDir, u"vbr-disable-restores-static.fyt"_s), 0, 120000);
    track.setBitrate(320);

    std::vector<int> updates;
    const auto bitrateConn = QObject::connect(&engine, &AudioEngine::bitrateChanged, &engine,
                                              [&updates](int bitrate) { updates.push_back(bitrate); });

    engine.loadTrack(makePlaybackItem(track, 1), false);
    ASSERT_TRUE(pumpUntil([&engine]() { return engine.trackStatus() == Engine::TrackStatus::Loaded; }));

    engine.play();
    ASSERT_TRUE(pumpUntil([&engine]() { return engine.playbackState() == Engine::PlaybackState::Playing; }));

    ASSERT_TRUE(pumpUntil([&updates]() { return !updates.empty() && updates.back() == 192; }, 1000ms));

    settings.set<Settings::Core::Internal::VBRUpdateInterval>(0);
    ASSERT_TRUE(pumpUntil([&updates]() { return !updates.empty() && updates.back() == 320; }, 1000ms));

    QObject::disconnect(bitrateConn);
    engine.stopImmediate();
}

FOOYIN_AUDIOENGINE_SENSITIVE_TEST(AudioEngineTest, PrepareNextTrackInvalidTrackEmitsNotReady)
{
    ensureCoreApplication();
    EngineHarness harness{false};

    bool received{false};
    bool ready{true};
    uint64_t requestId{0};
    Track signalTrack;

    QObject::connect(
        &harness.engine, &AudioEngine::nextTrackReadiness, &harness.engine,
        [&received, &signalTrack, &ready, &requestId](const Engine::PlaybackItem& item, bool trackReady, uint64_t id) {
            received    = true;
            signalTrack = item.track;
            ready       = trackReady;
            requestId   = id;
        });

    harness.engine.prepareNextTrack({}, 77);

    ASSERT_TRUE(pumpUntil([&received]() { return received; }, 1000ms));
    EXPECT_FALSE(signalTrack.isValid());
    EXPECT_FALSE(ready);
    EXPECT_EQ(requestId, 77U);
}

FOOYIN_AUDIOENGINE_SENSITIVE_TEST(AudioEngineTest, SetVolumeClampsAndPropagatesToOutput)
{
    ensureCoreApplication();
    EngineHarness harness{false};

    const Track track = harness.createTrack(u"volume-clamp.fyt"_s, 0, 100000);
    harness.engine.loadTrack(makePlaybackItem(track, 1), false);
    ASSERT_TRUE(pumpUntil([&harness]() { return harness.engine.trackStatus() == Engine::TrackStatus::Loaded; }));

    const int volumeCallsBefore = harness.outputStats->setVolumeCalls.load();
    harness.engine.setVolume(2.0);
    ASSERT_TRUE(pumpUntil(
        [&harness, volumeCallsBefore]() { return harness.outputStats->setVolumeCalls.load() > volumeCallsBefore; },
        2000ms));
    EXPECT_DOUBLE_EQ(harness.outputStats->volume(), 1.0);

    const int volumeCallsMid = harness.outputStats->setVolumeCalls.load();
    harness.engine.setVolume(-1.0);
    ASSERT_TRUE(pumpUntil(
        [&harness, volumeCallsMid]() { return harness.outputStats->setVolumeCalls.load() > volumeCallsMid; }, 2000ms));
    EXPECT_DOUBLE_EQ(harness.outputStats->volume(), 0.0);
}

FOOYIN_AUDIOENGINE_SENSITIVE_TEST(AudioEngineTest, SetAudioOutputReinitializesLoadedOutput)
{
    ensureCoreApplication();
    EngineHarness harness{false};

    const Track track = harness.createTrack(u"switch-output.fyt"_s, 0, 100000);
    harness.engine.loadTrack(makePlaybackItem(track, 1), false);
    ASSERT_TRUE(pumpUntil([&harness]() { return harness.engine.trackStatus() == Engine::TrackStatus::Loaded; }));
    ASSERT_EQ(harness.outputStats->initCalls.load(), 1);

    auto newOutputStats = std::make_shared<OutputStats>();
    harness.engine.setAudioOutput([stats = newOutputStats]() { return std::make_unique<FakeAudioOutput>(stats); },
                                  QString{});

    ASSERT_TRUE(pumpUntil([&harness]() { return harness.outputStats->uninitCalls.load() >= 1; }, 2000ms));
    ASSERT_TRUE(pumpUntil([&newOutputStats]() { return newOutputStats->initCalls.load() >= 1; }, 2000ms));
}

FOOYIN_AUDIOENGINE_SENSITIVE_TEST(AudioEngineTest, SetOutputDeviceReinitialisesLoadedOutput)
{
    ensureCoreApplication();
    EngineHarness harness{false};

    const Track track = harness.createTrack(u"switch-device.fyt"_s, 0, 100000);
    harness.engine.loadTrack(makePlaybackItem(track, 1), false);
    ASSERT_TRUE(pumpUntil([&harness]() { return harness.engine.trackStatus() == Engine::TrackStatus::Loaded; }));
    ASSERT_EQ(harness.outputStats->initCalls.load(), 1);

    harness.engine.setOutputDevice(u"hw:test"_s);

    ASSERT_TRUE(pumpUntil([&harness]() { return harness.outputStats->uninitCalls.load() >= 1; }, 2000ms));
    ASSERT_TRUE(pumpUntil([&harness]() { return harness.outputStats->initCalls.load() >= 2; }, 2000ms));
    ASSERT_TRUE(pumpUntil([&harness]() { return harness.outputStats->device() == u"hw:test"_s; }, 2000ms));
}

FOOYIN_AUDIOENGINE_SENSITIVE_TEST(AudioEngineTest, BufferLengthChangeReconfiguresPlaybackWithoutReinitialisingOutput)
{
    ensureCoreApplication();
    QTemporaryDir tempDir;
    SettingsManager settings{tempDir.filePath(u"settings.ini"_s)};
    registerMinimalEngineSettings(settings, false, false);

    auto decoderStats         = std::make_shared<DecoderStats>();
    auto outputStats          = std::make_shared<OutputStats>();
    auto loader               = std::make_shared<AudioLoader>();
    auto visualisationBackend = std::make_shared<VisualisationBackend>();
    DspRegistry registry;
    AudioEngine engine{loader, &settings, &registry, visualisationBackend};

    loader->addDecoder(u"FakeDecoder"_s, [decoderStats]() { return std::make_unique<FakeDecoder>(decoderStats); });
    engine.setAudioOutput([outputStats]() { return std::make_unique<FakeAudioOutput>(outputStats); }, QString{});

    const Track track = makeTrack(createDummyAudioFile(tempDir, u"buffer-length-reconfigure.fyt"_s), 0, 120000);
    engine.loadTrack(makePlaybackItem(track, 1), false);
    ASSERT_TRUE(pumpUntil([&engine]() { return engine.trackStatus() == Engine::TrackStatus::Loaded; }));

    engine.play();
    ASSERT_TRUE(pumpUntil([&engine]() { return engine.playbackState() == Engine::PlaybackState::Playing; }));
    ASSERT_TRUE(pumpUntil([&engine]() { return engine.position() >= 250; }, 4000ms));

    const uint64_t positionBefore           = engine.position();
    const int decoderInitCallsBefore        = decoderStats->initCalls.load();
    const int decoderSeekCallsBefore        = decoderStats->seekCalls.load();
    const int outputInitCallsBefore         = outputStats->initCalls.load();
    const int outputUninitCallsBefore       = outputStats->uninitCalls.load();
    const uint64_t trackGenerationBefore    = AudioEngineTestAccessor::trackGeneration(engine);
    const uint64_t currentTrackItemIdBefore = AudioEngineTestAccessor::currentTrackItemId(engine);

    settings.set<Settings::Core::BufferLength>(8000);

    ASSERT_TRUE(pumpUntil(
        [decoderStats, decoderInitCallsBefore]() { return decoderStats->initCalls.load() > decoderInitCallsBefore; },
        4000ms));
    ASSERT_TRUE(pumpUntil(
        [decoderStats, decoderSeekCallsBefore]() { return decoderStats->seekCalls.load() > decoderSeekCallsBefore; },
        4000ms));
    ASSERT_TRUE(pumpUntil([&engine, positionBefore]() { return engine.position() > positionBefore; }, 4000ms));

    EXPECT_EQ(outputStats->initCalls.load(), outputInitCallsBefore);
    EXPECT_EQ(outputStats->uninitCalls.load(), outputUninitCallsBefore);
    EXPECT_EQ(AudioEngineTestAccessor::trackGeneration(engine), trackGenerationBefore);
    EXPECT_EQ(AudioEngineTestAccessor::currentTrackItemId(engine), currentTrackItemIdBefore);
    EXPECT_EQ(engine.playbackState(), Engine::PlaybackState::Playing);
}

FOOYIN_AUDIOENGINE_SENSITIVE_TEST(AudioEngineTest, SetDspChainWithFormatChangeReinitializesOutput)
{
    ensureCoreApplication();
    EngineHarness harness{false};

    harness.registry.registerDsp({.id = u"test.dsp.format_shift"_s, .name = u"FormatShift"_s, .factory = []() {
                                      return std::make_unique<FormatShiftDsp>();
                                  }});

    const Track track = harness.createTrack(u"dsp-format-change.fyt"_s, 0, 100000);
    harness.engine.loadTrack(makePlaybackItem(track, 1), false);
    ASSERT_TRUE(pumpUntil([&harness]() { return harness.engine.trackStatus() == Engine::TrackStatus::Loaded; }));
    const int initBefore = harness.outputStats->initCalls.load();

    Engine::DspDefinition shift;
    shift.id = u"test.dsp.format_shift"_s;

    Engine::DspChains chains;
    chains.masterChain.push_back(shift);
    harness.engine.setDspChain(chains);

    ASSERT_TRUE(
        pumpUntil([&harness, initBefore]() { return harness.outputStats->initCalls.load() > initBefore; }, 3000ms));
}

FOOYIN_AUDIOENGINE_SENSITIVE_TEST(AudioEngineTest, FormatChangeReloadDoesNotReusePreparedDecoderPosition)
{
    ensureCoreApplication();
    EngineHarness harness{false};

    const Track currentTrack = harness.createTrack(u"prepared-reinit-current.fyt"_s, 0, 120000);
    const Track nextTrack    = harness.createTrack(u"prepared-reinit-next-sr2000.fyt"_s, 0, 120000);
    const auto currentItem   = makePlaybackItem(currentTrack, 1);
    const auto nextItem      = makePlaybackItem(nextTrack, 2);

    harness.engine.loadTrack(currentItem, false);
    ASSERT_TRUE(pumpUntil([&harness]() { return harness.engine.trackStatus() == Engine::TrackStatus::Loaded; }));

    harness.engine.play();
    ASSERT_TRUE(pumpUntil([&harness]() { return harness.engine.playbackState() == Engine::PlaybackState::Playing; }));
    ASSERT_TRUE(pumpUntil([&harness]() { return harness.engine.position() > 0; }, 3000ms));

    ASSERT_TRUE(AudioEngineTestAccessor::prepareNextTrackImmediate(harness.engine, nextItem, 1200));

    const int decoderInitBeforeSwitch = harness.decoderStats->initCalls.load();
    const int outputUninitBefore      = harness.outputStats->uninitCalls.load();

    harness.engine.loadTrack(nextItem, false);

    ASSERT_TRUE(pumpUntil(
        [&harness, outputUninitBefore]() { return harness.outputStats->uninitCalls.load() > outputUninitBefore; },
        4000ms));
    ASSERT_TRUE(pumpUntil(
        [&harness, decoderInitBeforeSwitch]() {
            return harness.decoderStats->initCalls.load() > decoderInitBeforeSwitch;
        },
        4000ms));
    ASSERT_TRUE(
        pumpUntil([&harness]() { return harness.engine.trackStatus() == Engine::TrackStatus::Buffered; }, 4000ms));
    ASSERT_TRUE(
        pumpUntil([&harness]() { return harness.engine.playbackState() == Engine::PlaybackState::Playing; }, 4000ms));

    ASSERT_TRUE(pumpUntil([&harness]() { return harness.engine.position() > 0; }, 2000ms));
    const uint64_t restartedPosition = harness.engine.position();
    EXPECT_LT(restartedPosition, 400U);
}

FOOYIN_AUDIOENGINE_SENSITIVE_TEST(AudioEngineTest, FormatMismatchPreparationDiscardsPreparedStream)
{
    ensureCoreApplication();
    EngineHarness harness{false};
    AudioEngineTestAccessor::setGaplessEnabled(harness.engine, true);

    const Track currentTrack = harness.createTrack(u"prepared-mismatch-current.fyt"_s, 0, 120000);
    const Track nextTrack    = harness.createTrack(u"prepared-mismatch-next-sr2000.fyt"_s, 0, 120000);
    const auto currentItem   = makePlaybackItem(currentTrack, 1);
    const auto nextItem      = makePlaybackItem(nextTrack, 2);

    harness.engine.loadTrack(currentItem, false);
    ASSERT_TRUE(pumpUntil([&harness]() { return harness.engine.trackStatus() == Engine::TrackStatus::Loaded; }));

    harness.engine.play();
    ASSERT_TRUE(pumpUntil([&harness]() { return harness.engine.playbackState() == Engine::PlaybackState::Playing; }));
    ASSERT_TRUE(pumpUntil([&harness]() { return harness.engine.position() > 0; }, 3000ms));

    ASSERT_TRUE(AudioEngineTestAccessor::prepareNextTrackImmediate(harness.engine, nextItem, 1200));
    ASSERT_TRUE(AudioEngineTestAccessor::hasPreparedNext(harness.engine));
    EXPECT_FALSE(AudioEngineTestAccessor::preparedNextHasStream(harness.engine));
    EXPECT_GT(AudioEngineTestAccessor::preparedDecodePositionMs(harness.engine), 0U);
}

FOOYIN_AUDIOENGINE_REGULAR_TEST(AudioEngineTest, SetAnalysisDataSubscriptionsAcceptsRuntimeChanges)
{
    ensureCoreApplication();
    EngineHarness harness{false};

    harness.engine.setAnalysisDataSubscriptions(Engine::AnalysisDataTypes{Engine::AnalysisDataType::LevelFrameData});
    harness.engine.setAnalysisDataSubscriptions(Engine::AnalysisDataTypes{Engine::AnalysisDataType::PcmFrameData});
    harness.engine.setAnalysisDataSubscriptions(Engine::AnalysisDataTypes{});

    SUCCEED();
}

FOOYIN_AUDIOENGINE_SENSITIVE_TEST(AudioEngineTest, AudioEngineDispatchesPcmFrames)
{
    ensureCoreApplication();
    EngineHarness harness{false};

    std::mutex mutex;
    std::optional<PcmFrame> receivedFrame;

    QObject::connect(&harness.engine, &AudioEngine::pcmReady, &harness.engine,
                     [&mutex, &receivedFrame](const PcmFrame& frame) {
                         const std::scoped_lock lock{mutex};
                         receivedFrame = frame;
                     });

    PcmFrame expectedFrame;
    expectedFrame.format = AudioFormat{SampleFormat::F32, 1000, 2};
    expectedFrame.format.setChannelLayout(
        {AudioFormat::ChannelPosition::FrontLeft, AudioFormat::ChannelPosition::FrontRight});
    expectedFrame.frameCount   = PcmFrame::MaxFrames;
    expectedFrame.streamTimeMs = 123;
    std::ranges::fill(expectedFrame.samples, 0.1F);

    AudioEngineTestAccessor::deliverPcmFrame(harness.engine, expectedFrame);

    ASSERT_TRUE(pumpUntil(
        [&mutex, &receivedFrame]() {
            const std::scoped_lock lock{mutex};
            return receivedFrame.has_value();
        },
        1000ms));

    const PcmFrame frame = [&mutex, &receivedFrame]() {
        const std::scoped_lock lock{mutex};
        return *receivedFrame;
    }();

    EXPECT_EQ(frame.frameCount, PcmFrame::MaxFrames);
    EXPECT_EQ(frame.format.sampleFormat(), SampleFormat::F32);
    EXPECT_EQ(frame.format.channelCount(), 2);
    EXPECT_EQ(frame.format.sampleRate(), 1000);
    EXPECT_EQ(frame.streamTimeMs, 123);
    EXPECT_TRUE(frame.format.hasChannelLayout());
    ASSERT_EQ(frame.format.channelLayoutView().size(), 2);
    EXPECT_EQ(frame.format.channelLayoutView()[0], AudioFormat::ChannelPosition::FrontLeft);
    EXPECT_EQ(frame.format.channelLayoutView()[1], AudioFormat::ChannelPosition::FrontRight);
    EXPECT_EQ(frame.sampleCount(), static_cast<size_t>(PcmFrame::MaxFrames) * 2);
    EXPECT_FALSE(frame.interleavedSamples().empty());
    EXPECT_FLOAT_EQ(frame.interleavedSamples().front(), 0.1F);
    EXPECT_FLOAT_EQ(frame.interleavedSamples().back(), 0.1F);
}

FOOYIN_AUDIOENGINE_REGULAR_TEST(AudioEngineTest, VisualisationBackendUsesPresentedPosition)
{
    ensureCoreApplication();

    const QTemporaryDir tempDir;
    const QString settingsPath = tempDir.filePath(u"settings.ini"_s);
    SettingsManager settings{settingsPath};
    registerMinimalEngineSettings(settings, false, false);

    auto loader = std::make_shared<AudioLoader>();
    DspRegistry registry;
    auto visualisationBackend = std::make_shared<VisualisationBackend>();
    AudioEngine engine{loader, &settings, &registry, visualisationBackend};

    AudioEngineTestAccessor::publishPosition(engine, 1000, 120, 1.0, AudioClock::UpdateMode::Discontinuity, false);

    EXPECT_EQ(AudioEngineTestAccessor::visualisationCurrentTimeMs(engine), 880);
}

FOOYIN_AUDIOENGINE_REGULAR_TEST(AudioEngineTest, UpdateLiveDspSettingsHandlesUnknownInstanceGracefully)
{
    ensureCoreApplication();
    EngineHarness harness{false};

    Engine::LiveDspSettingsUpdate update;
    update.scope      = Engine::DspChainScope::Master;
    update.instanceId = 999;
    update.settings   = QByteArrayLiteral("ignored");
    update.revision   = 1;

    harness.engine.updateLiveDspSettings(update);

    SUCCEED();
}

FOOYIN_AUDIOENGINE_SENSITIVE_TEST(AudioEngineTest, ManualChangeCrossfadeReanchorsPositionWithoutStoppingPlayback)
{
    ensureCoreApplication();
    EngineHarness harness{/*enablePauseStopFade=*/false, /*enableManualCrossfade=*/true};

    const Track firstTrack  = harness.createTrack(u"manual-crossfade-first.fyt"_s, 0, 120000);
    const Track secondTrack = harness.createTrack(u"manual-crossfade-second.fyt"_s, 0, 120000);

    harness.engine.loadTrack(makePlaybackItem(firstTrack, 1), false);
    ASSERT_TRUE(pumpUntil([&harness]() { return harness.engine.trackStatus() == Engine::TrackStatus::Loaded; }));

    harness.engine.play();
    ASSERT_TRUE(pumpUntil([&harness]() { return harness.engine.playbackState() == Engine::PlaybackState::Playing; }));
    ASSERT_TRUE(pumpUntil([&harness]() { return harness.engine.position() > 0; }, 3000ms));

    const uint64_t positionBeforeSwitch = harness.engine.position();
    const int outputUninitBefore        = harness.outputStats->uninitCalls.load();
    const int decoderInitBefore         = harness.decoderStats->initCalls.load();

    harness.engine.loadTrack(makePlaybackItem(secondTrack, 2), true);

    ASSERT_TRUE(pumpUntil(
        [&harness, decoderInitBefore]() { return harness.decoderStats->initCalls.load() > decoderInitBefore; },
        4000ms));
    ASSERT_TRUE(
        pumpUntil([&harness]() { return harness.engine.trackStatus() == Engine::TrackStatus::Buffered; }, 4000ms));
    ASSERT_TRUE(
        pumpUntil([&harness]() { return harness.engine.playbackState() == Engine::PlaybackState::Playing; }, 4000ms));

    ASSERT_TRUE(pumpUntil([&harness]() { return harness.engine.position() <= 300; }, 2000ms));
    const uint64_t resetPosition = harness.engine.position();
    ASSERT_TRUE(
        pumpUntil([&harness, resetPosition]() { return harness.engine.position() >= (resetPosition + 20); }, 4000ms));

    EXPECT_GT(positionBeforeSwitch, 0U);
    EXPECT_EQ(harness.outputStats->uninitCalls.load(), outputUninitBefore);
}

FOOYIN_AUDIOENGINE_SENSITIVE_TEST(AudioEngineTest, NonCueTracksDoNotForceEndAtMetadataDurationBoundary)
{
    ensureCoreApplication();
    EngineHarness harness{false};

    static constexpr uint64_t trackDurationMs = 200;
    const Track track = harness.createTrack(u"non-cue-duration-boundary.fyt"_s, 0, trackDurationMs);
    harness.engine.loadTrack(makePlaybackItem(track, 1), false);
    ASSERT_TRUE(pumpUntil([&harness]() { return harness.engine.trackStatus() == Engine::TrackStatus::Loaded; }));

    harness.engine.play();
    ASSERT_TRUE(pumpUntil([&harness]() { return harness.engine.playbackState() == Engine::PlaybackState::Playing; }));

    ASSERT_TRUE(pumpUntil([&harness]() { return harness.engine.position() > trackDurationMs + 600; }, 3000ms));
    EXPECT_NE(harness.engine.trackStatus(), Engine::TrackStatus::End);
}

FOOYIN_AUDIOENGINE_REGULAR_TEST(AudioEngineTest, TimelineTransitionHintsAreDisabledForRepeatTrackPlayback)
{
    Track finiteTrack{u"/music/test.fyt"_s};
    finiteTrack.setDuration(6000);

    EXPECT_TRUE(AudioEngine::shouldEnableTimelineTransitionHints(finiteTrack, AudioDecoder::PlaybackHints{}));

    AudioDecoder::PlaybackHints repeatHints{};
    repeatHints.setFlag(AudioDecoder::PlaybackHint::RepeatTrackEnabled, true);
    EXPECT_FALSE(AudioEngine::shouldEnableTimelineTransitionHints(finiteTrack, repeatHints));

    Track unknownDuration{u"/music/test.fyt"_s};
    unknownDuration.setDuration(0);
    EXPECT_FALSE(AudioEngine::shouldEnableTimelineTransitionHints(unknownDuration, AudioDecoder::PlaybackHints{}));
}
} // namespace Fooyin::Testing
