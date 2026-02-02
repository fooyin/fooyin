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
#include <QFile>
#include <QTemporaryDir>
#include <QVariant>

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <memory>
#include <thread>

using namespace std::chrono_literals;
using namespace Qt::StringLiterals;

namespace Fooyin::Testing {
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

Track makeTrack(const QString& filePath, uint64_t offsetMs, uint64_t durationMs)
{
    Track track{filePath};
    track.setOffset(offsetMs);
    track.setDuration(durationMs);
    track.setBitrate(320);
    return track;
}

void registerMinimalEngineSettings(SettingsManager& settings, bool enablePauseStopFade)
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
    settings.createSetting<Settings::Core::Internal::EngineCrossfading>(false, u"Engine/Crossfading"_s);
    settings.createSetting<Settings::Core::Internal::CrossfadingValues>(
        QVariant::fromValue(Engine::CrossfadingValues{}), u"Engine/CrossfadingValues"_s);
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

    std::optional<AudioFormat> init(const AudioSource&, const Track& track, DecoderOptions /*options*/) override
    {
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

    [[nodiscard]] OutputDevices getAllDevices(bool) override
    {
        return {};
    }

    int write(const AudioBuffer& buffer) override
    {
        if(!m_initialised || !buffer.isValid()) {
            return 0;
        }
        ++m_stats->writeCalls;
        return buffer.frameCount();
    }

    void setPaused(bool pause) override
    {
        m_paused = pause;
    }

    void setVolume(double volume) override
    {
        m_volume = volume;
    }

    void setDevice(const QString& device) override
    {
        m_device = device;
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

struct EngineHarness
{
    explicit EngineHarness(bool enablePauseStopFade)
        : tempDir{}
        , settingsPath{tempDir.filePath(u"settings.ini"_s)}
        , settings{settingsPath}
        , decoderStats{std::make_shared<DecoderStats>()}
        , outputStats{std::make_shared<OutputStats>()}
        , loader{std::make_shared<AudioLoader>()}
        , registry{}
        , engine{loader, &settings, &registry}
    {
        registerMinimalEngineSettings(settings, enablePauseStopFade);

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
    EXPECT_EQ(harness.engine.trackStatus(), Engine::TrackStatus::NoTrack);
}
} // namespace Fooyin::Testing
