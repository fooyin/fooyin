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

#include "decodercontext.h"

#include <core/engine/audiobuffer.h>
#include <core/engine/audioconverter.h>

#include <QLoggingCategory>

#include <algorithm>
#include <cstring>
#include <limits>
#include <optional>
#include <utility>

Q_DECLARE_LOGGING_CATEGORY(ENGINE)

constexpr auto MaxDiscardedPreRollReads = 8;

namespace {
[[nodiscard]] bool reachedTrackEnd(uint64_t positionMs, uint64_t endPosMs)
{
    return endPosMs != std::numeric_limits<uint64_t>::max() && positionMs >= endPosMs;
}

[[nodiscard]] std::optional<Fooyin::AudioBuffer> trimBufferToTrackWindow(const Fooyin::AudioBuffer& buffer,
                                                                         uint64_t windowStartMs, uint64_t windowEndMs)
{
    if(!buffer.isValid()) {
        return {};
    }

    const Fooyin::AudioFormat format = buffer.format();
    if(!format.isValid()) {
        return {};
    }

    const uint64_t chunkStartMs = buffer.startTime();
    const uint64_t chunkEndMs   = buffer.endTime();
    const uint64_t clampedStart = std::max(chunkStartMs, windowStartMs);
    const uint64_t clampedEnd   = std::min(chunkEndMs, windowEndMs);

    if(clampedStart >= clampedEnd) {
        return {};
    }

    const int totalFrames = buffer.frameCount();
    if(totalFrames <= 0) {
        return {};
    }

    int startFrame = 0;
    if(clampedStart > chunkStartMs) {
        startFrame = std::clamp(format.framesForDuration(clampedStart - chunkStartMs), 0, totalFrames);
    }

    int endFrame = totalFrames;
    if(clampedEnd < chunkEndMs) {
        endFrame = std::clamp(format.framesForDuration(clampedEnd - chunkStartMs), startFrame, totalFrames);
    }

    if(endFrame <= startFrame) {
        return {};
    }

    const int bytesPerFrame = format.bytesPerFrame();
    if(bytesPerFrame <= 0) {
        return {};
    }

    const size_t startByte = static_cast<size_t>(startFrame) * static_cast<size_t>(bytesPerFrame);
    const size_t byteCount = static_cast<size_t>(endFrame - startFrame) * static_cast<size_t>(bytesPerFrame);
    const auto inputBytes  = buffer.constData();
    if(startByte + byteCount > inputBytes.size()) {
        return {};
    }

    const uint64_t trimmedStartMs = chunkStartMs + format.durationForFrames(startFrame);

    return Fooyin::AudioBuffer{inputBytes.subspan(startByte, byteCount), format, trimmedStartMs};
}
} // namespace

namespace Fooyin {
DecoderContext::DecoderContext()
    : m_currentPos{0}
    , m_startPos{0}
    , m_endPolicy{EndPolicy::DecoderEofOnly}
    , m_playbackHints{AudioDecoder::NoHints}
    , m_isDecoding{false}
{ }

bool DecoderContext::isValid() const
{
    return m_decoder != nullptr;
}

bool DecoderContext::isDecoding() const
{
    return m_isDecoding;
}

bool DecoderContext::isSeekable() const
{
    return m_decoder && m_decoder->isSeekable();
}

AudioStreamPtr DecoderContext::activeStream() const
{
    return m_activeStream;
}

StreamId DecoderContext::activeStreamId() const
{
    return m_activeStream ? m_activeStream->id() : InvalidStreamId;
}

const Track& DecoderContext::track() const
{
    return m_track;
}

const AudioFormat& DecoderContext::format() const
{
    return m_format;
}

uint64_t DecoderContext::currentPosition() const
{
    return m_currentPos;
}

uint64_t DecoderContext::startPosition() const
{
    return m_startPos;
}

AudioDecoder::PlaybackHints DecoderContext::playbackHints() const
{
    return m_playbackHints;
}

void DecoderContext::setPlaybackHints(AudioDecoder::PlaybackHints hints)
{
    m_playbackHints = hints;

    if(m_decoder) {
        m_decoder->setPlaybackHints(m_playbackHints);
    }
}

bool DecoderContext::init(LoadedDecoder decoder, const Track& track)
{
    if(!decoder.decoder || !decoder.format) {
        return false;
    }

    m_track   = track;
    m_decoder = std::move(decoder.decoder);
    m_input   = std::move(decoder.input);
    m_input.rebind();

    m_format = *decoder.format;

    // Default to decoder-reported EOF for end detection.
    m_startPos = track.offset();
    setEndPolicy(EndPolicy::DecoderEofOnly);
    m_currentPos = m_startPos;

    return true;
}

bool DecoderContext::adoptPreparedDecoder(LoadedDecoder decoder, const Track& track)
{
    if(!decoder.decoder || !decoder.format) {
        return false;
    }

    m_decoder = std::move(decoder.decoder);
    m_decoder->setPlaybackHints(m_playbackHints);
    m_input = std::move(decoder.input);
    m_input.rebind();

    m_track    = track;
    m_format   = *decoder.format;
    m_startPos = track.offset();
    setEndPolicy(EndPolicy::DecoderEofOnly);
    m_currentPos = m_startPos;

    m_isDecoding = false;

    return true;
}

AudioStreamPtr DecoderContext::createStream(size_t bufferSamples, Engine::FadeCurve fadeCurve)
{
    auto stream = StreamFactory::createStream(m_format, bufferSamples);
    stream->setTrack(m_track);
    stream->setFadeCurve(fadeCurve);
    return stream;
}

void DecoderContext::setActiveStream(AudioStreamPtr stream)
{
    m_activeStream = std::move(stream);
}

AudioStreamPtr DecoderContext::detachStream()
{
    return std::exchange(m_activeStream, nullptr);
}

void DecoderContext::start()
{
    if(!m_decoder || m_isDecoding) {
        return;
    }

    m_decoder->start();
    m_isDecoding = true;
}

void DecoderContext::stop()
{
    if(m_decoder) {
        m_decoder->stop();
    }

    m_isDecoding = false;
}

bool DecoderContext::seek(uint64_t positionMs)
{
    if(!m_decoder || !m_decoder->isSeekable()) {
        return false;
    }

    m_decoder->seek(positionMs);
    m_currentPos = positionMs;

    return true;
}

bool DecoderContext::switchContiguousTrack(const Track& track)
{
    if(!m_decoder || !track.isValid()) {
        return false;
    }

    m_track    = track;
    m_startPos = track.offset();
    setEndPolicy(EndPolicy::DecoderEofOnly);

    // Keep decoder continuity across contiguous logical segments.
    m_currentPos = std::max(m_currentPos, m_startPos);
    m_isDecoding = true;

    if(m_activeStream) {
        m_activeStream->setTrack(track);
        m_activeStream->resetEndOfInput();
    }

    return true;
}

void DecoderContext::setEndPolicy(EndPolicy policy, std::optional<uint64_t> windowEndMs)
{
    m_endPolicy    = policy;
    m_windowEndPos = windowEndMs;
}

DecoderContext::EndPolicy DecoderContext::endPolicy() const
{
    return m_endPolicy;
}

int DecoderContext::decodeChunk(size_t maxFrames)
{
    if(!m_isDecoding || !m_decoder || !m_activeStream) {
        return 0;
    }

    auto writer = m_activeStream->writer();
    if(writer.capacity() == 0) {
        qCWarning(ENGINE) << "decodeChunk: buffer has zero capacity";
        return 0;
    }

    if(m_activeStream->endOfInput()) {
        return 0;
    }

    const bool windowBounded = m_endPolicy == EndPolicy::WindowOrDecoderEof && m_windowEndPos.has_value();
    const uint64_t windowEnd = windowBounded ? *m_windowEndPos : std::numeric_limits<uint64_t>::max();

    if(windowBounded && reachedTrackEnd(m_currentPos, windowEnd)) {
        m_activeStream->setEndOfInput();
        m_isDecoding = false;
        return 0;
    }

    const size_t availableSpace = writer.writeAvailable();
    if(availableSpace == 0) {
        // Buffer full, try again later
        return 0;
    }

    const auto channels            = static_cast<size_t>(m_format.channelCount());
    const size_t maxFramesToDecode = std::min(maxFrames, availableSpace / channels);

    if(maxFramesToDecode == 0) {
        return 0;
    }

    const auto bytesPerFrame = static_cast<size_t>(m_format.bytesPerFrame());
    const size_t bytesToRead = maxFramesToDecode * bytesPerFrame;

    for(int readAttempt{0}; readAttempt < MaxDiscardedPreRollReads; ++readAttempt) {
        const auto audioBuffer = m_decoder->readBuffer(bytesToRead);
        if(!audioBuffer.isValid()) {
            m_activeStream->setEndOfInput();
            m_isDecoding = false;
            return 0;
        }

        const uint64_t chunkEndMs  = audioBuffer.endTime();
        const bool chunkReachedEnd = windowBounded && reachedTrackEnd(chunkEndMs, windowEnd);
        const auto boundedInput    = trimBufferToTrackWindow(audioBuffer, m_startPos, windowEnd);

        m_currentPos = windowBounded ? std::min(chunkEndMs, windowEnd) : chunkEndMs;

        if(!boundedInput.has_value()) {
            if(chunkReachedEnd) {
                m_activeStream->setEndOfInput();
                m_isDecoding = false;
                return 0;
            }
            continue;
        }

        auto format = boundedInput->format();
        format.setSampleFormat(SampleFormat::F64);

        auto convertedBuffer = Audio::convert(boundedInput.value(), format);
        if(!convertedBuffer.isValid()) {
            return 0;
        }

        const auto sampleCount = static_cast<size_t>(convertedBuffer.frameCount()) * channels;
        const auto inputBytes  = convertedBuffer.constData();
        const size_t byteCount = sampleCount * sizeof(double);
        if(inputBytes.size() < byteCount) {
            return 0;
        }

        if(m_decodeScratch.size() < sampleCount) {
            m_decodeScratch.resize(sampleCount);
        }
        std::memcpy(m_decodeScratch.data(), inputBytes.data(), byteCount);

        const size_t samplesWritten = writer.write(m_decodeScratch.data(), sampleCount);
        if(samplesWritten < sampleCount) {
            qCWarning(ENGINE) << "Buffer overflow: tried to write" << sampleCount << "samples but only wrote"
                              << samplesWritten;
        }

        m_currentPos = windowBounded ? std::min(boundedInput->endTime(), windowEnd) : boundedInput->endTime();

        if(windowBounded && (chunkReachedEnd || reachedTrackEnd(m_currentPos, windowEnd))) {
            m_activeStream->setEndOfInput();
            m_isDecoding = false;
        }

        return convertedBuffer.frameCount();
    }

    return 0;
}

void DecoderContext::syncStreamPosition()
{
    if(!m_activeStream) {
        return;
    }

    const auto relativeMs = m_currentPos - m_startPos;
    const auto samples    = static_cast<uint64_t>(m_format.framesForDuration(relativeMs)) * m_format.channelCount();

    m_activeStream->setPosition(samples);
    m_activeStream->resetEndOfInput();
}

int DecoderContext::prefillActiveStream(size_t targetSamples, int maxChunks, size_t maxFramesPerChunk)
{
    if(!m_activeStream || targetSamples == 0 || maxFramesPerChunk == 0) {
        return 0;
    }

    static constexpr auto MaxNoProgressIterations = 8;

    int chunksDecoded{0};
    int chunksRemaining{maxChunks};
    int noProgressIter{0};

    while(m_activeStream->bufferedSamples() < targetSamples && !m_activeStream->endOfInput()) {
        if(maxChunks > 0 && chunksRemaining <= 0) {
            break;
        }

        const int decodedFrames = decodeChunk(maxFramesPerChunk);
        if(decodedFrames <= 0) {
            if(m_activeStream->endOfInput()) {
                break;
            }

            if(++noProgressIter >= MaxNoProgressIterations) {
                break;
            }

            continue;
        }

        noProgressIter = 0;

        if(maxChunks > 0) {
            --chunksRemaining;
        }

        ++chunksDecoded;
    }

    return chunksDecoded;
}

int DecoderContext::prefillActiveStreamMs(uint64_t targetMs, int maxChunks, size_t maxFramesPerChunk)
{
    if(!m_activeStream || targetMs <= 0 || !m_format.isValid()) {
        return {};
    }

    const int channels   = m_format.channelCount();
    const int sampleRate = m_format.sampleRate();
    if(channels <= 0 || sampleRate <= 0) {
        return {};
    }

    const auto targetSamples = targetMs * sampleRate * channels / 1000;
    return prefillActiveStream(targetSamples, maxChunks, maxFramesPerChunk);
}

bool DecoderContext::trackHasChanged() const
{
    return m_decoder && m_decoder->trackHasChanged();
}

Track DecoderContext::changedTrack() const
{
    return m_decoder ? m_decoder->changedTrack() : Track{};
}

bool DecoderContext::refreshTrackMetadata()
{
    if(!m_decoder || !m_decoder->trackHasChanged()) {
        return false;
    }

    const Track changed = m_decoder->changedTrack();
    if(!changed.isValid() || changed == m_track) {
        return false;
    }

    m_track    = changed;
    m_startPos = changed.offset();
    setEndPolicy(EndPolicy::DecoderEofOnly);

    m_currentPos = std::max(m_currentPos, m_startPos);

    if(m_activeStream) {
        m_activeStream->setTrack(m_track);
    }

    return true;
}

int DecoderContext::bitrate() const
{
    return m_decoder ? m_decoder->bitrate() : 0;
}

LoadedDecoder DecoderContext::takeLoadedDecoder()
{
    LoadedDecoder loaded;
    loaded.decoder = std::exchange(m_decoder, nullptr);
    if(loaded.decoder) {
        loaded.format = m_format;
    }
    loaded.input = std::exchange(m_input, {});
    loaded.input.rebind();
    return loaded;
}

void DecoderContext::reset()
{
    stop();

    m_decoder.reset();
    m_input = {};
    m_activeStream.reset();
    m_track      = {};
    m_format     = {};
    m_currentPos = 0;
    m_startPos   = 0;
    m_windowEndPos.reset();
    m_endPolicy = EndPolicy::DecoderEofOnly;
    std::vector<double>{}.swap(m_decodeScratch);
}
} // namespace Fooyin
