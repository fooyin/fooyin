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

#include "visualisationbackend.h"

#include <algorithm>
#include <chrono>
#include <numbers>

#include <functional>
#include <mutex>
#include <ranges>
#include <utility>

constexpr uint64_t BacklogPaddingMs       = 100;
constexpr uint64_t ContinuityToleranceMs  = 100;
constexpr uint64_t DefaultBacklogDuration = 250;
constexpr int MinimumSpectrumFrameCount   = 256;

namespace {
int64_t steadyClockMs()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

int64_t steadyClockMs(std::chrono::steady_clock::time_point time)
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(time.time_since_epoch()).count();
}

uint64_t addSignedOffset(uint64_t value, int64_t offset)
{
    if(offset >= 0) {
        return value + static_cast<uint64_t>(offset);
    }

    const auto delta = static_cast<uint64_t>(-offset);
    return value > delta ? value - delta : 0;
}

int previousPowerOfTwo(int value)
{
    int power{1};
    while(power <= value / 2) {
        power *= 2;
    }
    return power;
}

float spectrumWindowGain(Fooyin::VisualisationSession::SpectrumWindowFunction windowFunction, float phase)
{
    using WindowFunction = Fooyin::VisualisationSession::SpectrumWindowFunction;

    switch(windowFunction) {
        case WindowFunction::BlackmanHarris:
            return 0.35875F - (0.48829F * std::cos(phase)) + (0.14128F * std::cos(2.0F * phase))
                 - (0.01168F * std::cos(3.0F * phase));
        case WindowFunction::Hann:
            return 0.5F * (1.0F - std::cos(phase));
        case WindowFunction::None:
            return 1.0F;
    }

    return 1.0F;
}

} // namespace

namespace Fooyin {
size_t VisualisationBackend::WindowCacheKeyHash::operator()(const WindowCacheKey& key) const
{
    const auto fftHash      = std::hash<int>{}(key.fftSize);
    const auto functionHash = std::hash<int>{}(static_cast<int>(key.function));
    return fftHash ^ (functionHash + 0x9e3779b9U + (fftHash << 6U) + (fftHash >> 2U));
}

VisualisationBackend::SessionToken VisualisationBackend::registerSession()
{
    const std::unique_lock lock{m_mutex};

    SessionToken token = m_nextSessionToken++;
    if(token == 0) {
        token = m_nextSessionToken++;
    }
    if(m_nextSessionToken == 0) {
        m_nextSessionToken = 1;
    }

    m_backlogRequests[token] = DefaultBacklogDuration;
    return token;
}

VisualisationBackend::VisualisationBackend()
    : m_nextSessionToken{1}
    , m_format{SampleFormat::F32, 0, 0}
    , m_capacityFrames{0}
    , m_headFrame{0}
    , m_frameCount{0}
    , m_startStreamFrame{0}
    , m_nextStreamFrame{0}
    , m_currentStreamId{0}
    , m_currentTimeMs{0}
    , m_currentTimeReferenceClockMs{0}
    , m_currentTimePublished{false}
    , m_currentTimeHasPresentationClock{false}
{ }

bool VisualisationBackend::unregisterSession(SessionToken token)
{
    const std::unique_lock lock{m_mutex};
    return m_backlogRequests.erase(token) > 0;
}

void VisualisationBackend::requestBacklog(SessionToken token, uint64_t durationMs)
{
    const std::unique_lock lock{m_mutex};
    if(auto it = m_backlogRequests.find(token); it != m_backlogRequests.end()) {
        it->second = durationMs;
    }
}

bool VisualisationBackend::hasActiveSessions() const
{
    const std::shared_lock lock{m_mutex};
    return !m_backlogRequests.empty();
}

void VisualisationBackend::setCurrentTimeMs(uint64_t currentTimeMs)
{
    setCurrentTimeMs(0, currentTimeMs);
}

void VisualisationBackend::setCurrentTimeMs(uint32_t streamId, uint64_t currentTimeMs)
{
    uint64_t visualTimeMs{currentTimeMs};

    {
        const std::shared_lock lock{m_mutex};
        visualTimeMs = mapSourceTimeToVisualTime(streamId, currentTimeMs);
    }

    m_currentTimeMs.store(visualTimeMs, std::memory_order_relaxed);
    m_currentTimeReferenceClockMs.store(0, std::memory_order_relaxed);
    m_currentTimePublished.store(true, std::memory_order_relaxed);
    m_currentTimeHasPresentationClock.store(false, std::memory_order_relaxed);
}

void VisualisationBackend::setCurrentTimeMs(uint32_t streamId, uint64_t currentTimeMs,
                                            std::chrono::steady_clock::time_point presentationTime)
{
    setCurrentTimeMs(streamId, currentTimeMs);
    m_currentTimeReferenceClockMs.store(steadyClockMs(presentationTime), std::memory_order_relaxed);
    m_currentTimeHasPresentationClock.store(true, std::memory_order_relaxed);
}

uint64_t VisualisationBackend::currentTimeMs() const
{
    const uint64_t currentTimeMs = m_currentTimeMs.load(std::memory_order_relaxed);
    if(currentTimeMs == 0 || !m_currentTimeHasPresentationClock.load(std::memory_order_relaxed)) {
        return currentTimeMs;
    }

    const int64_t referenceClockMs = m_currentTimeReferenceClockMs.load(std::memory_order_relaxed);
    if(referenceClockMs <= 0) {
        return currentTimeMs;
    }

    const int64_t elapsedMs = steadyClockMs() - referenceClockMs;
    if(elapsedMs >= 0) {
        return currentTimeMs + static_cast<uint64_t>(elapsedMs);
    }

    const auto earlyMs = static_cast<uint64_t>(-elapsedMs);
    return currentTimeMs > earlyMs ? currentTimeMs - earlyMs : 0;
}

void VisualisationBackend::setStopped()
{
    m_currentTimeMs.store(0, std::memory_order_relaxed);
    m_currentTimeReferenceClockMs.store(0, std::memory_order_relaxed);
    m_currentTimePublished.store(false, std::memory_order_relaxed);
    m_currentTimeHasPresentationClock.store(false, std::memory_order_relaxed);
    const std::unique_lock lock{m_mutex};
    m_currentStreamId = 0;
}

void VisualisationBackend::appendFrame(const PcmFrame& frame)
{
    const AudioFormat& format = frame.format;
    const int sampleRate      = format.sampleRate();
    const int channelCount    = format.channelCount();
    const int frameCount      = std::clamp(frame.frameCount, 0, PcmFrame::MaxFrames);

    if(frameCount <= 0 || !format.isValid() || format.sampleFormat() != SampleFormat::F32 || channelCount <= 0
       || channelCount > PcmFrame::MaxChannels || sampleRate <= 0) {
        return;
    }

    const size_t sampleCount = static_cast<size_t>(frameCount) * static_cast<size_t>(channelCount);
    if(sampleCount > frame.samples.size()) {
        return;
    }

    const std::unique_lock lock{m_mutex};
    if(m_backlogRequests.empty()) {
        return;
    }

    const uint32_t sourceKey          = frame.streamId;
    const uint64_t reportedStartFrame = msToFrames(frame.streamTimeMs, sampleRate);
    const uint64_t reportedNextFrame  = reportedStartFrame + static_cast<uint64_t>(frameCount);
    bool resetBacklog                 = false;
    const bool formatChanged          = m_format.isValid() && m_format != format;

    if(formatChanged) {
        resetBacklog = true;
    }
    else if(m_frameCount > 0) {
        if(const auto it = m_sourceTimelines.find(sourceKey); it != m_sourceTimelines.end()) {
            const uint64_t toleranceFrames   = std::max<uint64_t>(1, msToFrames(ContinuityToleranceMs, sampleRate));
            const bool sourceTimestampBehind = reportedStartFrame + toleranceFrames < it->second.nextFrame;
            const bool sourceTimestampAhead  = reportedStartFrame > it->second.nextFrame + toleranceFrames;

            if(sourceTimestampBehind || sourceTimestampAhead) {
                resetBacklog = true;
            }
        }
    }

    if(resetBacklog) {
        resetLocked();
        m_currentTimeMs.store(0, std::memory_order_relaxed);
        m_currentTimeReferenceClockMs.store(0, std::memory_order_relaxed);
        m_currentTimePublished.store(false, std::memory_order_relaxed);
        m_currentTimeHasPresentationClock.store(false, std::memory_order_relaxed);
    }

    if(!m_format.isValid()) {
        m_format = format;
    }

    if(m_frameCount == 0) {
        m_startStreamFrame = 0;
        m_nextStreamFrame  = 0;
    }

    const uint64_t visualStartFrame = m_nextStreamFrame;
    const uint64_t visualStartMs    = (visualStartFrame * 1000ULL) / static_cast<uint64_t>(sampleRate);

    auto& sourceTimeline              = m_sourceTimelines[sourceKey];
    sourceTimeline.nextFrame          = reportedNextFrame;
    sourceTimeline.visualTimeOffsetMs = static_cast<int64_t>(visualStartMs) - static_cast<int64_t>(frame.streamTimeMs);

    if(frame.streamId != 0) {
        m_currentStreamId = frame.streamId;
    }

    const size_t requiredFrames = std::max<size_t>(
        static_cast<size_t>(frameCount), requestedBacklogFrames(sampleRate) + static_cast<size_t>(frameCount));
    ensureCapacity(requiredFrames);
    appendFrames(std::span<const float>{frame.samples.data(), sampleCount}, static_cast<size_t>(frameCount),
                 channelCount, visualStartFrame);

    const uint64_t availableEndMs = (m_nextStreamFrame * 1000ULL) / static_cast<uint64_t>(sampleRate);
    if(availableEndMs > 0
       && (!m_currentTimePublished.load(std::memory_order_relaxed)
           || m_currentTimeMs.load(std::memory_order_relaxed) == 0)) {
        m_currentTimeMs.store(availableEndMs, std::memory_order_relaxed);
        m_currentTimePublished.store(true, std::memory_order_relaxed);
        m_currentTimeHasPresentationClock.store(false, std::memory_order_relaxed);
    }
}

void VisualisationBackend::reset()
{
    {
        const std::unique_lock lock{m_mutex};
        resetLocked();
    }
    setStopped();
}

bool VisualisationBackend::resolveWindow(WindowRange& out, uint64_t timeMs, int requestedFrameCount,
                                         int minimumFrameCount, const WindowAnchor anchor) const
{
    if(std::cmp_less(m_frameCount, minimumFrameCount) || !m_format.isValid() || m_format.sampleRate() <= 0
       || m_format.channelCount() <= 0 || requestedFrameCount < minimumFrameCount) {
        return false;
    }

    const int frameCount    = std::clamp(requestedFrameCount, minimumFrameCount, static_cast<int>(m_frameCount));
    const auto windowFrames = static_cast<uint64_t>(frameCount);
    const uint64_t availableStartFrame = m_startStreamFrame;
    const uint64_t availableEndFrame   = m_nextStreamFrame;

    if(availableEndFrame < availableStartFrame + windowFrames) {
        return false;
    }

    const uint64_t timeFrame = msToFrames(timeMs, m_format.sampleRate());
    uint64_t startFrame{0};

    if(anchor == WindowAnchor::Center) {
        const auto halfWindow              = static_cast<uint64_t>(frameCount / 2);
        const uint64_t requestedStartFrame = timeFrame > halfWindow ? timeFrame - halfWindow : 0;
        const uint64_t latestStartFrame    = availableEndFrame - windowFrames;
        startFrame                         = std::clamp(requestedStartFrame, availableStartFrame, latestStartFrame);
    }
    else {
        const uint64_t clampedEndFrame = std::clamp(timeFrame, availableStartFrame + windowFrames, availableEndFrame);
        startFrame                     = clampedEndFrame - windowFrames;
    }

    out.startFrame = startFrame;
    out.frameCount = frameCount;
    return true;
}

bool VisualisationBackend::resolveSpectrumWindowEndingAt(WindowRange& out, uint64_t endTimeMs, int requestedFrameCount,
                                                         int minimumFrameCount) const
{
    // Keep startup/track change windows anchored to playback time. The generic end-window resolver clamps to
    // the first full window, which makes larger FFT sizes appear frozen until playback catches up.
    if(std::cmp_less(m_frameCount, minimumFrameCount) || !m_format.isValid() || m_format.sampleRate() <= 0
       || m_format.channelCount() <= 0 || requestedFrameCount < minimumFrameCount) {
        return false;
    }

    const uint64_t availableStartFrame = m_startStreamFrame;
    const uint64_t availableEndFrame   = m_nextStreamFrame;
    const uint64_t timeFrame           = msToFrames(endTimeMs, m_format.sampleRate());
    const uint64_t clampedEndFrame     = std::clamp(timeFrame, availableStartFrame, availableEndFrame);

    if(clampedEndFrame <= availableStartFrame) {
        return false;
    }

    const uint64_t availableWindowFrames = clampedEndFrame - availableStartFrame;
    int frameCount                       = requestedFrameCount;
    if(std::cmp_less(availableWindowFrames, requestedFrameCount)) {
        frameCount = previousPowerOfTwo(static_cast<int>(availableWindowFrames));
    }

    if(frameCount < minimumFrameCount) {
        return false;
    }

    const auto windowFrames = static_cast<uint64_t>(frameCount);
    out.startFrame          = clampedEndFrame - windowFrames;
    out.frameCount          = frameCount;
    return true;
}

bool VisualisationBackend::getPcmWindow(VisualisationSession::PcmWindow& out, uint64_t centerTimeMs,
                                        uint64_t durationMs, const ChannelSelection& selection) const
{
    const std::shared_lock lock{m_mutex};

    WindowRange window;
    const int requestedFrames = std::max(1, m_format.framesForDuration(std::max<uint64_t>(1, durationMs)));
    if(!resolveWindow(window, centerTimeMs, requestedFrames, 1, WindowAnchor::Center)) {
        return false;
    }

    return fillWindow(out, window.startFrame, window.frameCount, selection);
}

bool VisualisationBackend::getPcmWindowEndingAt(VisualisationSession::PcmWindow& out, uint64_t endTimeMs,
                                                uint64_t durationMs, const ChannelSelection& selection) const
{
    const std::shared_lock lock{m_mutex};

    WindowRange window;
    const int requestedFrames = std::max(1, m_format.framesForDuration(std::max<uint64_t>(1, durationMs)));
    if(!resolveWindow(window, endTimeMs, requestedFrames, 1, WindowAnchor::End)) {
        return false;
    }

    return fillWindow(out, window.startFrame, window.frameCount, selection);
}

bool VisualisationBackend::getSpectrumWindow(VisualisationSession::SpectrumWindow& out, uint64_t centerTimeMs,
                                             int fftSize, const ChannelSelection& selection,
                                             SpectrumWindowFunction windowFunction) const
{
    VisualisationSession::PcmWindow window;

    {
        const std::shared_lock lock{m_mutex};

        WindowRange range;
        if(!resolveWindow(range, centerTimeMs, fftSize, 2, WindowAnchor::Center)) {
            return false;
        }

        if(!fillWindow(window, range.startFrame, range.frameCount, selection)) {
            return false;
        }
    }

    return fillSpectrumWindow(out, window, selection, windowFunction);
}

bool VisualisationBackend::getSpectrumWindowEndingAt(VisualisationSession::SpectrumWindow& out, uint64_t endTimeMs,
                                                     int fftSize, const ChannelSelection& selection,
                                                     SpectrumWindowFunction windowFunction) const
{
    VisualisationSession::PcmWindow window;

    {
        const std::shared_lock lock{m_mutex};

        WindowRange range;
        if(!resolveSpectrumWindowEndingAt(range, endTimeMs, fftSize, MinimumSpectrumFrameCount)) {
            return false;
        }

        if(!fillWindow(window, range.startFrame, range.frameCount, selection)) {
            return false;
        }
    }

    return fillSpectrumWindow(out, window, selection, windowFunction);
}

uint64_t VisualisationBackend::mapSourceTimeToVisualTime(uint32_t streamId, uint64_t sourceTimeMs) const
{
    if(const auto it = m_sourceTimelines.find(streamId); it != m_sourceTimelines.end()) {
        return addSignedOffset(sourceTimeMs, it->second.visualTimeOffsetMs);
    }

    if(m_format.isValid() && m_format.sampleRate() > 0 && m_nextStreamFrame > m_startStreamFrame) {
        return (m_nextStreamFrame * 1000ULL) / static_cast<uint64_t>(m_format.sampleRate());
    }

    return sourceTimeMs;
}

bool VisualisationBackend::fillWindow(VisualisationSession::PcmWindow& out, uint64_t startFrame, int frameCount,
                                      const ChannelSelection& selection) const
{
    out.format = m_format;

    const auto selectionMode = selection.mixMode;
    if(selectionMode != ChannelSelection::MixMode::AllChannels) {
        out.format.setChannelCount(1);
        out.format.clearChannelLayout();
    }

    const int sampleRate = m_format.sampleRate();

    out.frameCount  = frameCount;
    out.startTimeMs = (startFrame * 1000ULL) / static_cast<uint64_t>(sampleRate);
    out.samples.resize(static_cast<size_t>(frameCount) * static_cast<size_t>(out.format.channelCount()));

    const int sourceChannels = m_format.channelCount();

    const auto relativeStart = startFrame - m_startStreamFrame;
    if(selectionMode == ChannelSelection::MixMode::MonoAverage) {
        for(int frameIndex{0}; frameIndex < frameCount; ++frameIndex) {
            const size_t sourceFrame
                = (m_headFrame + relativeStart + static_cast<size_t>(frameIndex)) % m_capacityFrames;
            const size_t sourceBase = sourceFrame * static_cast<size_t>(sourceChannels);

            float sum{0.0F};
            for(int channel{0}; channel < sourceChannels; ++channel) {
                sum += m_samples[sourceBase + static_cast<size_t>(channel)];
            }

            out.samples[static_cast<size_t>(frameIndex)] = sum / static_cast<float>(sourceChannels);
        }
    }
    else if(selectionMode == ChannelSelection::MixMode::SingleChannel) {
        if(selection.channelIndex < 0 || selection.channelIndex >= sourceChannels) {
            return false;
        }

        const auto selectedChannel = static_cast<size_t>(selection.channelIndex);
        for(int frameIndex{0}; frameIndex < frameCount; ++frameIndex) {
            const size_t sourceFrame
                = (m_headFrame + relativeStart + static_cast<size_t>(frameIndex)) % m_capacityFrames;
            const size_t sourceBase = sourceFrame * static_cast<size_t>(sourceChannels);

            out.samples[static_cast<size_t>(frameIndex)] = m_samples[sourceBase + selectedChannel];
        }
    }
    else if(selectionMode == ChannelSelection::MixMode::Mid || selectionMode == ChannelSelection::MixMode::Side) {
        if(sourceChannels < 2) {
            return false;
        }

        for(int frameIndex{0}; frameIndex < frameCount; ++frameIndex) {
            const size_t sourceFrame
                = (m_headFrame + relativeStart + static_cast<size_t>(frameIndex)) % m_capacityFrames;
            const size_t sourceBase = sourceFrame * static_cast<size_t>(sourceChannels);

            const float left  = m_samples[sourceBase];
            const float right = m_samples[sourceBase + 1];

            out.samples[static_cast<size_t>(frameIndex)]
                = selectionMode == ChannelSelection::MixMode::Mid ? ((left + right) * 0.5F) : ((left - right) * 0.5F);
        }
    }
    else if(selectionMode == ChannelSelection::MixMode::AllChannels) {
        const auto channelCount = static_cast<size_t>(sourceChannels);

        for(int frameIndex{0}; frameIndex < frameCount; ++frameIndex) {
            const size_t sourceFrame
                = (m_headFrame + relativeStart + static_cast<size_t>(frameIndex)) % m_capacityFrames;
            const size_t sourceBase = sourceFrame * channelCount;
            const size_t destBase   = static_cast<size_t>(frameIndex) * channelCount;

            std::copy_n(m_samples.data() + static_cast<std::ptrdiff_t>(sourceBase), channelCount,
                        out.samples.data() + static_cast<std::ptrdiff_t>(destBase));
        }
    }
    else {
        return false;
    }

    return true;
}

bool VisualisationBackend::fillSpectrumWindow(VisualisationSession::SpectrumWindow& out,
                                              const VisualisationSession::PcmWindow& window,
                                              const ChannelSelection& selection,
                                              SpectrumWindowFunction windowFunction) const
{
    if(!window.isValid() || window.format.channelCount() <= 0 || window.frameCount < 2) {
        return false;
    }

    const std::scoped_lock fftLock{m_fftMutex};

    auto [it, inserted] = m_fftCache.try_emplace(window.frameCount);
    if(inserted || !it->second.isValid() || it->second.fftSize() != window.frameCount) {
        if(!it->second.reset(window.frameCount)) {
            m_fftCache.erase(it);
            return false;
        }
    }

    out.fftSize     = window.frameCount;
    out.sampleRate  = window.format.sampleRate();
    out.startTimeMs = window.startTimeMs;
    out.magnitudes.resize(static_cast<size_t>(it->second.binCount()));

    std::ranges::fill(out.magnitudes, 0.0);

    std::vector<float> magnitudes;
    magnitudes.resize(out.magnitudes.size(), 0.0);

    std::vector<float> fftInput(static_cast<size_t>(window.frameCount), 0.0);
    const auto samples     = window.interleavedSamples();
    const int channelCount = window.format.channelCount();

    const int transformChannelCount = selection.mixMode == ChannelSelection::MixMode::AllChannels ? channelCount : 1;
    const bool applyWindow          = window.frameCount > 2 && windowFunction != SpectrumWindowFunction::None;

    float magnitudeScale{1.0F};
    std::vector<float> windowGains;

    if(applyWindow) {
        windowGains.resize(static_cast<size_t>(window.frameCount), 1.0F);

        float gainSum{0.0F};
        const auto denom = static_cast<float>(window.frameCount - 1);

        for(int frameIndex{0}; frameIndex < window.frameCount; ++frameIndex) {
            const float phase = (static_cast<float>(frameIndex) * 2.0F * std::numbers::pi_v<float>) / denom;
            const float gain  = spectrumWindowGain(windowFunction, phase);
            windowGains[static_cast<size_t>(frameIndex)] = gain;
            gainSum += gain;
        }

        if(gainSum > 0.0F) {
            magnitudeScale = static_cast<float>(window.frameCount) / gainSum;
        }
    }

    for(int channel{0}; channel < transformChannelCount; ++channel) {
        if(!applyWindow) {
            for(int frameIndex{0}; frameIndex < window.frameCount; ++frameIndex) {
                const size_t sampleIndex = (static_cast<size_t>(frameIndex) * static_cast<size_t>(channelCount))
                                         + static_cast<size_t>(channel);
                fftInput[static_cast<size_t>(frameIndex)] = samples[sampleIndex];
            }
        }
        else {
            for(int frameIndex{0}; frameIndex < window.frameCount; ++frameIndex) {
                const size_t sampleIndex = (static_cast<size_t>(frameIndex) * static_cast<size_t>(channelCount))
                                         + static_cast<size_t>(channel);
                fftInput[static_cast<size_t>(frameIndex)]
                    = samples[sampleIndex] * windowGains[static_cast<size_t>(frameIndex)];
            }
        }

        if(!it->second.transformMagnitudes(fftInput, magnitudes)) {
            return false;
        }

        for(size_t bin{0}; bin < out.magnitudes.size(); ++bin) {
            out.magnitudes[bin] = std::max(out.magnitudes[bin], magnitudes[bin] * magnitudeScale);
        }
    }

    return true;
}

uint64_t VisualisationBackend::msToFrames(uint64_t ms, int sampleRate)
{
    if(sampleRate <= 0) {
        return 0;
    }

    return (ms * static_cast<uint64_t>(sampleRate)) / 1000ULL;
}

void VisualisationBackend::resetLocked()
{
    m_headFrame        = 0;
    m_frameCount       = 0;
    m_startStreamFrame = 0;
    m_nextStreamFrame  = 0;
    m_currentStreamId  = 0;
    m_format           = AudioFormat{SampleFormat::F32, 0, 0};
    m_sourceTimelines.clear();
}

void VisualisationBackend::ensureCapacity(size_t requiredFrames)
{
    if(!m_format.isValid() || m_format.channelCount() <= 0) {
        return;
    }

    const auto channelCount = static_cast<size_t>(m_format.channelCount());
    if(requiredFrames <= m_capacityFrames && m_samples.size() >= (requiredFrames * channelCount)) {
        return;
    }

    std::vector<float> newSamples(requiredFrames * channelCount, 0.0);

    for(size_t frameIndex{0}; frameIndex < m_frameCount; ++frameIndex) {
        if(m_samples.empty() || m_capacityFrames == 0) {
            break;
        }

        const size_t sourceFrame = (m_headFrame + frameIndex) % m_capacityFrames;
        const size_t sourceBase  = sourceFrame * channelCount;
        const size_t destBase    = frameIndex * channelCount;

        std::copy_n(m_samples.data() + sourceBase, channelCount, newSamples.data() + destBase);
    }

    m_samples        = std::move(newSamples);
    m_capacityFrames = requiredFrames;
    m_headFrame      = 0;
}

size_t VisualisationBackend::requestedBacklogFrames(int sampleRate) const
{
    uint64_t requestedBacklogMs{DefaultBacklogDuration};
    for(const auto& backlogMs : m_backlogRequests | std::views::values) {
        requestedBacklogMs = std::max(requestedBacklogMs, backlogMs);
    }

    const AudioFormat format{SampleFormat::F32, sampleRate, std::max(1, m_format.channelCount())};
    return static_cast<size_t>(format.framesForDuration(requestedBacklogMs + BacklogPaddingMs));
}

void VisualisationBackend::appendFrames(std::span<const float> samples, size_t frameCount, int channelCount,
                                        uint64_t startFrame)
{
    if(frameCount == 0 || channelCount <= 0 || m_capacityFrames == 0) {
        return;
    }

    if(m_frameCount + frameCount > m_capacityFrames) {
        const size_t droppedFrames = (m_frameCount + frameCount) - m_capacityFrames;
        m_headFrame                = (m_headFrame + droppedFrames) % m_capacityFrames;
        m_frameCount -= droppedFrames;
        m_startStreamFrame += droppedFrames;
    }

    const auto channels          = static_cast<size_t>(channelCount);
    const size_t tailFrame       = (m_headFrame + m_frameCount) % m_capacityFrames;
    const size_t firstRunFrames  = std::min(frameCount, m_capacityFrames - tailFrame);
    const size_t firstRunSamples = firstRunFrames * channels;

    std::copy_n(samples.data(), firstRunSamples, m_samples.data() + (tailFrame * channels));

    const size_t remainingFrames = frameCount - firstRunFrames;
    if(remainingFrames > 0) {
        const size_t remainingSamples = remainingFrames * channels;
        std::copy_n(samples.data() + firstRunSamples, remainingSamples, m_samples.data());
    }

    m_frameCount += frameCount;
    m_nextStreamFrame = startFrame + frameCount;

    if(m_frameCount == frameCount) {
        m_startStreamFrame = startFrame;
    }
}
} // namespace Fooyin
