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

#include <core/engine/enginedefs.h>
#include <core/engine/levelframe.h>
#include <core/engine/lockfreeringbuffer.h>

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <span>
#include <thread>

namespace Fooyin {
/*!
 * Background audio-analysis bus.
 *
 * `push()` is safe for the audio thread.
 * Handler callbacks run on the analysis worker thread.
 */
class AudioAnalysisBus
{
public:
    using Clock     = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    explicit AudioAnalysisBus(size_t slotCapacity = 64);
    ~AudioAnalysisBus();

    AudioAnalysisBus(const AudioAnalysisBus&)            = delete;
    AudioAnalysisBus& operator=(const AudioAnalysisBus&) = delete;

    void setSubscriptions(Engine::AnalysisDataTypes subscriptions);
    [[nodiscard]] Engine::AnalysisDataTypes subscriptions() const;
    [[nodiscard]] bool hasSubscription(Engine::AnalysisDataType subscription) const;

    using LevelReadyHandler = std::function<void(const LevelFrame&)>;

    void setLevelReadyHandler(LevelReadyHandler handler);
    void clearLevelReadyHandler()
    {
        setLevelReadyHandler(LevelReadyHandler{});
    }

    template <typename Receiver, typename Method>
        requires std::invocable<Method, Receiver&, const LevelFrame&>
    void setLevelReadyHandler(Receiver* receiver, Method method)
    {
        if(!receiver) {
            clearLevelReadyHandler();
            return;
        }

        setLevelReadyHandler([receiver, method](const LevelFrame& frame) { std::invoke(method, *receiver, frame); });
    }

    void push(std::span<const float> samples, int channelCount, int sampleRate, uint64_t streamTimeMs,
              TimePoint presentationTime);
    //! Drop queued analysis data and request hop-state reset.
    void flush();

private:
    static constexpr int HopFrames         = 1024;
    static constexpr int MaxSamplesPerSlot = HopFrames * LevelFrame::MaxChannels;

    struct AudioAnalysisSlot
    {
        std::array<float, MaxSamplesPerSlot> samples;
        int sampleCount{0};
        int channelCount{0};
        int sampleRate{0};
        uint64_t streamTimeMs{0};
        TimePoint presentationTime;
    };

    void signalWorker();

    void run(const std::stop_token& stopToken);
    void processSlot(const AudioAnalysisSlot& slot);
    void emitCompletedHop();
    void resetHopState();
    void emitLevelFrame(const LevelFrame& frame);

    LockFreeRingBuffer<AudioAnalysisSlot> m_slots;

    std::atomic<bool> m_resetRequested;
    std::atomic<bool> m_wakePosted;

    std::atomic<uint32_t> m_subscriptionMask;
    std::atomic<bool> m_hasLevelHandler;

    std::condition_variable m_wakeCondition;
    std::mutex m_wakeMutex;
    std::jthread m_worker;

    std::mutex m_handlerMutex;
    LevelReadyHandler m_levelReadyHandler;

    std::array<float, MaxSamplesPerSlot> m_hopSamples;
    int m_hopFrames;
    int m_hopChannels;
    int m_hopSampleRate;
    uint64_t m_hopStreamTimeMs;
    TimePoint m_hopPresentationTime;
};
} // namespace Fooyin
