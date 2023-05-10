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

#include "ffmpegcodec.h"

namespace Fy::Core::Engine::FFmpeg {
std::optional<Codec> Codec::create(AVStream* stream)
{
    if(!stream) {
        return {};
    }

    const AVCodec* decoder = avcodec_find_decoder(stream->codecpar->codec_id);
    if(!decoder) {
        return {};
    }

    CodecContextPtr context(avcodec_alloc_context3(decoder));
    if(!context) {
        return {};
    }

    if(context->codec_type != AVMEDIA_TYPE_AUDIO) {
        return {};
    }

    int ret = avcodec_parameters_to_context(context.get(), stream->codecpar);
    if(ret < 0) {
        return {};
    }

    AVDictionary* opts{nullptr};
    av_dict_set(&opts, "refcounted_frames", "1", 0);
    av_dict_set(&opts, "threads", "auto", 0);
    ret = avcodec_open2(context.get(), decoder, &opts);
    if(ret < 0) {
        return {};
    }

    return Codec{std::move(context), stream};
}

Codec::Codec(Codec&& other)
    : m_context{std::move(other.m_context)}
    , m_stream{other.m_stream}
{ }

Codec& Codec::operator=(Codec&& other)
{
    m_context = std::move(other.m_context);
    m_stream  = std::move(other.m_stream);
    return *this;
}

AVCodecContext* Codec::context() const
{
    return m_context.get();
}

AVStream* Codec::stream() const
{
    return m_stream;
}

uint Codec::streamIndex() const
{
    return m_stream->index;
}

Codec::Codec(CodecContextPtr context, AVStream* stream)
    : m_context{std::move(context)}
    , m_stream{stream}
{ }
} // namespace Fy::Core::Engine::FFmpeg
