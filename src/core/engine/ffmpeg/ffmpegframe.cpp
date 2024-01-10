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

#include "ffmpegframe.h"

#include <QDebug>

namespace Fooyin {
Frame::Frame(FramePtr frame)
    : p{new Private(std::move(frame))}
{ }

bool Frame::isValid() const
{
    return p && p->frame;
}

AVFrame* Frame::avFrame() const
{
    return p->frame ? p->frame.get() : nullptr;
}

int Frame::channelCount() const
{
    return p->frame ? p->frame->ch_layout.nb_channels : 0;
}

int Frame::sampleRate() const
{
    return p->frame ? p->frame->sample_rate : 0;
}

AVSampleFormat Frame::format() const
{
    if(!p->frame) {
        return AV_SAMPLE_FMT_NONE;
    }
    return static_cast<AVSampleFormat>(p->frame->format);
}

int Frame::sampleCount() const
{
    return p->frame ? p->frame->nb_samples : 0;
}

uint64_t Frame::ptsMs() const
{
    if(!p->frame || p->frame->pts < 0) {
        return 0;
    }
    return av_rescale_q(p->frame->pts, p->frame->time_base, {1, 1000});
}

uint64_t Frame::durationMs() const
{
    if(!p->frame) {
        return 0;
    }
    return av_rescale_q(p->frame->duration, p->frame->time_base, {1, 1000});
}

uint64_t Frame::end() const
{
    if(!p->frame) {
        return 0;
    }
    return p->frame->pts + p->frame->duration;
}
} // namespace Fooyin
