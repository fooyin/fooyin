/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <luket@pm.me>
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

#include <QLoggingCategory>

extern "C"
{
#include <libavcodec/avcodec.h>
}

#include <memory>

class QString;
struct AVCodecParameters;
struct AVCodecContext;
struct AVAudioFifo;
struct AVFormatContext;
struct AVIOContext;
struct AVPacket;
struct SwrContext;

#define OLD_CHANNEL_LAYOUT (LIBAVCODEC_VERSION_INT < AV_VERSION_INT(59, 24, 100))
#define OLD_CODEC_SUPPORTED_CONFIG (LIBAVCODEC_VERSION_MAJOR < 61)
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

struct OutputFormatContextDeleter
{
    void operator()(AVFormatContext* context) const;
};
using OutputFormatContextPtr = std::unique_ptr<AVFormatContext, OutputFormatContextDeleter>;

struct CodecContextDeleter
{
    void operator()(AVCodecContext* context) const;
};
using CodecContextPtr = std::unique_ptr<AVCodecContext, CodecContextDeleter>;

struct FrameDeleter
{
    void operator()(AVFrame* frame) const;
};
using FramePtr = std::unique_ptr<AVFrame, FrameDeleter>;

struct FormatContext
{
    FormatContextPtr formatContext;
    IOContextPtr ioContext;
    std::shared_ptr<void> ioContextData;
};

struct PacketDeleter
{
    void operator()(AVPacket* packet) const;
};
using PacketPtr = std::unique_ptr<AVPacket, PacketDeleter>;

struct SwrContextDeleter
{
    void operator()(SwrContext* context) const;
};
using SwrContextPtr = std::unique_ptr<SwrContext, SwrContextDeleter>;

struct AudioFifoDeleter
{
    void operator()(AVAudioFifo* fifo) const;
};
using AudioFifoPtr = std::unique_ptr<AVAudioFifo, AudioFifoDeleter>;

namespace Fooyin::Utils {
FYCORE_EXPORT QString ffmpegErrorString(int error);
FYCORE_EXPORT void printError(int error);
FYCORE_EXPORT void printError(const QString& error);

FYCORE_EXPORT SampleFormat sampleFormat(AVSampleFormat format, int bps);
FYCORE_EXPORT AVSampleFormat sampleFormat(SampleFormat format, bool planar = false);
FYCORE_EXPORT AudioFormat audioFormatFromCodec(AVCodecParameters* codec, AVSampleFormat ctxFormat);
FYCORE_EXPORT Stream findAudioStream(AVFormatContext* context);
} // namespace Fooyin::Utils
