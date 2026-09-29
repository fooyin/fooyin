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

#include <vorbis/vorbisfile.h>

namespace Fooyin::Vorbis {
class VorbisDecoder : public AudioDecoder
{
public:
    VorbisDecoder();

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
    static size_t readCallback(void* buffer, size_t size, size_t count, void* source);
    static int seekCallback(void* source, ogg_int64_t offset, int whence);
    static long tellCallback(void* source);

    [[nodiscard]] bool formatMatches(int link) const;
    [[nodiscard]] uint64_t currentTimestamp() const;

    struct DecoderDeleter
    {
        void operator()(OggVorbis_File* decoder) const
        {
            if(decoder) {
                ov_clear(decoder);
                delete decoder;
            }
        }
    };
    using DecoderPtr = std::unique_ptr<OggVorbis_File, DecoderDeleter>;

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
} // namespace Fooyin::Vorbis
