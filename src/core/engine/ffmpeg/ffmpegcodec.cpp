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

#include "ffmpegcodec.h"

namespace Fooyin {
Codec::Codec(CodecContextPtr context, AVStream* stream)
    : m_context{std::move(context)}
    , m_stream{stream}
{ }

Codec::Codec(Codec&& other) noexcept
    : m_context{std::move(other.m_context)}
    , m_stream{other.m_stream}
{ }

Codec& Codec::operator=(Codec&& other) noexcept
{
    m_context = std::move(other.m_context);
    m_stream  = other.m_stream;
    return *this;
}

bool Codec::isValid() const
{
    return m_context && m_stream;
}

AVCodecContext* Codec::context() const
{
    return m_context.get();
}

AVStream* Codec::stream() const
{
    return m_stream;
}

int Codec::streamIndex() const
{
    return m_stream ? m_stream->index : -1;
}

bool Codec::isPlanar() const
{
    return av_sample_fmt_is_planar(m_context->sample_fmt);
}
} // namespace Fooyin
