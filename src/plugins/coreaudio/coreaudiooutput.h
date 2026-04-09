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

#include <core/engine/audiooutput.h>
#include <utils/lockfreeringbuffer.h>

#include <AudioToolbox/AudioQueue.h>

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <memory>
#include <mutex>
#include <vector>

namespace Fooyin::CoreAudio {
class CoreAudioOutput : public AudioOutput
{
public:
    CoreAudioOutput();

    bool init(const AudioFormat& format) override;
    void uninit() override;
    void reset() override;
    void start() override;
    void drain() override;

    [[nodiscard]] bool initialised() const override;
    [[nodiscard]] QString device() const override;
    [[nodiscard]] int bufferSize() const override;
    OutputState currentState() override;
    [[nodiscard]] OutputDevices getAllDevices(bool isCurrentOutput) override;

    int write(std::span<const std::byte> data, int frameCount) override;
    void setPaused(bool pause) override;
    void setVolume(double volume) override;
    [[nodiscard]] bool supportsVolumeControl() const override;
    void setDevice(const QString& device) override;
    [[nodiscard]] AudioFormat negotiateFormat(const AudioFormat& requested) const override;

    [[nodiscard]] QString error() const override;
    [[nodiscard]] AudioFormat format() const override;

private:
    struct QueueBufferState
    {
        AudioQueueBufferRef buffer{nullptr};
        size_t queuedBytes{0};
    };

    bool initQueue();
    void resetQueueState();
    void disposeQueue();
    bool primeQueue(bool allowSilence);
    bool refillBuffer(QueueBufferState& state, bool allowSilence);
    void onBufferCompleted(AudioQueueBufferRef buffer);
    void handleQueueError(OSStatus status, const char* action);
    bool applyQueueVolume();

    [[nodiscard]] size_t queuedHardwareBytes() const;

    static void outputCallback(void* userData, AudioQueueRef queue, AudioQueueBufferRef buffer);

    AudioFormat m_format;
    QString m_device;
    QString m_error;
    double m_volume;
    int m_ringBufferFrames;
    int m_queueBufferFrames;

    std::unique_ptr<LockFreeRingBuffer<std::byte>> m_buffer;
    AudioQueueRef m_queue;
    std::vector<QueueBufferState> m_queueBuffers;

    mutable std::mutex m_stateMutex;
    std::condition_variable m_stateCv;
    std::atomic_size_t m_enqueuedBytes;

    bool m_initialised;
    bool m_started;
    bool m_paused;
    bool m_draining;
    bool m_stopping;
};
} // namespace Fooyin::CoreAudio
