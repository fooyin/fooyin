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

#include "core/engine/visualisationbackend.h"

#include <core/engine/pcmframe.h>

#include <gtest/gtest.h>

#include <numbers>
#include <ranges>

namespace {
Fooyin::PcmFrame makeStereoSineFrame(int frameCount, int sampleRate, int leftBin, int rightBin, uint64_t streamTimeMs,
                                     uint32_t streamId = 0)
{
    static constexpr float twoPi = std::numbers::pi_v<float> * 2.0F;

    Fooyin::PcmFrame frame;
    frame.format       = Fooyin::AudioFormat{Fooyin::SampleFormat::F32, sampleRate, 2};
    frame.frameCount   = frameCount;
    frame.streamTimeMs = streamTimeMs;
    frame.streamId     = streamId;

    for(size_t index{0}; std::cmp_less(index, frameCount); ++index) {
        const float leftPhase
            = twoPi * static_cast<float>(leftBin) * static_cast<float>(index) / static_cast<float>(frameCount);
        const float rightPhase
            = twoPi * static_cast<float>(rightBin) * static_cast<float>(index) / static_cast<float>(frameCount);
        frame.samples[index * 2]         = std::sin(leftPhase);
        frame.samples[((index * 2) + 1)] = std::sin(rightPhase);
    }

    return frame;
}

TEST(VisualisationBackendTest, ComputesSpectrumForMonoPcmBacklog)
{
    static constexpr int fftSize    = 1024;
    static constexpr int sampleRate = 1024;
    static constexpr int targetBin  = 7;
    static constexpr float twoPi    = std::numbers::pi_v<float> * 2.0F;

    Fooyin::VisualisationBackend backend;
    const auto token = backend.registerSession();
    backend.requestBacklog(token, 1000);

    Fooyin::PcmFrame frame;
    frame.format       = Fooyin::AudioFormat{Fooyin::SampleFormat::F32, sampleRate, 1};
    frame.frameCount   = fftSize;
    frame.streamTimeMs = 0;

    for(int index{0}; index < fftSize; ++index) {
        const float phase
            = twoPi * static_cast<float>(targetBin) * static_cast<float>(index) / static_cast<float>(fftSize);
        frame.samples[static_cast<size_t>(index)] = std::sin(phase);
    }

    backend.appendFrame(frame);

    Fooyin::VisualisationSession::SpectrumWindow spectrum;
    ASSERT_TRUE(backend.getSpectrumWindow(spectrum, 500, fftSize, {},
                                          Fooyin::VisualisationSession::SpectrumWindowFunction::Hann));
    ASSERT_TRUE(spectrum.isValid());
    EXPECT_EQ(spectrum.fftSize, fftSize);
    EXPECT_EQ(spectrum.sampleRate, sampleRate);
    EXPECT_EQ(spectrum.startTimeMs, 0);
    ASSERT_EQ(spectrum.binCount(), (fftSize / 2) + 1);

    const auto dominant = std::ranges::max_element(spectrum.magnitudes);
    ASSERT_NE(dominant, spectrum.magnitudes.end());
    EXPECT_EQ(std::distance(spectrum.magnitudes.begin(), dominant), targetBin);
    EXPECT_GT(*dominant, 0.9F);
}

TEST(VisualisationBackendTest, CompensatesWindowGainForSpectrumMagnitude)
{
    static constexpr int fftSize    = 1024;
    static constexpr int sampleRate = 1024;
    static constexpr int targetBin  = 7;
    static constexpr float twoPi    = std::numbers::pi_v<float> * 2.0F;

    Fooyin::PcmFrame frame;
    frame.format       = Fooyin::AudioFormat{Fooyin::SampleFormat::F32, sampleRate, 1};
    frame.frameCount   = fftSize;
    frame.streamTimeMs = 0;

    for(int index{0}; index < fftSize; ++index) {
        const float phase
            = twoPi * static_cast<float>(targetBin) * static_cast<float>(index) / static_cast<float>(fftSize);
        frame.samples[static_cast<size_t>(index)] = std::sin(phase);
    }

    for(const auto windowFunction : {Fooyin::VisualisationSession::SpectrumWindowFunction::Hann,
                                     Fooyin::VisualisationSession::SpectrumWindowFunction::BlackmanHarris,
                                     Fooyin::VisualisationSession::SpectrumWindowFunction::None}) {
        Fooyin::VisualisationBackend backend;
        const auto token = backend.registerSession();
        backend.requestBacklog(token, 1000);
        backend.appendFrame(frame);

        Fooyin::VisualisationSession::SpectrumWindow spectrum;
        ASSERT_TRUE(backend.getSpectrumWindow(spectrum, 500, fftSize, {}, windowFunction));
        ASSERT_TRUE(spectrum.isValid());

        const auto dominant = std::ranges::max_element(spectrum.magnitudes);
        ASSERT_NE(dominant, spectrum.magnitudes.end());
        EXPECT_EQ(std::distance(spectrum.magnitudes.begin(), dominant), targetBin);
        EXPECT_GT(*dominant, 0.9F);
        EXPECT_LT(*dominant, 1.1F);
    }
}

TEST(VisualisationBackendTest, DropsOldBacklogWhenScopedStreamGapExceedsTolerance)
{
    static constexpr auto frameCount               = 128;
    static constexpr auto sampleRate               = 1000;
    static constexpr uint64_t discontinuityStartMs = 400;
    static constexpr uint32_t streamId             = 1;

    Fooyin::VisualisationBackend backend;
    const auto token = backend.registerSession();
    backend.requestBacklog(token, 1000);

    backend.appendFrame(makeStereoSineFrame(frameCount, sampleRate, 7, 19, 0, streamId));
    backend.appendFrame(makeStereoSineFrame(frameCount, sampleRate, 7, 19, 128, streamId));

    Fooyin::VisualisationSession::PcmWindow window;
    ASSERT_TRUE(backend.getPcmWindowEndingAt(window, 128, 20, {}));
    EXPECT_EQ(window.startTimeMs, 108);

    backend.appendFrame(makeStereoSineFrame(frameCount, sampleRate, 7, 19, discontinuityStartMs, streamId));

    ASSERT_TRUE(backend.getPcmWindowEndingAt(window, 128, 20, {}));
    EXPECT_EQ(window.startTimeMs, 108);
    EXPECT_EQ(window.frameCount, 20);

    ASSERT_TRUE(backend.getPcmWindowEndingAt(window, 128, 200, {}));
    EXPECT_EQ(window.startTimeMs, 0);
    EXPECT_EQ(window.frameCount, frameCount);

    ASSERT_TRUE(backend.getPcmWindowEndingAt(window, discontinuityStartMs, 200, {}));
    EXPECT_EQ(window.startTimeMs, 0);
    EXPECT_EQ(window.frameCount, frameCount);
}

TEST(VisualisationBackendTest, PreservesTimelineWhenPcmStreamChanges)
{
    static constexpr auto frameCount = 100;
    static constexpr auto sampleRate = 1000;

    Fooyin::VisualisationBackend backend;
    const auto token = backend.registerSession();
    backend.requestBacklog(token, 2000);

    for(uint64_t streamTimeMs{10000}; streamTimeMs < 10400; streamTimeMs += 100) {
        backend.appendFrame(makeStereoSineFrame(frameCount, sampleRate, 7, 19, streamTimeMs, 1));
    }

    backend.appendFrame(makeStereoSineFrame(frameCount, sampleRate, 7, 19, 0, 2));
    backend.appendFrame(makeStereoSineFrame(frameCount, sampleRate, 7, 19, 100, 2));

    Fooyin::VisualisationSession::PcmWindow window;
    ASSERT_TRUE(backend.getPcmWindowEndingAt(window, 600, 20, {}));

    EXPECT_EQ(window.startTimeMs, 580);
    EXPECT_EQ(window.frameCount, 20);

    ASSERT_TRUE(backend.getPcmWindowEndingAt(window, 420, 40, {}));
    EXPECT_LT(window.startTimeMs, 400);
}

TEST(VisualisationBackendTest, StreamScopedTimeUpdateSwitchesTimelineExplicitly)
{
    static constexpr auto frameCount = 100;
    static constexpr auto sampleRate = 1000;

    Fooyin::VisualisationBackend backend;
    const auto token = backend.registerSession();
    backend.requestBacklog(token, 2000);

    backend.setCurrentTimeMs(1, 180000);
    backend.appendFrame(makeStereoSineFrame(frameCount, sampleRate, 7, 19, 180000, 1));
    backend.setCurrentTimeMs(2, 0);
    backend.appendFrame(makeStereoSineFrame(frameCount, sampleRate, 7, 19, 0, 2));
    ASSERT_GT(backend.currentTimeMs(), 0);
    ASSERT_LT(backend.currentTimeMs(), 500);
}

TEST(VisualisationBackendTest, ResolvesStartupSpectrumAfterFirstAnalysisFrame)
{
    static constexpr auto frameCount = Fooyin::PcmFrame::MaxFrames;
    static constexpr auto sampleRate = 44100;
    static constexpr auto fftSize    = 8192;

    Fooyin::VisualisationBackend backend;
    const auto token = backend.registerSession();
    backend.requestBacklog(token, 2000);

    backend.appendFrame(makeStereoSineFrame(frameCount, sampleRate, 7, 19, 0, 1));

    Fooyin::VisualisationSession::SpectrumWindow spectrum;
    ASSERT_TRUE(backend.getSpectrumWindowEndingAt(spectrum, 23, fftSize, {},
                                                  Fooyin::VisualisationSession::SpectrumWindowFunction::Hann));
    ASSERT_TRUE(spectrum.isValid());
    EXPECT_LT(spectrum.fftSize, fftSize);
    EXPECT_GE(spectrum.fftSize, frameCount / 2);
    EXPECT_EQ(spectrum.sampleRate, sampleRate);
    EXPECT_LE(spectrum.startTimeMs, 12);
}

TEST(VisualisationBackendTest, ChannelSelectionsSupportSingleChannelMidAndSide)
{
    static constexpr auto fftSize    = 1024;
    static constexpr auto sampleRate = 1024;
    static constexpr auto leftBin    = 7;
    static constexpr auto rightBin   = 19;

    Fooyin::VisualisationBackend backend;
    const auto token = backend.registerSession();
    backend.requestBacklog(token, 1000);
    backend.appendFrame(makeStereoSineFrame(fftSize, sampleRate, leftBin, rightBin, 0));

    Fooyin::VisualisationSession::SpectrumWindow leftSpectrum;
    ASSERT_TRUE(backend.getSpectrumWindow(
        leftSpectrum, 500, fftSize,
        Fooyin::VisualisationSession::ChannelSelection{
            .mixMode = Fooyin::VisualisationSession::ChannelSelection::MixMode::SingleChannel, .channelIndex = 0},
        Fooyin::VisualisationSession::SpectrumWindowFunction::Hann));
    const auto leftDominant = std::ranges::max_element(leftSpectrum.magnitudes);
    ASSERT_NE(leftDominant, leftSpectrum.magnitudes.end());
    EXPECT_EQ(std::distance(leftSpectrum.magnitudes.begin(), leftDominant), leftBin);

    Fooyin::VisualisationSession::SpectrumWindow rightSpectrum;
    ASSERT_TRUE(backend.getSpectrumWindow(
        rightSpectrum, 500, fftSize,
        Fooyin::VisualisationSession::ChannelSelection{
            .mixMode = Fooyin::VisualisationSession::ChannelSelection::MixMode::SingleChannel, .channelIndex = 1},
        Fooyin::VisualisationSession::SpectrumWindowFunction::Hann));
    const auto rightDominant = std::ranges::max_element(rightSpectrum.magnitudes);
    ASSERT_NE(rightDominant, rightSpectrum.magnitudes.end());
    EXPECT_EQ(std::distance(rightSpectrum.magnitudes.begin(), rightDominant), rightBin);

    Fooyin::VisualisationSession::PcmWindow midWindow;
    ASSERT_TRUE(
        backend.getPcmWindowEndingAt(midWindow, 1000, 1000,
                                     Fooyin::VisualisationSession::ChannelSelection{
                                         .mixMode = Fooyin::VisualisationSession::ChannelSelection::MixMode::Mid}));
    ASSERT_EQ(midWindow.format.channelCount(), 1);
    EXPECT_NEAR(midWindow.samples[0], 0.0F, 1.0e-4F);

    Fooyin::VisualisationSession::PcmWindow sideWindow;
    ASSERT_TRUE(
        backend.getPcmWindowEndingAt(sideWindow, 1000, 1000,
                                     Fooyin::VisualisationSession::ChannelSelection{
                                         .mixMode = Fooyin::VisualisationSession::ChannelSelection::MixMode::Side}));
    ASSERT_EQ(sideWindow.format.channelCount(), 1);
    EXPECT_NEAR(sideWindow.samples[0], 0.0F, 1.0e-4F);
}

TEST(VisualisationBackendTest, FormatChangeDropsOldHistory)
{
    static constexpr int fftSize = 1024;

    Fooyin::VisualisationBackend backend;
    const auto token = backend.registerSession();
    backend.requestBacklog(token, 2000);

    backend.appendFrame(makeStereoSineFrame(fftSize, 44100, 7, 19, 0));
    backend.appendFrame(makeStereoSineFrame(fftSize, 44100, 7, 19, 23));

    Fooyin::PcmFrame monoFrame;
    monoFrame.format       = Fooyin::AudioFormat{Fooyin::SampleFormat::F32, 48000, 1};
    monoFrame.frameCount   = fftSize;
    monoFrame.streamTimeMs = 1000;
    std::fill_n(monoFrame.samples.data(), fftSize, 0.2F);
    backend.appendFrame(monoFrame);

    Fooyin::VisualisationSession::PcmWindow window;
    ASSERT_TRUE(backend.getPcmWindowEndingAt(window, 23, 20, {}));
    EXPECT_LT(window.startTimeMs, 23);
    EXPECT_EQ(window.format.sampleRate(), 48000);
    EXPECT_EQ(window.format.channelCount(), 1);

    ASSERT_TRUE(backend.getPcmWindowEndingAt(window, 1021, 20, {}));
    EXPECT_EQ(window.format.sampleRate(), 48000);
    EXPECT_EQ(window.format.channelCount(), 1);

    backend.appendFrame(makeStereoSineFrame(fftSize, 44100, 7, 19, 2000));

    ASSERT_TRUE(backend.getPcmWindowEndingAt(window, 2021, 20, {}));
    EXPECT_EQ(window.format.sampleRate(), 44100);
    EXPECT_EQ(window.format.channelCount(), 2);
}
} // namespace
