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

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

#include <memory>

namespace Fy::Core::Engine::FFmpeg {
struct CodecContextDeleter
{
    void operator()(AVCodecContext* context) const
    {
        if(context) {
            avcodec_free_context(&context);
        }
    }
};
using CodecContextPtr = std::unique_ptr<AVCodecContext, CodecContextDeleter>;

class Codec
{
public:
    Codec(CodecContextPtr context, AVStream* stream);

    Codec(Codec&& other) noexcept ;
    Codec& operator=(Codec&& other);

    Codec(const Codec& other)            = delete;
    Codec& operator=(const Codec& other) = delete;

    [[nodiscard]] AVCodecContext* context() const;
    [[nodiscard]] AVStream* stream() const;
    [[nodiscard]] int streamIndex() const;

private:
    CodecContextPtr m_context;
    AVStream* m_stream;
};
} // namespace Fy::Core::Engine::FFmpeg
