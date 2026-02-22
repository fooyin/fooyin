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

#include "audiostream.h"

#include <algorithm>
#include <cmath>

namespace Fooyin {
AudioStream::AudioStream(StreamId id, const AudioFormat& format, size_t bufferSamples)
    : m_buffer{bufferSamples}
    , m_trackRevision{0}
    , m_position{0}
    , m_fadeGain{1.0}
    , m_fadeTotalFrames{0}
    , m_fadeProcessedFrames{0}
    , m_fadeStartGain{1.0}
    , m_fadeEndGain{1.0}
    , m_id{id}
    , m_channelCount{format.channelCount()}
    , m_sampleRate{format.sampleRate()}
    , m_fadeDurationMs{0}
    , m_format{format}
    , m_state{AudioStream::State::Pending}
    , m_endOfInput{false}
    , m_fadeCurve{Engine::FadeCurve::Cosine}
{ }

StreamId AudioStream::id() const
{
    return m_id;
}

void AudioStream::applyCommand(Command cmd, int param)
{
    handleCommand(cmd, param);
}

LockFreeRingBuffer<double>::Reader AudioStream::reader()
{
    return m_buffer.reader();
}

LockFreeRingBuffer<double>::Writer AudioStream::writer()
{
    return m_buffer.writer();
}

Track AudioStream::track() const
{
    const std::scoped_lock lock{m_metadataMutex};
    return m_track;
}

uint64_t AudioStream::trackRevision() const
{
    return m_trackRevision.load(std::memory_order_relaxed);
}

void AudioStream::setTrack(const Track& track)
{
    const std::scoped_lock lock{m_metadataMutex};

    m_track = track;
    m_trackRevision.fetch_add(1, std::memory_order_relaxed);
}

AudioFormat AudioStream::format() const
{
    return m_format;
}

int AudioStream::channelCount() const
{
    return m_channelCount;
}

int AudioStream::sampleRate() const
{
    return m_sampleRate;
}

AudioStream::State AudioStream::state() const
{
    return m_state.load(std::memory_order_relaxed);
}

uint64_t AudioStream::position() const
{
    return m_position.load(std::memory_order_relaxed);
}

uint64_t AudioStream::positionMs() const
{
    const int channels = channelCount();
    const int rate     = sampleRate();

    if(channels <= 0 || rate <= 0) {
        return 0;
    }

    const uint64_t frames = position() / static_cast<uint64_t>(channels);
    return (frames * 1000) / static_cast<uint64_t>(rate);
}

bool AudioStream::endOfInput() const
{
    return m_endOfInput.load(std::memory_order_relaxed);
}

double AudioStream::fadeGain() const
{
    return m_fadeGain.load(std::memory_order_acquire);
}

bool AudioStream::isFading() const
{
    const auto s = state();
    return s == AudioStream::State::FadingIn || s == AudioStream::State::FadingOut;
}

void AudioStream::setFadeCurve(Engine::FadeCurve curve)
{
    m_fadeCurve.store(curve, std::memory_order_release);
}

void AudioStream::setEndOfInput()
{
    m_endOfInput.store(true, std::memory_order_relaxed);
}

void AudioStream::resetEndOfInput()
{
    m_endOfInput.store(false, std::memory_order_relaxed);
}

void AudioStream::setPosition(uint64_t pos)
{
    m_position.store(pos, std::memory_order_relaxed);
}

int AudioStream::read(double* output, int frames)
{
    const int channels = channelCount();
    if(channels <= 0) {
        return 0;
    }

    const auto samples       = static_cast<size_t>(frames) * channels;
    auto reader              = m_buffer.reader();
    const size_t readSamples = reader.read(output, samples);
    const int readFrames     = static_cast<int>(readSamples) / channels;

    if(readFrames > 0) {
        m_position.fetch_add(readSamples, std::memory_order_relaxed);
    }

    return readFrames;
}

AudioStream::FadeRamp AudioStream::calculateFadeGainRamp(int frames, int outputSampleRate)
{
    const auto currentState = state();
    if(currentState != AudioStream::State::FadingIn && currentState != AudioStream::State::FadingOut) {
        const double gain = m_fadeGain.load(std::memory_order_acquire);
        return {.startGain = gain, .endGain = gain, .active = false};
    }

    if(frames <= 0) {
        frames = 1;
    }

    if(m_fadeTotalFrames <= 0) {
        const int rate = std::max(1, (outputSampleRate > 0) ? outputSampleRate : sampleRate());
        m_fadeTotalFrames
            = std::max<int64_t>(1, (static_cast<int64_t>(m_fadeDurationMs) * static_cast<int64_t>(rate)) / 1000);
    }

    const int64_t startFrame = std::clamp<int64_t>(m_fadeProcessedFrames, 0, m_fadeTotalFrames);
    const int64_t endFrame   = std::clamp<int64_t>(startFrame + static_cast<int64_t>(frames), 0, m_fadeTotalFrames);
    const double progressStart
        = std::clamp(static_cast<double>(startFrame) / static_cast<double>(m_fadeTotalFrames), 0.0, 1.0);
    const double progressEnd
        = std::clamp(static_cast<double>(endFrame) / static_cast<double>(m_fadeTotalFrames), 0.0, 1.0);

    const Engine::FadeCurve curve = m_fadeCurve.load(std::memory_order_acquire);
    const Engine::FadeDirection direction
        = currentState == AudioStream::State::FadingOut ? Engine::FadeDirection::Out : Engine::FadeDirection::In;
    const double gainStart01 = Engine::gain01(progressStart, curve, direction);
    const double gainEnd01   = Engine::gain01(progressEnd, curve, direction);

    const double gainStart = direction == Engine::FadeDirection::Out
                               ? (m_fadeEndGain + ((m_fadeStartGain - m_fadeEndGain) * gainStart01))
                               : (m_fadeStartGain + ((m_fadeEndGain - m_fadeStartGain) * gainStart01));
    const double gainEnd   = direction == Engine::FadeDirection::Out
                               ? (m_fadeEndGain + ((m_fadeStartGain - m_fadeEndGain) * gainEnd01))
                               : (m_fadeStartGain + ((m_fadeEndGain - m_fadeStartGain) * gainEnd01));

    m_fadeProcessedFrames = endFrame;
    m_fadeGain.store(gainEnd, std::memory_order_release);

    // Check if fade complete
    if(endFrame >= m_fadeTotalFrames) {
        m_fadeGain.store(m_fadeEndGain, std::memory_order_release);

        if(currentState == AudioStream::State::FadingOut) {
            m_state.store(AudioStream::State::Stopped, std::memory_order_relaxed);
        }
        else {
            m_state.store(AudioStream::State::Playing, std::memory_order_relaxed);
        }

        m_fadeTotalFrames     = 0;
        m_fadeProcessedFrames = 0;
    }

    return {.startGain = gainStart, .endGain = gainEnd, .active = true};
}

bool AudioStream::isBufferLow() const
{
    // Consider low if less than 25% full
    return m_buffer.readAvailable() < m_buffer.capacity() / 4;
}

bool AudioStream::isEndOfStream() const
{
    return m_endOfInput.load(std::memory_order_relaxed) && m_buffer.empty();
}

size_t AudioStream::bufferedSamples() const
{
    return m_buffer.readAvailable();
}

bool AudioStream::bufferEmpty() const
{
    return m_buffer.empty();
}

uint64_t AudioStream::bufferedDurationMs() const
{
    const int channels = channelCount();
    const int rate     = sampleRate();
    if(channels <= 0 || rate <= 0) {
        return 0;
    }

    const size_t samples = m_buffer.readAvailable();
    const size_t frames  = samples / channels;
    return (frames * 1000) / rate;
}

void AudioStream::handleCommand(Command cmd, int param)
{
    switch(cmd) {
        case(Command::Play):
            m_state.store(AudioStream::State::Playing, std::memory_order_relaxed);
            break;
        case(Command::Pause):
            m_state.store(AudioStream::State::Paused, std::memory_order_relaxed);
            break;
        case(Command::Stop):
            m_state.store(AudioStream::State::Stopped, std::memory_order_relaxed);
            break;
        case(Command::StartFadeIn):
            startFadeInternal(param, true);
            break;
        case(Command::StartFadeOut):
            startFadeInternal(param, false);
            break;
        case(Command::StopFade): {
            m_fadeTotalFrames     = 0;
            m_fadeProcessedFrames = 0;
            const auto fadeState  = m_state.load(std::memory_order_acquire);

            if(fadeState == AudioStream::State::FadingIn || fadeState == AudioStream::State::FadingOut) {
                m_state.store(AudioStream::State::Playing, std::memory_order_relaxed);
            }
            break;
        }
        case(Command::ResetForSeek):
            // Safe buffer reset - called from audio thread
            resetBufferForSeek();
            break;
    }
}

void AudioStream::startFadeInternal(int durationMs, bool fadeIn)
{
    const auto currentState = m_state.load(std::memory_order_acquire);
    double startGain        = std::clamp(m_fadeGain.load(std::memory_order_acquire), 0.0, 1.0);
    const double targetGain = fadeIn ? 1.0 : 0.0;

    if(fadeIn && (currentState == AudioStream::State::Pending || currentState == AudioStream::State::Stopped)) {
        startGain = 0.0;
    }

    m_fadeStartGain = startGain;
    m_fadeEndGain   = targetGain;

    if(durationMs <= 0 || std::abs(targetGain - startGain) < 0.000001) {
        m_fadeDurationMs      = 0;
        m_fadeTotalFrames     = 0;
        m_fadeProcessedFrames = 0;
        m_fadeGain.store(targetGain, std::memory_order_release);
        m_state.store(fadeIn ? AudioStream::State::Playing : AudioStream::State::Stopped, std::memory_order_release);
        return;
    }

    m_fadeDurationMs      = durationMs;
    m_fadeTotalFrames     = 0;
    m_fadeProcessedFrames = 0;
    m_fadeGain.store(startGain, std::memory_order_release);
    m_state.store(fadeIn ? AudioStream::State::FadingIn : AudioStream::State::FadingOut, std::memory_order_release);
}

void AudioStream::resetBufferForSeek()
{
    // Called from the audio thread via command queue,
    // so it's safe to clear the buffer
    auto reader = m_buffer.reader();
    reader.requestReset();

    m_endOfInput.store(false, std::memory_order_relaxed);
}

StreamId StreamRegistry::registerStream(AudioStreamPtr stream)
{
    if(!stream) {
        return InvalidStreamId;
    }

    const std::scoped_lock lock{m_mutex};

    const StreamId id = stream->id();
    m_streams[id]     = std::move(stream);

    return id;
}

AudioStreamPtr StreamRegistry::getStream(StreamId id) const
{
    const std::scoped_lock lock{m_mutex};
    auto it = m_streams.find(id);
    return it != m_streams.end() ? it->second : nullptr;
}

void StreamRegistry::unregisterStream(StreamId id)
{
    const std::scoped_lock lock{m_mutex};
    m_streams.erase(id);
}

void StreamRegistry::clear()
{
    const std::scoped_lock lock{m_mutex};
    m_streams.clear();
}

AudioStreamPtr StreamFactory::createStream(const AudioFormat& format, size_t bufferSamples)
{
    StreamId id{InvalidStreamId};

    while(id == InvalidStreamId) {
        id = s_nextId.fetch_add(1, std::memory_order_relaxed) + 1;
    }

    return std::make_shared<AudioStream>(id, format, bufferSamples);
}

std::atomic<StreamId> StreamFactory::s_nextId{0};
} // namespace Fooyin
