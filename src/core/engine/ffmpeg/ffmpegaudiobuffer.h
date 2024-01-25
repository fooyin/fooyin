/*
 * Fooyin
 * Copyright 2022-2024, Luke Taylor <LukeT1@proton.me>
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

#include <core/engine/audiobuffer.h>
#include <core/engine/audioformat.h>

extern "C"
{
#include <libavutil/samplefmt.h>
}

#include <QExplicitlySharedDataPointer>

namespace Fooyin {
class FFmpegAudioBufferPrivate;

class FFmpegAudioBuffer : public AudioBuffer
{
public:
    FFmpegAudioBuffer();
    FFmpegAudioBuffer(QByteArray data, AudioFormat format, uint64_t startTime);
    ~FFmpegAudioBuffer() override;

    FFmpegAudioBuffer(const FFmpegAudioBuffer& other);
    FFmpegAudioBuffer& operator=(const FFmpegAudioBuffer& other);
    FFmpegAudioBuffer(FFmpegAudioBuffer&& other) noexcept;

    bool isValid() const;
    void detach();

    AudioFormat format() const override;
    int frameCount() const override;
    int sampleCount() const;
    int byteCount() const;
    uint64_t startTime() const override;
    uint64_t duration() const override;

    uint8_t* constData() const override;
    uint8_t* data() override;

private:
    QExplicitlySharedDataPointer<FFmpegAudioBufferPrivate> p;
};
} // namespace Fooyin
