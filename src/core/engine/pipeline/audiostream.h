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

#include "core/engine/lockfreeringbuffer.h"

#include <core/engine/audioformat.h>
#include <core/engine/enginedefs.h>
#include <core/track.h>

#include <cstddef>
#include <cstdint>

#include <atomic>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace Fooyin {
using StreamId                     = uint32_t;
constexpr StreamId InvalidStreamId = 0;

/*!
 * Represents a single audio stream in the pipeline.
 *
 * Thread safety model:
 * - Ring buffer: Lock-free SPSC (decode thread writes, audio thread reads)
 * - Stream commands: produced by pipeline command flow and applied on the
 *   audio thread only.
 * - Atomic members: thread-safe for concurrent read/write
 * - Format is immutable after construction.
 * - Track metadata can be updated by controller code and read concurrently.
 * - Fade state: managed only by audio thread
 */
class FYCORE_EXPORT AudioStream
{
public:
    enum class State : uint8_t
    {
        Pending = 0,
        Playing,
        FadingIn,
        FadingOut,
        Paused,
        Stopped,
        Error
    };

    // Stream control commands interpreted by applyCommand().
    enum class Command : uint8_t
    {
        Play = 0,
        Pause,
        Stop,
        StartFadeIn,  // param = duration in ms
        StartFadeOut, // param = duration in ms
        StopFade,
        ResetForSeek, // Safely reset buffer for seek
    };

    explicit AudioStream(StreamId id, const AudioFormat& format, size_t bufferSamples);

    AudioStream(const AudioStream&)            = delete;
    AudioStream& operator=(const AudioStream&) = delete;
    AudioStream(AudioStream&&)                 = delete;
    AudioStream& operator=(AudioStream&&)      = delete;

    //! Get unique stream id (thread-safe, immutable).
    [[nodiscard]] StreamId id() const;

    // ========================================================================
    // Command interface (audio thread only)
    // ========================================================================

    //! Apply command transition (audio thread only).
    void applyCommand(Command cmd, int param = 0);

    // ========================================================================
    // Ring buffer access (lock-free, SPSC)
    // Decode thread: write only
    // Audio thread: read only
    // ========================================================================

    //! Get reader handle (must be used by a single consumer thread).
    [[nodiscard]] LockFreeRingBuffer<double>::Reader reader();
    //! Get writer handle (must be used by a single producer thread).
    [[nodiscard]] LockFreeRingBuffer<double>::Writer writer();

    // ========================================================================
    // Track and format (immutable after construction)
    // ========================================================================

    Track track() const;
    [[nodiscard]] uint64_t trackRevision() const;

    void setTrack(const Track& track);

    AudioFormat format() const;
    [[nodiscard]] int channelCount() const;
    [[nodiscard]] int sampleRate() const;

    // ========================================================================
    // State queries (thread-safe, read from any thread)
    // ========================================================================

    [[nodiscard]] State state() const;
    [[nodiscard]] uint64_t position() const;
    [[nodiscard]] uint64_t positionMs() const;
    [[nodiscard]] bool endOfInput() const;

    [[nodiscard]] double fadeGain() const;
    [[nodiscard]] bool isFading() const;
    void setFadeCurve(Engine::FadeCurve curve);

    // ========================================================================
    // Decode thread operations (decode thread only)
    // ========================================================================

    void setEndOfInput();
    void resetEndOfInput();

    void setPosition(uint64_t pos);

    // ========================================================================
    // Audio thread operations (audio thread only)
    // ========================================================================

    /*!
     * Read samples from ring buffer (audio thread only).
     * @param output Destination buffer (interleaved F64 samples)
     * @param frames Number of frames to read
     * @return Number of frames actually read
     */
    int read(double* output, int frames);

    struct FadeRamp
    {
        double startGain{1.0};
        double endGain{1.0};
        bool active{false};
    };

    /*!
     * Calculate fade gain ramp for a block at @p outputSampleRate.
     * Returns start/end gain for per-sample interpolation.
     */
    FadeRamp calculateFadeGainRamp(int frames, int outputSampleRate);

    // ========================================================================
    // Buffer status queries (thread-safe)
    // ========================================================================

    //! True when buffered data is below low-water threshold.
    [[nodiscard]] bool isBufferLow() const;
    //! True when decode ended and no buffered samples remain.
    [[nodiscard]] bool isEndOfStream() const;
    //! Number of buffered interleaved samples.
    [[nodiscard]] size_t bufferedSamples() const;
    //! True when no buffered samples are available.
    [[nodiscard]] bool bufferEmpty() const;
    //! Buffered duration estimate in milliseconds.
    [[nodiscard]] uint64_t bufferedDurationMs() const;

private:
    void handleCommand(Command cmd, int param);
    void startFadeInternal(int durationMs, bool fadeIn);
    void resetBufferForSeek();

    LockFreeRingBuffer<double> m_buffer;

    Track m_track;
    std::atomic<uint64_t> m_trackRevision;

    std::atomic<uint64_t> m_position;

    std::atomic<double> m_fadeGain;
    int64_t m_fadeTotalFrames;
    int64_t m_fadeProcessedFrames;
    double m_fadeStartGain;
    double m_fadeEndGain;

    mutable std::mutex m_metadataMutex;

    const StreamId m_id;

    const int m_channelCount;
    const int m_sampleRate;

    int m_fadeDurationMs;

    const AudioFormat m_format;

    std::atomic<State> m_state;
    std::atomic<bool> m_endOfInput;
    std::atomic<Engine::FadeCurve> m_fadeCurve;
};

using AudioStreamPtr = std::shared_ptr<AudioStream>;

/*!
 * Registry for stream id -> shared stream lookup.
 *
 * Access is mutex-protected and intended for control-path lookups.
 */
class FYCORE_EXPORT StreamRegistry
{
public:
    StreamId registerStream(AudioStreamPtr stream);

    AudioStreamPtr getStream(StreamId id) const;

    void unregisterStream(StreamId id);

    void clear();

private:
    mutable std::mutex m_mutex;
    std::unordered_map<StreamId, AudioStreamPtr> m_streams;
};

/*!
 * Factory that allocates unique stream IDs and returns shared stream instances.
 * Thread-safe via atomic id allocator.
 */
class FYCORE_EXPORT StreamFactory
{
public:
    static AudioStreamPtr createStream(const AudioFormat& format, size_t bufferSamples);

private:
    static std::atomic<StreamId> s_nextId;
};
} // namespace Fooyin
