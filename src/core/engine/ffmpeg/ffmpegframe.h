/*
 * Fooyin
 * Copyright 2022-2023, Luke Taylor <LukeT1@proton.me>
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

#include "ffmpegcodec.h"

extern "C"
{
#include <libavutil/frame.h>
}

namespace Fy::Core::Engine::FFmpeg {
struct FrameDeleter
{
    void operator()(AVFrame* frame) const
    {
        if(!!frame) {
            av_frame_free(&frame);
        }
    }
};
using FramePtr = std::unique_ptr<AVFrame, FrameDeleter>;

class Frame
{
public:
    Frame() = default;
    Frame(FramePtr f, const Codec& codec);

    Frame(Frame&& other);
    Frame(const Frame& other)            = delete;
    Frame& operator=(const Frame& other) = delete;

    bool isValid() const;

    AVFrame* avFrame() const;
    const Codec* codec() const;

    int channelCount() const;
    int sampleRate() const;
    AVSampleFormat format() const;

    // Returns pts in milliseconds
    uint64_t pts() const;
    uint64_t duration() const;
    uint64_t end() const;

private:
    FramePtr m_frame;
    const Codec* m_codec;
};
} // namespace Fy::Core::Engine::FFmpeg
