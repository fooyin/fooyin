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

#include "ffmpegframe.h"

#include <QDebug>

namespace Fy::Core::Engine::FFmpeg {
Frame::Frame(FramePtr f, const Codec& codec)
    : m_frame{std::move(f)}
    , m_codec{&codec}
{ }

Frame::Frame(Frame&& other)
    : m_frame{std::move(other.m_frame)}
    , m_codec{std::move(other.m_codec)}
{ }

bool Frame::isValid() const
{
    return !!m_frame;
}

AVFrame* Frame::avFrame() const
{
    return m_frame.get();
}

const Codec* Frame::codec() const
{
    return m_codec ? m_codec : nullptr;
}

int Frame::channelCount() const
{
    return m_frame->ch_layout.nb_channels;
}

int Frame::sampleRate() const
{
    return m_frame->sample_rate;
}

AVSampleFormat Frame::format() const
{
    return static_cast<AVSampleFormat>(m_frame->format);
}

uint64_t Frame::pts() const
{
    if(m_frame->pts < 0) {
        return 0;
    }
    return av_rescale_q(m_frame->pts, m_codec->stream()->time_base, {1, 1000});
}

uint64_t Frame::duration() const
{
    return av_rescale_q(m_frame->duration, m_codec->stream()->time_base, {1, 1000});
}

uint64_t Frame::end() const
{
    return m_frame->pts + m_frame->duration;
}
} // namespace Fy::Core::Engine::FFmpeg
