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

#include <mpg123.h>

namespace Fooyin::Mpg123 {
class MpegDecoder : public AudioDecoder
{
public:
    MpegDecoder();

    [[nodiscard]] QStringList extensions() const override;
    [[nodiscard]] bool isSeekable() const override;
    [[nodiscard]] QStringList takeWarnings() override;
    [[nodiscard]] int bitrate() const override;

    std::optional<AudioFormat> init(const AudioSource& source, const Track& track, DecoderOptions options) override;
    void stop() override;

    void seek(uint64_t pos) override;

    ReadResult readAudio(size_t bytes) override;
    AudioBuffer readBuffer(size_t bytes) override;

private:
    static mpg123_ssize_t readCallback(void* handle, void* buffer, size_t bytes);
    static off_t seekCallback(void* handle, off_t offset, int whence);

    [[nodiscard]] bool configureOutput() const;
    bool updateFormat();
    void updateBitrate();
    [[nodiscard]] QString decoderError() const;
    [[nodiscard]] uint64_t currentTimestamp() const;

    struct DecoderDeleter
    {
        void operator()(mpg123_handle* decoder) const
        {
            if(decoder) {
                mpg123_delete(decoder);
            }
        }
    };
    using DecoderPtr = std::unique_ptr<mpg123_handle, DecoderDeleter>;

    DecoderPtr m_decoder;
    QIODevice* m_device;
    AudioFormat m_format;
    DecoderOptions m_options;
    QStringList m_warnings;
    uint64_t m_currentFrame;
    int m_bitrate;
    bool m_finished;
};
} // namespace Fooyin::Mpg123
