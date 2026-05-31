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

#pragma once

#include "fycore_export.h"

#include <core/engine/dsp/realfft.h>
#include <core/engine/pcmframe.h>
#include <core/engine/visualisationservice.h>

#include <atomic>
#include <chrono>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>

namespace Fooyin {
class FYCORE_EXPORT VisualisationBackend
{
public:
    using SessionToken           = uint64_t;
    using ChannelSelection       = VisualisationSession::ChannelSelection;
    using SpectrumWindowFunction = VisualisationSession::SpectrumWindowFunction;

    VisualisationBackend();

    SessionToken registerSession();
    bool unregisterSession(SessionToken token);
    void requestBacklog(SessionToken token, uint64_t durationMs);

    [[nodiscard]] bool hasActiveSessions() const;

    void setCurrentTimeMs(uint64_t currentTimeMs);
    void setCurrentTimeMs(uint32_t streamId, uint64_t currentTimeMs);
    void setCurrentTimeMs(uint32_t streamId, uint64_t currentTimeMs,
                          std::chrono::steady_clock::time_point presentationTime);
    [[nodiscard]] uint64_t currentTimeMs() const;
    void setStopped();

    void appendFrame(const PcmFrame& frame);
    void reset();

    [[nodiscard]] bool getPcmWindow(VisualisationSession::PcmWindow& out, uint64_t centerTimeMs, uint64_t durationMs,
                                    const ChannelSelection& selection) const;
    [[nodiscard]] bool getPcmWindowEndingAt(VisualisationSession::PcmWindow& out, uint64_t endTimeMs,
                                            uint64_t durationMs, const ChannelSelection& selection) const;
    [[nodiscard]] bool getSpectrumWindow(VisualisationSession::SpectrumWindow& out, uint64_t centerTimeMs, int fftSize,
                                         const ChannelSelection& selection,
                                         SpectrumWindowFunction windowFunction = SpectrumWindowFunction::Hann) const;
    [[nodiscard]] bool getSpectrumWindowEndingAt(VisualisationSession::SpectrumWindow& out, uint64_t endTimeMs,
                                                 int fftSize, const ChannelSelection& selection,
                                                 SpectrumWindowFunction windowFunction
                                                 = SpectrumWindowFunction::Hann) const;

private:
    enum class WindowAnchor : uint8_t
    {
        Center = 0,
        End,
    };

    struct WindowRange
    {
        uint64_t startFrame{0};
        int frameCount{0};
    };

    struct WindowCacheKey
    {
        int fftSize{0};
        SpectrumWindowFunction function{SpectrumWindowFunction::Hann};

        [[nodiscard]] bool operator==(const WindowCacheKey&) const = default;
    };

    struct WindowCacheKeyHash
    {
        [[nodiscard]] size_t operator()(const WindowCacheKey& key) const;
    };

    [[nodiscard]] static uint64_t msToFrames(uint64_t ms, int sampleRate);
    [[nodiscard]] bool resolveWindow(WindowRange& out, uint64_t timeMs, int requestedFrameCount, int minimumFrameCount,
                                     WindowAnchor anchor) const;
    [[nodiscard]] bool resolveSpectrumWindowEndingAt(WindowRange& out, uint64_t endTimeMs, int requestedFrameCount,
                                                     int minimumFrameCount) const;
    [[nodiscard]] uint64_t mapSourceTimeToVisualTime(uint32_t streamId, uint64_t sourceTimeMs) const;
    [[nodiscard]] bool fillWindow(VisualisationSession::PcmWindow& out, uint64_t startFrame, int frameCount,
                                  const ChannelSelection& selection) const;
    [[nodiscard]] bool fillSpectrumWindow(VisualisationSession::SpectrumWindow& out,
                                          const VisualisationSession::PcmWindow& window,
                                          const ChannelSelection& selection,
                                          SpectrumWindowFunction windowFunction) const;

    void resetLocked();
    void ensureCapacity(size_t requiredFrames);
    [[nodiscard]] size_t requestedBacklogFrames(int sampleRate) const;
    void appendFrames(std::span<const float> samples, size_t frameCount, int channelCount, uint64_t startFrame);

    mutable std::shared_mutex m_mutex;
    std::unordered_map<SessionToken, uint64_t> m_backlogRequests;
    SessionToken m_nextSessionToken;

    AudioFormat m_format;
    std::vector<float> m_samples;
    size_t m_capacityFrames;
    size_t m_headFrame;
    size_t m_frameCount;
    uint64_t m_startStreamFrame;
    uint64_t m_nextStreamFrame;
    uint32_t m_currentStreamId;

    struct SourceTimeline
    {
        uint64_t nextFrame{0};
        int64_t visualTimeOffsetMs{0};
    };
    std::unordered_map<uint32_t, SourceTimeline> m_sourceTimelines;

    std::atomic<uint64_t> m_currentTimeMs;
    std::atomic<int64_t> m_currentTimeReferenceClockMs;
    std::atomic_bool m_currentTimePublished;
    std::atomic_bool m_currentTimeHasPresentationClock;

    mutable std::mutex m_fftMutex;
    mutable std::unordered_map<int, Dsp::RealFft> m_fftCache;
    mutable std::unordered_map<WindowCacheKey, std::vector<float>, WindowCacheKeyHash> m_windowCache;
    mutable std::vector<float> m_fftInputScratch;
    mutable std::vector<float> m_magnitudeScratch;
};
} // namespace Fooyin
