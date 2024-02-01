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

#include "ffmpegdecoder.h"

#include "ffmpegcodec.h"
#include "ffmpegframe.h"
#include "ffmpegpacket.h"
#include "ffmpegstream.h"
#include "ffmpegutils.h"

#include <core/engine/audiobuffer.h>
#include <utils/worker.h>

#include <QDebug>

#include <bit>
#include <chrono>
#include <queue>

using namespace std::chrono_literals;
using namespace Qt::Literals::StringLiterals;

namespace {
template <typename T>
void interleaveSamples(uint8_t** in, Fooyin::AudioBuffer& buffer)
{
    const auto* out    = buffer.data();
    const auto format  = buffer.format();
    const int channels = format.channelCount();
    const int frames   = buffer.frameCount();

    for(int ch{0}; ch < channels; ++ch) {
        const auto* pSamples = std::bit_cast<const T*>(in[ch]);
        auto* iSamples       = std::bit_cast<T*>(out) + ch;
        auto end             = pSamples + frames;
        while(pSamples < end) {
            std::memmove(iSamples, pSamples, format.bytesPerSample());
            pSamples++;
            iSamples += channels;
        }
    }
}

void interleave(uint8_t** in, Fooyin::AudioBuffer& buffer)
{
    if(!buffer.isValid()) {
        return;
    }

    const auto format = buffer.format();

    switch(format.sampleFormat()) {
        case(Fooyin::AudioFormat::UInt8):
            interleaveSamples<uint8_t>(in, buffer);
            break;
        case(Fooyin::AudioFormat::Int16):
            interleaveSamples<int16_t>(in, buffer);
            break;
        case(Fooyin::AudioFormat::Int32):
            interleaveSamples<int32_t>(in, buffer);
            break;
        case(Fooyin::AudioFormat::Int64):
            interleaveSamples<int64_t>(in, buffer);
            break;
        case(Fooyin::AudioFormat::Float):
            interleaveSamples<float>(in, buffer);
            break;
        case(Fooyin::AudioFormat::Double):
            interleaveSamples<double>(in, buffer);
            break;
        case(Fooyin::AudioFormat::Unknown):
            break;
    }
}

struct FormatContextDeleter
{
    void operator()(AVFormatContext* context) const
    {
        if(context) {
            avformat_close_input(&context);
            avformat_free_context(context);
        }
    }
};
using FormatContextPtr = std::unique_ptr<AVFormatContext, FormatContextDeleter>;
} // namespace

namespace Fooyin {
struct FFmpegDecoder::Private
{
    FFmpegDecoder* self;

    FormatContextPtr context;
    Stream stream;
    Codec codec;
    AudioFormat audioFormat;

    Error error{NoError};
    bool isSeekable{false};
    bool draining{false};
    bool isDecoding{false};

    AudioBuffer buffer;
    uint64_t currentPts{0};

    explicit Private(FFmpegDecoder* self)
        : self{self}
    { }

    bool setup(const QString& source)
    {
        error = Error::NoError;

        if(!createAVFormatContext(source)) {
            return false;
        }

        isSeekable = !(context->ctx_flags & AVFMTCTX_UNSEEKABLE);

        if(!findStream(context)) {
            return false;
        }

        audioFormat = Utils::audioFormatFromCodec(stream.avStream()->codecpar);

        return createCodec(stream.avStream());
    }

    bool createAVFormatContext(const QString& source)
    {
        AVFormatContext* avContext{nullptr};

        const int ret = avformat_open_input(&avContext, source.toUtf8().constData(), nullptr, nullptr);
        if(ret < 0) {
            if(ret == AVERROR(EACCES)) {
                Utils::printError(u"Access denied: "_s + source);
                error = Error::AccessDeniedError;
            }
            else if(ret == AVERROR(EINVAL)) {
                Utils::printError(u"Invalid format: "_s + source);
                error = Error::FormatError;
            }
            return false;
        }

        if(avformat_find_stream_info(avContext, nullptr) < 0) {
            Utils::printError(u"Could not find stream info"_s);
            avformat_close_input(&avContext);
            error = Error::ResourceError;
            return false;
        }

        //        av_dump_format(avContext, 0, data, 0);

        context.reset(avContext);

        return true;
    }

    bool findStream(const FormatContextPtr& formatContext)
    {
        for(unsigned int i = 0; i < formatContext->nb_streams; ++i) {
            AVStream* avStream = formatContext->streams[i];
            const auto type    = avStream->codecpar->codec_type;

            if(type == AVMEDIA_TYPE_AUDIO) {
                stream = Fooyin::Stream{avStream};
                return true;
            }
        }

        error = Error::ResourceError;

        return false;
    }

    bool createCodec(AVStream* avStream)
    {
        if(!avStream) {
            return false;
        }

        const AVCodec* avCodec = avcodec_find_decoder(avStream->codecpar->codec_id);
        if(!avCodec) {
            Utils::printError(u"Could not find a decoder for stream"_s);
            error = Error::ResourceError;
            return false;
        }

        Fooyin::CodecContextPtr avCodecContext{avcodec_alloc_context3(avCodec)};
        if(!avCodecContext) {
            Utils::printError(u"Could not allocate context"_s);
            error = Error::ResourceError;
            return false;
        }

        if(avCodecContext->codec_type != AVMEDIA_TYPE_AUDIO) {
            return false;
        }

        if(avcodec_parameters_to_context(avCodecContext.get(), avStream->codecpar) < 0) {
            Utils::printError(u"Could not obtain codec parameters"_s);
            error = Error::ResourceError;
            return {};
        }

        avCodecContext.get()->pkt_timebase = avStream->time_base;

        if(avcodec_open2(avCodecContext.get(), avCodec, nullptr) < 0) {
            Utils::printError(u"Could not initialise codec context"_s);
            error = Error::ResourceError;
            return {};
        }

        codec = {std::move(avCodecContext), avStream};

        return true;
    }

    void decodeAudio(const Packet& packet)
    {
        if(!isDecoding) {
            return;
        }

        int result = sendAVPacket(packet);

        if(result == AVERROR(EAGAIN)) {
            receiveAVFrames();
            result = sendAVPacket(packet);

            if(result != AVERROR(EAGAIN)) {
                qWarning() << "Unexpected decoder behavior";
            }
        }

        if(result == 0) {
            receiveAVFrames();
        }
    }

    [[nodiscard]] bool hasError() const
    {
        return error != Error::NoError;
    }

    [[nodiscard]] int sendAVPacket(const Packet& packet) const
    {
        if(hasError() || !isDecoding) {
            return -1;
        }

        return avcodec_send_packet(codec.context(), !packet.isValid() || draining ? nullptr : packet.avPacket());
    }

    void receiveAVFrames()
    {
        if(hasError() || !isDecoding) {
            return;
        }

        auto avFrame     = FramePtr(av_frame_alloc());
        const int result = avcodec_receive_frame(codec.context(), avFrame.get());

        if(result == AVERROR_EOF || result == AVERROR(EAGAIN)) {
            return;
        }
        if(result < 0) {
            qWarning() << "Error receiving decoded frame";
            return;
        }
        avFrame->time_base = context->streams[codec.streamIndex()]->time_base;
        const Frame frame{std::move(avFrame)};

        currentPts = frame.ptsMs();

        const auto sampleCount = audioFormat.bytesPerFrame() * frame.sampleCount();

        if(av_sample_fmt_is_planar(frame.format())) {
            buffer = {audioFormat, frame.ptsMs()};
            buffer.resize(static_cast<size_t>(sampleCount));
            interleave(frame.avFrame()->data, buffer);
        }
        else {
            buffer = {frame.avFrame()->data[0], static_cast<size_t>(sampleCount), audioFormat, frame.ptsMs()};
        }

        QMetaObject::invokeMethod(self, &FFmpegDecoder::bufferReady);
    }

    void readNext()
    {
        if(!isDecoding) {
            return;
        }

        const Packet packet(PacketPtr{av_packet_alloc()});
        const int readResult = av_read_frame(context.get(), packet.avPacket());
        if(readResult < 0) {
            if(readResult != AVERROR_EOF) {
                Utils::printError(readResult);
            }
            else if(!draining) {
                draining = true;
                decodeAudio(packet);
                return;
            }

            self->stop();
            QMetaObject::invokeMethod(self, &FFmpegDecoder::finished);
            return;
        }

        if(packet.avPacket()->stream_index != codec.streamIndex()) {
            readNext();
            return;
        }

        packet.avPacket()->time_base = codec.stream()->time_base;

        decodeAudio(packet);
    }

    void seek(uint64_t pos) const
    {
        if(!isDecoding || !isSeekable || hasError()) {
            return;
        }

        const int64_t timestamp = av_rescale_q(static_cast<int64_t>(pos), {1, 1000}, stream.avStream()->time_base);

        const int flags = pos < currentPts ? AVSEEK_FLAG_BACKWARD : 0;
        if(av_seek_frame(context.get(), stream.index(), timestamp, flags) < 0) {
            qWarning() << "Could not seek to position: " << pos;
            return;
        }

        avcodec_flush_buffers(codec.context());
    }
};

FFmpegDecoder::FFmpegDecoder(QObject* parent)
    : AudioDecoder{parent}
    , p{std::make_unique<Private>(this)}
{
    setObjectName("Decoder");
}

FFmpegDecoder::~FFmpegDecoder() = default;

bool FFmpegDecoder::init(const QString& source)
{
    return p->setup(source);
}

void FFmpegDecoder::start()
{
    if(p->hasError()) {
        return;
    }

    if(!std::exchange(p->isDecoding, true)) {
        p->readNext();
    }
}

void FFmpegDecoder::stop()
{
    p->isDecoding = false;
    p->draining   = false;

    p->context.reset();
    p->stream = {};
    p->codec  = {};
    p->buffer = {};
}

AudioFormat FFmpegDecoder::format() const
{
    return p->audioFormat;
}

bool FFmpegDecoder::isSeekable() const
{
    return p->isSeekable;
}

void FFmpegDecoder::seek(uint64_t pos)
{
    p->seek(pos);
    p->readNext();
}

AudioBuffer FFmpegDecoder::readBuffer()
{
    if(!p->isDecoding || p->hasError()) {
        return {};
    }

    QMetaObject::invokeMethod(
        this, [this]() { p->readNext(); }, Qt::QueuedConnection);

    return std::exchange(p->buffer, {});
}

AudioDecoder::Error FFmpegDecoder::error() const
{
    return p->error;
}
} // namespace Fooyin

#include "moc_ffmpegdecoder.cpp"
