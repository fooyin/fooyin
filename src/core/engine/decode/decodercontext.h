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

#include "core/engine/pipeline/audiostream.h"

#include <core/engine/audioformat.h>
#include <core/engine/audioinput.h>
#include <core/track.h>

#include <QFile>

#include <memory>
#include <vector>

namespace Fooyin {
/*!
 * Manages a single decoder that feeds one active stream.
 *
 * A DecoderContext owns decoder/file/source lifetime and the currently active
 * AudioStream write target. During seek/transition overlap the previously
 * active stream may be detached ("orphaned") so it can drain while decode
 * continues into a new stream.
 */
class FYCORE_EXPORT DecoderContext
{
public:
    DecoderContext();
    ~DecoderContext() = default;

    DecoderContext(DecoderContext&&)                 = default;
    DecoderContext& operator=(DecoderContext&&)      = default;
    DecoderContext(const DecoderContext&)            = delete;
    DecoderContext& operator=(const DecoderContext&) = delete;

    [[nodiscard]] bool isValid() const;
    [[nodiscard]] bool isDecoding() const;
    [[nodiscard]] bool isSeekable() const;
    [[nodiscard]] AudioStreamPtr activeStream() const;
    [[nodiscard]] StreamId activeStreamId() const;
    [[nodiscard]] const Track& track() const;
    [[nodiscard]] const AudioFormat& format() const;
    [[nodiscard]] uint64_t currentPosition() const;
    [[nodiscard]] uint64_t startPosition() const;

    //! Initialise decoder/source for @p track (does not attach a stream).
    bool init(std::unique_ptr<AudioDecoder> decoder, const Track& track);

    bool adoptPreparedDecoder(std::unique_ptr<AudioDecoder> decoder, std::unique_ptr<QFile> file, AudioSource source,
                              const Track& track, const AudioFormat& format);

    //! Create a stream compatible with current decoder format.
    AudioStreamPtr createStream(size_t bufferSamples, Engine::FadeCurve fadeCurve = Engine::FadeCurve::Linear);
    //! Set stream that receives decoded samples.
    void setActiveStream(AudioStreamPtr stream);
    //! Detach current stream (for orphaning during crossfade)
    //! Returns the detached stream so caller can manage its lifecycle
    AudioStreamPtr detachStream();

    void start();
    void stop();
    //! Seek decoder position (caller is responsible for stream reset/position sync).
    bool seek(uint64_t positionMs);
    /*!
     * Decode up to @p maxFrames into the active stream ring buffer.
     * @return Number of frames decoded; 0 on end/error/no writable space.
     */
    int decodeChunk(size_t maxFrames = 4096);

    //! Align active stream read position with decoder current position.
    void syncStreamPosition();

    /*!
     * Decode into active stream until @p targetSamples buffered (or stream end).
     * @param maxChunks <=0 means no chunk-budget limit.
     */
    [[nodiscard]] int prefillActiveStream(size_t targetSamples, int maxChunks = 0, size_t maxFramesPerChunk = 4096);
    /*!
     * Decode into active stream until @p targetMs buffered (or stream end).
     * @param maxChunks <=0 means no chunk-budget limit.
     */
    [[nodiscard]] int prefillActiveStreamMs(uint64_t targetMs, int maxChunks = 0, size_t maxFramesPerChunk = 4096);

    [[nodiscard]] bool trackHasChanged() const;
    [[nodiscard]] Track changedTrack() const;
    //! Apply decoder-reported track metadata to context state.
    //! Returns true when context track data changed.
    [[nodiscard]] bool refreshTrackMetadata();

    //! Get current bitrate
    [[nodiscard]] int bitrate() const;

    [[nodiscard]] std::unique_ptr<AudioDecoder> takeDecoder();
    [[nodiscard]] std::unique_ptr<QFile> takeFile();
    [[nodiscard]] AudioSource takeSource();

    //! Release decoder/file/stream and clear all tracked state.
    void reset();

private:
    std::unique_ptr<AudioDecoder> m_decoder;
    std::unique_ptr<QFile> m_file;
    AudioSource m_source;

    AudioStreamPtr m_activeStream;

    Track m_track;
    AudioFormat m_format;

    uint64_t m_currentPos;
    uint64_t m_startPos;
    uint64_t m_endPos;

    bool m_isDecoding;
    std::vector<double> m_decodeScratch;
};
} // namespace Fooyin
