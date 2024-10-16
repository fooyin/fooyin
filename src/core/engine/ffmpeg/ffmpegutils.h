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

#include "fycore_export.h"

#include "ffmpegstream.h"

#include <core/engine/audioformat.h>

#if defined(__GNUG__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#elif defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wold-style-cast"
#endif

extern "C"
{
#include <libavcodec/avcodec.h>
}

#if defined(__GNUG__)
#pragma GCC diagnostic pop
#elif defined(__clang__)
#pragma clang diagnostic pop
#endif

#include <QLoggingCategory>

#include <memory>

class QString;
struct AVCodecParameters;
struct AVFormatContext;
struct AVIOContext;
struct AVPacket;

#define OLD_CHANNEL_LAYOUT (LIBAVCODEC_VERSION_INT < AV_VERSION_INT(59, 24, 100))
#define OLD_FRAME (LIBAVUTIL_VERSION_MAJOR < 58)

FYCORE_EXPORT Q_DECLARE_LOGGING_CATEGORY(FFMPEG)

struct IOContextDeleter
{
    void operator()(AVIOContext* context) const;
};
using IOContextPtr = std::unique_ptr<AVIOContext, IOContextDeleter>;

struct FormatContextDeleter
{
    void operator()(AVFormatContext* context) const;
};
using FormatContextPtr = std::unique_ptr<AVFormatContext, FormatContextDeleter>;

struct FrameDeleter
{
    void operator()(AVFrame* frame) const;
};
using FramePtr = std::unique_ptr<AVFrame, FrameDeleter>;

struct FormatContext
{
    FormatContextPtr formatContext;
    IOContextPtr ioContext;
};

struct PacketDeleter
{
    void operator()(AVPacket* packet) const;
};
using PacketPtr = std::unique_ptr<AVPacket, PacketDeleter>;

namespace Fooyin::Utils {
FYCORE_EXPORT void printError(int error);
FYCORE_EXPORT void printError(const QString& error);

FYCORE_EXPORT SampleFormat sampleFormat(AVSampleFormat format, int bps);
FYCORE_EXPORT AVSampleFormat sampleFormat(SampleFormat format, bool planar = false);
FYCORE_EXPORT AudioFormat audioFormatFromCodec(AVCodecParameters* codec);
FYCORE_EXPORT Stream findAudioStream(AVFormatContext* context);
} // namespace Fooyin::Utils
