/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <LukeT1@proton.me>
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

extern "C"
{
#include <libswresample/swresample.h>
}

namespace Fooyin {
struct SwrContextDeleter
{
    void operator()(SwrContext* context) const
    {
        if(context) {
            swr_free(&context);
        }
    }
};
using SwrContextPtr = std::unique_ptr<SwrContext, SwrContextDeleter>;

class FFmpegResampler
{
public:
    FFmpegResampler(const AudioFormat& inFormat, const AudioFormat& outFormat, uint64_t startTime = 0);

    [[nodiscard]] bool canResample() const;

    AudioBuffer resample(const AudioBuffer& buffer);

private:
    AudioFormat m_inFormat;
    AudioFormat m_outFormat;
    uint64_t m_startTime;

    SwrContextPtr m_context;
    uint64_t m_samplesConverted;
};
} // namespace Fooyin
