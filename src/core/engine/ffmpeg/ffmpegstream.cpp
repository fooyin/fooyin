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

#include "ffmpegstream.h"

extern "C"
{
#include <libavformat/avformat.h>
}

namespace Fooyin {
Stream::Stream(AVStream* stream)
    : m_stream{stream}
{ }

int Stream::index() const
{
    return m_stream->index;
}

uint64_t Stream::duration() const
{
    return av_rescale_q(m_stream->duration, m_stream->time_base, {1, 1000});
}

AVStream* Stream::avStream() const
{
    return m_stream;
}
} // namespace Fooyin
