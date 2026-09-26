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

#include <opus/opusfile.h>

namespace Fooyin::Opus {
class OpusDecoder : public AudioDecoder
{
public:
    OpusDecoder();

    [[nodiscard]] QStringList extensions() const override;
    [[nodiscard]] bool isSeekable() const override;
    [[nodiscard]] int bitrate() const override;
    [[nodiscard]] QStringList takeWarnings() override;

    std::optional<AudioFormat> init(const AudioSource& source, const Track& track, DecoderOptions options) override;
    void stop() override;

    void seek(uint64_t pos) override;

    ReadResult readAudio(size_t bytes) override;
    AudioBuffer readBuffer(size_t bytes) override;

private:
    static int readCallback(void* source, unsigned char* buffer, int bytes);
    static int seekCallback(void* source, opus_int64 offset, int whence);
    static opus_int64 tellCallback(void* source);

    [[nodiscard]] uint64_t currentTimestamp() const;

    struct DecoderDeleter
    {
        void operator()(OggOpusFile* decoder) const
        {
            if(decoder) {
                op_free(decoder);
            }
        }
    };
    using DecoderPtr = std::unique_ptr<OggOpusFile, DecoderDeleter>;

    DecoderPtr m_decoder;
    QIODevice* m_device;
    AudioFormat m_format;
    DecoderOptions m_options;
    QStringList m_warnings;
    uint64_t m_currentFrame;
    uint64_t m_totalFrames;
    int m_bitrate;
    bool m_finished;
};
} // namespace Fooyin::Opus
