/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
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

extern "C"
{
#include <libavutil/frame.h>
}

#include <memory>

namespace Fooyin {
struct FrameDeleter
{
    void operator()(AVFrame* frame) const
    {
        if(frame != nullptr) {
            av_frame_free(&frame);
        }
    }
};
using FramePtr = std::unique_ptr<AVFrame, FrameDeleter>;

class Frame
{
public:
    explicit Frame(AVRational timeBase);
    ~Frame();

    [[nodiscard]] bool isValid() const;

    [[nodiscard]] AVFrame* avFrame() const;

    [[nodiscard]] int channelCount() const;
    [[nodiscard]] int sampleRate() const;
    [[nodiscard]] AVSampleFormat format() const;

    [[nodiscard]] int sampleCount() const;

    [[nodiscard]] uint64_t ptsMs() const;
    [[nodiscard]] uint64_t durationMs() const;

    [[nodiscard]] uint64_t end() const;

private:
    FramePtr m_frame;
    AVRational m_timeBase;
};
} // namespace Fooyin
