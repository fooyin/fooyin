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

#include <core/engine/audioencoder.h>

#include <FLAC/stream_encoder.h>

#include <QFile>

namespace Fooyin::Flac {
class FlacEncoder : public AudioEncoder
{
public:
    FlacEncoder();
    ~FlacEncoder() override;

    [[nodiscard]] std::vector<AudioEncoderInfo> availableEncoders() const override;

    Result init(const QString& outputPath, const AudioFormat& inputFormat,
                const AudioEncoderSettings& settings) override;
    Result write(const AudioBuffer& buffer) override;
    Result finish() override;

private:
    static FLAC__StreamEncoderWriteStatus writeCallback(const FLAC__StreamEncoder* encoder, const FLAC__byte buffer[],
                                                        size_t bytes, uint32_t samples, uint32_t currentFrame,
                                                        void* clientData);
    static FLAC__StreamEncoderSeekStatus seekCallback(const FLAC__StreamEncoder* encoder, FLAC__uint64 offset,
                                                      void* clientData);
    static FLAC__StreamEncoderTellStatus tellCallback(const FLAC__StreamEncoder* encoder, FLAC__uint64* offset,
                                                      void* clientData);

    void cleanup();
    void setError(QString error);

    struct StreamEncoderDeleter
    {
        void operator()(FLAC__StreamEncoder* encoder) const
        {
            FLAC__stream_encoder_delete(encoder);
        }
    };
    using StreamEncoderPtr = std::unique_ptr<FLAC__StreamEncoder, StreamEncoderDeleter>;

    StreamEncoderPtr m_encoder;
    QFile m_file;
    AudioFormat m_inputFormat;
    AudioFormat m_outputFormat;
    QString m_error;
    bool m_dither;
    bool m_initialised;
    bool m_finished;
};
} // namespace Fooyin::Flac
