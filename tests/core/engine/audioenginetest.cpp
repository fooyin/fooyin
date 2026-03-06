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

#include "core/engine/dsp/dspregistry.h"
#include "core/internalcoresettings.h"

#include <core/coresettings.h>
#include <core/engine/audioloader.h>
#include <utils/settings/settingsmanager.h>

#include <QCoreApplication>
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

namespace {
struct DecoderStats
{
    std::atomic<int> initCalls{0};
    std::atomic<int> seekCalls{0};
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

    mutable std::mutex mutex;
    double lastVolume{1.0};
    QString lastDevice;
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
        return 320;
    }

    std::optional<AudioFormat> init(const AudioSource& source, const Track& track, DecoderOptions options) override
    {
        Q_UNUSED(source)
        Q_UNUSED(options)

        ++m_stats->initCalls;
        m_track     = track;
        m_position  = track.offset();
        m_started   = false;
        m_initValid = true;
        return m_format;
    }

    void start() override
    {
        m_started = true;
    }

    void stop() override
    {
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
    { }

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
        return {.freeFrames = m_bufferFrames, .queuedFrames = 0, .delay = 0.0};
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
        return QStringLiteral("FormatShiftDsp");
    }

    QString id() const override
    {
        return QStringLiteral("test.dsp.format_shift");
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
        , engine{loader, &settings, &registry}
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
    AudioEngine engine;
};

} // namespace

TEST(AudioEngineTest, LoadTrackFullReinitInitialisesDecoderAndOutput)
{
    ensureCoreApplication();
    EngineHarness harness{false};

    const Track track = harness.createTrack(u"full-reinit.fyt"_s, 0, 100000);
    harness.engine.loadTrack(track, false);

    ASSERT_TRUE(pumpUntil([&harness]() { return harness.engine.trackStatus() == Engine::TrackStatus::Loaded; }));
    EXPECT_EQ(harness.engine.playbackState(), Engine::PlaybackState::Stopped);
    EXPECT_GE(harness.decoderStats->initCalls.load(), 1);
    EXPECT_EQ(harness.outputStats->initCalls.load(), 1);
    EXPECT_EQ(harness.outputStats->uninitCalls.load(), 0);
}

TEST(AudioEngineTest, ContiguousSegmentSwitchDoesNotReinitDecoderOrOutput)
{
    ensureCoreApplication();
    EngineHarness harness{false};

    const Track firstTrack = harness.createTrack(u"segments.fyt"_s, 0, 100000);
    harness.engine.loadTrack(firstTrack, false);
    ASSERT_TRUE(pumpUntil([&harness]() { return harness.engine.trackStatus() == Engine::TrackStatus::Loaded; }));

    harness.engine.play();
    ASSERT_TRUE(pumpUntil([&harness]() { return harness.engine.playbackState() == Engine::PlaybackState::Playing; }));

    const int decoderInitBefore  = harness.decoderStats->initCalls.load();
    const int outputInitBefore   = harness.outputStats->initCalls.load();
    const int outputUninitBefore = harness.outputStats->uninitCalls.load();

    Track nextSegment = firstTrack;
    nextSegment.setOffset(firstTrack.offset() + firstTrack.duration());
    nextSegment.setDuration(firstTrack.duration());

    harness.engine.loadTrack(nextSegment, false);
    QCoreApplication::processEvents(QEventLoop::AllEvents, 10);

    EXPECT_EQ(harness.decoderStats->initCalls.load(), decoderInitBefore);
    EXPECT_EQ(harness.outputStats->initCalls.load(), outputInitBefore);
    EXPECT_EQ(harness.outputStats->uninitCalls.load(), outputUninitBefore);
    EXPECT_EQ(harness.engine.playbackState(), Engine::PlaybackState::Playing);
    EXPECT_NE(harness.engine.trackStatus(), Engine::TrackStatus::Invalid);
}

TEST(AudioEngineTest, PlayPauseStopWithFadeCompletesStateTransitions)
{
    ensureCoreApplication();
    EngineHarness harness{/*enablePauseStopFade=*/true};

    const Track track = harness.createTrack(u"fade-smoke.fyt"_s, 0, 100000);
    harness.engine.loadTrack(track, false);
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

TEST(AudioEngineTest, SeekDiscontinuityPublishesNearTargetQuickly)
{
    ensureCoreApplication();
    EngineHarness harness{false};

    const Track track = harness.createTrack(u"seek-quick-discontinuity.fyt"_s, 0, 120000);
    harness.engine.loadTrack(track, false);
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

TEST(AudioEngineTest, SeekWithRequestPublishesMatchingRequestIds)
{
    ensureCoreApplication();
    EngineHarness harness{false};

    const Track track = harness.createTrack(u"seek-request-id.fyt"_s, 0, 120000);
    harness.engine.loadTrack(track, false);
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

TEST(AudioEngineTest, SeekWithRequestFromStoppedLoadsTrackAndStartsPlayback)
{
    ensureCoreApplication();
    EngineHarness harness{false};

    const Track track = harness.createTrack(u"seek-from-stopped.fyt"_s, 0, 120000);
    harness.engine.loadTrack(track, false);
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

TEST(AudioEngineTest, PrepareNextTrackInvalidTrackEmitsNotReady)
{
    ensureCoreApplication();
    EngineHarness harness{false};

    bool received{false};
    bool ready{true};
    uint64_t requestId{0};
    Track signalTrack;

    QObject::connect(&harness.engine, &AudioEngine::nextTrackReadiness, &harness.engine,
                     [&received, &signalTrack, &ready, &requestId](const Track& track, bool trackReady, uint64_t id) {
                         received    = true;
                         signalTrack = track;
                         ready       = trackReady;
                         requestId   = id;
                     });

    harness.engine.prepareNextTrack(Track{}, 77);

    ASSERT_TRUE(pumpUntil([&received]() { return received; }, 1000ms));
    EXPECT_FALSE(signalTrack.isValid());
    EXPECT_FALSE(ready);
    EXPECT_EQ(requestId, 77U);
}

TEST(AudioEngineTest, SetVolumeClampsAndPropagatesToOutput)
{
    ensureCoreApplication();
    EngineHarness harness{false};

    const Track track = harness.createTrack(u"volume-clamp.fyt"_s, 0, 100000);
    harness.engine.loadTrack(track, false);
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

TEST(AudioEngineTest, SetAudioOutputReinitializesLoadedOutput)
{
    ensureCoreApplication();
    EngineHarness harness{false};

    const Track track = harness.createTrack(u"switch-output.fyt"_s, 0, 100000);
    harness.engine.loadTrack(track, false);
    ASSERT_TRUE(pumpUntil([&harness]() { return harness.engine.trackStatus() == Engine::TrackStatus::Loaded; }));
    ASSERT_EQ(harness.outputStats->initCalls.load(), 1);

    auto newOutputStats = std::make_shared<OutputStats>();
    harness.engine.setAudioOutput([stats = newOutputStats]() { return std::make_unique<FakeAudioOutput>(stats); },
                                  QString{});

    ASSERT_TRUE(pumpUntil([&harness]() { return harness.outputStats->uninitCalls.load() >= 1; }, 2000ms));
    ASSERT_TRUE(pumpUntil([&newOutputStats]() { return newOutputStats->initCalls.load() >= 1; }, 2000ms));
}

TEST(AudioEngineTest, SetDspChainWithFormatChangeReinitializesOutput)
{
    ensureCoreApplication();
    EngineHarness harness{false};

    harness.registry.registerDsp({.id      = QStringLiteral("test.dsp.format_shift"),
                                  .name    = QStringLiteral("FormatShift"),
                                  .factory = []() { return std::make_unique<FormatShiftDsp>(); }});

    const Track track = harness.createTrack(u"dsp-format-change.fyt"_s, 0, 100000);
    harness.engine.loadTrack(track, false);
    ASSERT_TRUE(pumpUntil([&harness]() { return harness.engine.trackStatus() == Engine::TrackStatus::Loaded; }));
    const int initBefore = harness.outputStats->initCalls.load();

    Engine::DspDefinition shift;
    shift.id = QStringLiteral("test.dsp.format_shift");

    Engine::DspChains chains;
    chains.masterChain.push_back(shift);
    harness.engine.setDspChain(chains);

    ASSERT_TRUE(
        pumpUntil([&harness, initBefore]() { return harness.outputStats->initCalls.load() > initBefore; }, 3000ms));
}

TEST(AudioEngineTest, SetAnalysisDataSubscriptionsAcceptsRuntimeChanges)
{
    ensureCoreApplication();
    EngineHarness harness{false};

    harness.engine.setAnalysisDataSubscriptions(Engine::AnalysisDataTypes{Engine::AnalysisDataType::LevelFrameData});
    harness.engine.setAnalysisDataSubscriptions(Engine::AnalysisDataTypes{});

    SUCCEED();
}

TEST(AudioEngineTest, UpdateLiveDspSettingsHandlesUnknownInstanceGracefully)
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

TEST(AudioEngineTest, ManualChangeCrossfadeReanchorsPositionWithoutStoppingPlayback)
{
    ensureCoreApplication();
    EngineHarness harness{/*enablePauseStopFade=*/false, /*enableManualCrossfade=*/true};

    const Track firstTrack  = harness.createTrack(u"manual-crossfade-first.fyt"_s, 0, 120000);
    const Track secondTrack = harness.createTrack(u"manual-crossfade-second.fyt"_s, 0, 120000);

    harness.engine.loadTrack(firstTrack, false);
    ASSERT_TRUE(pumpUntil([&harness]() { return harness.engine.trackStatus() == Engine::TrackStatus::Loaded; }));

    harness.engine.play();
    ASSERT_TRUE(pumpUntil([&harness]() { return harness.engine.playbackState() == Engine::PlaybackState::Playing; }));
    ASSERT_TRUE(pumpUntil([&harness]() { return harness.engine.position() > 0; }, 3000ms));

    const uint64_t positionBeforeSwitch = harness.engine.position();
    const int outputUninitBefore        = harness.outputStats->uninitCalls.load();
    const int decoderInitBefore         = harness.decoderStats->initCalls.load();

    harness.engine.loadTrack(secondTrack, true);

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

TEST(AudioEngineTest, NonCueTracksDoNotForceEndAtMetadataDurationBoundary)
{
    ensureCoreApplication();
    EngineHarness harness{false};

    static constexpr uint64_t trackDurationMs = 200;
    const Track track = harness.createTrack(u"non-cue-duration-boundary.fyt"_s, 0, trackDurationMs);
    harness.engine.loadTrack(track, false);
    ASSERT_TRUE(pumpUntil([&harness]() { return harness.engine.trackStatus() == Engine::TrackStatus::Loaded; }));

    harness.engine.play();
    ASSERT_TRUE(pumpUntil([&harness]() { return harness.engine.playbackState() == Engine::PlaybackState::Playing; }));

    ASSERT_TRUE(pumpUntil([&harness]() { return harness.engine.position() > trackDurationMs + 600; }, 3000ms));
    EXPECT_NE(harness.engine.trackStatus(), Engine::TrackStatus::End);
}

TEST(AudioEngineTest, TimelineTransitionHintsAreDisabledForRepeatTrackPlayback)
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
