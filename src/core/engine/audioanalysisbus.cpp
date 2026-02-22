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

#include "audioanalysisbus.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr uint32_t analysisMask(const Fooyin::Engine::AnalysisDataTypes subscriptions)
{
    return static_cast<uint32_t>(subscriptions.toInt());
}

constexpr uint32_t analysisMask(const Fooyin::Engine::AnalysisDataType subscription)
{
    return static_cast<uint32_t>(subscription);
}
} // namespace

namespace Fooyin {
AudioAnalysisBus::AudioAnalysisBus(const size_t slotCapacity)
    : m_slots{std::max(static_cast<size_t>(1), slotCapacity)}
    , m_resetRequested{false}
    , m_wakePosted{false}
    , m_subscriptionMask{analysisMask(Engine::AnalysisDataTypes{})}
    , m_hasLevelHandler{false}
    , m_hopSamples{}
    , m_hopFrames{0}
    , m_hopChannels{0}
    , m_hopSampleRate{0}
    , m_hopStreamTimeMs{0}
{
    m_worker = std::jthread{[this](std::stop_token stopToken) { run(stopToken); }};
}

AudioAnalysisBus::~AudioAnalysisBus()
{
    m_worker.request_stop();
    m_wakeCondition.notify_all();
}

void AudioAnalysisBus::setSubscriptions(const Engine::AnalysisDataTypes subscriptions)
{
    const uint32_t newMask = analysisMask(subscriptions);
    const uint32_t oldMask = m_subscriptionMask.exchange(newMask, std::memory_order_acq_rel);

    if(oldMask == newMask) {
        return;
    }

    if((newMask & analysisMask(Engine::AnalysisDataType::LevelFrameData)) == 0U) {
        flush();
    }

    signalWorker();
}

Engine::AnalysisDataTypes AudioAnalysisBus::subscriptions() const
{
    return Engine::AnalysisDataTypes::fromInt(static_cast<int>(m_subscriptionMask.load(std::memory_order_acquire)));
}

bool AudioAnalysisBus::hasSubscription(const Engine::AnalysisDataType subscription) const
{
    const uint32_t mask = m_subscriptionMask.load(std::memory_order_acquire);
    return (mask & analysisMask(subscription)) != 0U;
}

void AudioAnalysisBus::setLevelReadyHandler(LevelReadyHandler handler)
{
    const bool hasHandler = static_cast<bool>(handler);

    {
        const std::scoped_lock lock{m_handlerMutex};
        m_levelReadyHandler = std::move(handler);
    }

    m_hasLevelHandler.store(hasHandler, std::memory_order_release);

    if(!hasHandler) {
        flush();
    }

    signalWorker();
}

void AudioAnalysisBus::push(const std::span<const float> samples, const int channelCount, const int sampleRate,
                            const uint64_t streamTimeMs, const TimePoint presentationTime)
{
    if(!hasSubscription(Engine::AnalysisDataType::LevelFrameData)
       || !m_hasLevelHandler.load(std::memory_order_acquire)) {
        return;
    }

    if(samples.empty() || channelCount <= 0 || channelCount > LevelFrame::MaxChannels || sampleRate <= 0) {
        return;
    }

    const auto channels             = static_cast<size_t>(channelCount);
    const size_t alignedSampleCount = (samples.size() / channels) * channels;
    if(alignedSampleCount == 0) {
        return;
    }

    size_t sampleOffset{0};
    bool wakeSignalled{false};

    auto writer = m_slots.writer();

    while(sampleOffset < alignedSampleCount) {
        const size_t remainingSamples = alignedSampleCount - sampleOffset;
        const size_t requestedSamples = std::min(remainingSamples, static_cast<size_t>(MaxSamplesPerSlot));
        const size_t slotSampleCount  = (requestedSamples / channels) * channels;

        if(slotSampleCount == 0) {
            break;
        }

        const uint64_t sampleOffsetFrames = sampleOffset / channels;
        const uint64_t offsetMs           = (sampleOffsetFrames * 1000ULL) / static_cast<uint64_t>(sampleRate);

        const auto reservation = writer.reserveOne();
        if(!reservation) {
            break;
        }

        AudioAnalysisSlot* slot = reservation.data;
        slot->sampleCount       = static_cast<int>(slotSampleCount);
        slot->channelCount      = channelCount;
        slot->sampleRate        = sampleRate;
        slot->streamTimeMs      = streamTimeMs + offsetMs;
        slot->presentationTime  = presentationTime + std::chrono::milliseconds{offsetMs};
        std::copy_n(samples.data() + static_cast<std::ptrdiff_t>(sampleOffset), slotSampleCount, slot->samples.data());

        writer.commitOne(reservation);

        if(!wakeSignalled) {
            signalWorker();
            wakeSignalled = true;
        }

        sampleOffset += slotSampleCount;
    }
}

void AudioAnalysisBus::flush()
{
    m_slots.requestReset();
    m_resetRequested.store(true, std::memory_order_release);
    signalWorker();
}

void AudioAnalysisBus::signalWorker()
{
    if(!m_wakePosted.exchange(true, std::memory_order_relaxed)) {
        m_wakeCondition.notify_one();
    }
}

void AudioAnalysisBus::run(const std::stop_token& stopToken)
{
    AudioAnalysisSlot slot;

    const auto canProcessLevelFrames = [this]() {
        return hasSubscription(Engine::AnalysisDataType::LevelFrameData)
            && m_hasLevelHandler.load(std::memory_order_acquire);
    };

    const auto hasPendingFrames = [this]() {
        return m_slots.readAvailable() != 0;
    };

    auto reader = m_slots.reader();

    while(!stopToken.stop_requested()) {
        if(m_resetRequested.exchange(false, std::memory_order_acq_rel)) {
            resetHopState();
        }

        if(!canProcessLevelFrames()) {
            std::unique_lock lock{m_wakeMutex};

            if(stopToken.stop_requested() || hasPendingFrames() || m_resetRequested.load(std::memory_order_acquire)
               || canProcessLevelFrames()) {
                continue;
            }

            m_wakePosted.store(false, std::memory_order_relaxed);
            m_wakeCondition.wait(lock, [this, &stopToken, &canProcessLevelFrames, &hasPendingFrames]() {
                return stopToken.stop_requested() || hasPendingFrames()
                    || m_resetRequested.load(std::memory_order_acquire) || canProcessLevelFrames();
            });

            continue;
        }

        bool drainedFrames{false};

        while(reader.read(&slot, 1) == 1) {
            drainedFrames = true;
            processSlot(slot);
        }

        if(drainedFrames) {
            continue;
        }

        std::unique_lock lock{m_wakeMutex};

        if(stopToken.stop_requested() || hasPendingFrames() || m_resetRequested.load(std::memory_order_acquire)
           || !canProcessLevelFrames()) {
            continue;
        }

        m_wakePosted.store(false, std::memory_order_relaxed);
        m_wakeCondition.wait(lock, [this, &stopToken, &canProcessLevelFrames, &hasPendingFrames]() {
            return stopToken.stop_requested() || hasPendingFrames() || m_resetRequested.load(std::memory_order_acquire)
                || !canProcessLevelFrames();
        });
    }
}

void AudioAnalysisBus::processSlot(const AudioAnalysisSlot& slot)
{
    if(slot.sampleCount <= 0 || slot.channelCount <= 0 || slot.channelCount > LevelFrame::MaxChannels
       || slot.sampleRate <= 0) {
        return;
    }

    const int slotFrames = slot.sampleCount / slot.channelCount;
    if(slotFrames <= 0) {
        return;
    }

    if(m_hopFrames > 0 && (m_hopChannels != slot.channelCount || m_hopSampleRate != slot.sampleRate)) {
        resetHopState();
    }

    int consumedFrames{0};
    while(consumedFrames < slotFrames) {
        if(m_hopFrames == 0) {
            const uint64_t offsetMs
                = (static_cast<uint64_t>(consumedFrames) * 1000ULL) / static_cast<uint64_t>(slot.sampleRate);
            m_hopChannels         = slot.channelCount;
            m_hopSampleRate       = slot.sampleRate;
            m_hopStreamTimeMs     = slot.streamTimeMs + offsetMs;
            m_hopPresentationTime = slot.presentationTime + std::chrono::milliseconds{offsetMs};
        }

        const int availableFrames = slotFrames - consumedFrames;
        const int copyFrames      = std::min(HopFrames - m_hopFrames, availableFrames);
        const auto channels       = static_cast<size_t>(slot.channelCount);

        const auto sourceOffsetSamples = static_cast<size_t>(consumedFrames) * channels;
        const auto destOffsetSamples   = static_cast<size_t>(m_hopFrames) * channels;
        const auto sampleCount         = static_cast<size_t>(copyFrames) * channels;

        std::copy_n(slot.samples.data() + static_cast<std::ptrdiff_t>(sourceOffsetSamples), sampleCount,
                    m_hopSamples.data() + static_cast<std::ptrdiff_t>(destOffsetSamples));

        consumedFrames += copyFrames;
        m_hopFrames += copyFrames;

        if(m_hopFrames == HopFrames) {
            emitCompletedHop();
            resetHopState();
        }
    }
}

void AudioAnalysisBus::emitCompletedHop()
{
    if(m_hopFrames != HopFrames || m_hopChannels <= 0 || m_hopChannels > LevelFrame::MaxChannels) {
        return;
    }

    LevelFrame frame;
    frame.channelCount     = m_hopChannels;
    frame.streamTimeMs     = m_hopStreamTimeMs;
    frame.presentationTime = m_hopPresentationTime;

    const auto channels = static_cast<size_t>(m_hopChannels);
    for(size_t channel{0}; channel < channels; ++channel) {
        float peak{0.0F};
        double rms{0.0};

        for(int frameIndex{0}; frameIndex < HopFrames; ++frameIndex) {
            const size_t sampleIndex = (static_cast<size_t>(frameIndex) * channels) + channel;
            const float sample       = m_hopSamples[sampleIndex];
            const float absolute     = std::abs(sample);

            peak = std::max(peak, absolute);
            rms += static_cast<double>(sample) * static_cast<double>(sample);
        }

        frame.peak[channel] = peak;
        frame.rms[channel]  = static_cast<float>(std::sqrt(rms / static_cast<double>(HopFrames)));
    }

    emitLevelFrame(frame);
}

void AudioAnalysisBus::resetHopState()
{
    m_hopFrames           = 0;
    m_hopChannels         = 0;
    m_hopSampleRate       = 0;
    m_hopStreamTimeMs     = 0;
    m_hopPresentationTime = {};
}

void AudioAnalysisBus::emitLevelFrame(const LevelFrame& frame)
{
    LevelReadyHandler handler;
    {
        const std::scoped_lock lock{m_handlerMutex};
        handler = m_levelReadyHandler;
    }

    if(handler) {
        handler(frame);
    }
}
} // namespace Fooyin
