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

#include <core/engine/audioinput.h>

#include <FLAC/stream_decoder.h>

#include <vector>

namespace Fooyin::Flac {
class FlacDecoder : public AudioDecoder
{
public:
    FlacDecoder();

    [[nodiscard]] QStringList extensions() const override;
    [[nodiscard]] bool isSeekable() const override;

    std::optional<AudioFormat> init(const AudioSource& source, const Track& track, DecoderOptions options) override;
    void stop() override;

    void seek(uint64_t pos) override;

    ReadResult readAudio(size_t bytes) override;
    AudioBuffer readBuffer(size_t bytes) override;

    [[nodiscard]] QStringList takeWarnings() override;

private:
    static FLAC__StreamDecoderReadStatus readCallback(const FLAC__StreamDecoder* decoder, FLAC__byte buffer[],
                                                      size_t* bytes, void* clientData);
    static FLAC__StreamDecoderSeekStatus seekCallback(const FLAC__StreamDecoder* decoder, FLAC__uint64 offset,
                                                      void* clientData);
    static FLAC__StreamDecoderTellStatus tellCallback(const FLAC__StreamDecoder* decoder, FLAC__uint64* offset,
                                                      void* clientData);
    static FLAC__StreamDecoderLengthStatus lengthCallback(const FLAC__StreamDecoder* decoder, FLAC__uint64* length,
                                                          void* clientData);
    static FLAC__bool eofCallback(const FLAC__StreamDecoder* decoder, void* clientData);
    static FLAC__StreamDecoderWriteStatus writeCallback(const FLAC__StreamDecoder* decoder, const FLAC__Frame* frame,
                                                        const FLAC__int32* const buffer[], void* clientData);
    static void metadataCallback(const FLAC__StreamDecoder* decoder, const FLAC__StreamMetadata* metadata,
                                 void* clientData);
    static void errorCallback(const FLAC__StreamDecoder* decoder, FLAC__StreamDecoderErrorStatus status,
                              void* clientData);

    void setError(QString error);
    [[nodiscard]] uint64_t currentTimestamp() const;
    [[nodiscard]] bool finishStream();

    struct StreamDecoderDeleter
    {
        void operator()(FLAC__StreamDecoder* decoder) const
        {
            if(decoder) {
                FLAC__stream_decoder_delete(decoder);
            }
        }
    };
    using StreamDecoderPtr = std::unique_ptr<FLAC__StreamDecoder, StreamDecoderDeleter>;

    StreamDecoderPtr m_decoder;
    QIODevice* m_device;
    AudioFormat m_format;
    DecoderOptions m_options;
    std::vector<std::byte> m_pending;

    size_t m_pendingOffset;
    uint64_t m_currentFrame;
    uint64_t m_totalFrames;

    QString m_error;
    QStringList m_warnings;

    bool m_initialised;
    bool m_finished;
};
} // namespace Fooyin::Flac
