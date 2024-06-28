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

#if defined(__GNUG__)
#pragma GCC diagnostic ignored "-Wold-style-cast"
#elif defined(__clang__)
#pragma clang diagnostic ignored "-Wold-style-cast"
#endif

using namespace std::chrono_literals;

namespace {
void interleaveSamples(uint8_t** in, Fooyin::AudioBuffer& buffer)
{
    const auto format  = buffer.format();
    const int channels = format.channelCount();
    const int samples  = buffer.frameCount();
    const int bps      = format.bytesPerSample();
    auto* out          = buffer.data();

    for(int i{0}; i < samples; ++i) {
        for(int ch{0}; ch < channels; ++ch) {
            const auto inOffset  = i * bps;
            const auto outOffset = (i * channels + ch) * bps;
            std::memmove(out + outOffset, in[ch] + inOffset, bps);
        }
    }
}

void interleave(uint8_t** in, Fooyin::AudioBuffer& buffer)
{
    if(!buffer.isValid()) {
        return;
    }

    if(buffer.format().sampleFormat() != Fooyin::SampleFormat::Unknown) {
        interleaveSamples(in, buffer);
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
    FFmpegDecoder* m_self;

    FormatContextPtr m_context;
    Stream m_stream;
    Codec m_codec;
    AudioFormat m_audioFormat;

    Error m_error{Error::NoError};
    AVRational m_timeBase;
    bool m_isSeekable{false};
    bool m_draining{false};
    bool m_isDecoding{false};

    AudioBuffer m_buffer;
    int m_bufferPos{0};
    uint64_t m_currentPts{0};

    explicit Private(FFmpegDecoder* self)
        : m_self{self}
        , m_timeBase{0, 0}
    { }

    bool setup(const QString& source)
    {
        m_context.reset();
        m_stream = {};
        m_codec  = {};
        m_buffer = {};

        m_error = Error::NoError;

        if(!createAVFormatContext(source)) {
            return false;
        }

        m_isSeekable = !(m_context->ctx_flags & AVFMTCTX_UNSEEKABLE);

        if(!findStream(m_context)) {
            return false;
        }

        m_audioFormat = Utils::audioFormatFromCodec(m_stream.avStream()->codecpar);

        return createCodec(m_stream.avStream());
    }

    bool createAVFormatContext(const QString& source)
    {
        AVFormatContext* avContext{nullptr};

        const int ret = avformat_open_input(&avContext, source.toUtf8().constData(), nullptr, nullptr);
        if(ret < 0) {
            if(ret == AVERROR(EACCES)) {
                Utils::printError(QStringLiteral("Access denied: ") + source);
                m_error = Error::AccessDeniedError;
            }
            else if(ret == AVERROR(EINVAL)) {
                Utils::printError(QStringLiteral("Invalid format: ") + source);
                m_error = Error::FormatError;
            }
            return false;
        }

        if(avformat_find_stream_info(avContext, nullptr) < 0) {
            Utils::printError(QStringLiteral("Could not find stream info"));
            avformat_close_input(&avContext);
            m_error = Error::ResourceError;
            return false;
        }

        //        av_dump_format(avContext, 0, data, 0);

        m_context.reset(avContext);

        return true;
    }

    bool findStream(const FormatContextPtr& formatContext)
    {
        for(unsigned int i = 0; i < formatContext->nb_streams; ++i) {
            AVStream* avStream = formatContext->streams[i];
            const auto type    = avStream->codecpar->codec_type;

            if(type == AVMEDIA_TYPE_AUDIO) {
                m_timeBase = avStream->time_base;
                m_stream   = Fooyin::Stream{avStream};
                return true;
            }
        }

        m_error = Error::ResourceError;

        return false;
    }

    bool createCodec(AVStream* avStream)
    {
        if(!avStream) {
            return false;
        }

        const AVCodec* avCodec = avcodec_find_decoder(avStream->codecpar->codec_id);
        if(!avCodec) {
            Utils::printError(QStringLiteral("Could not find a decoder for stream"));
            m_error = Error::ResourceError;
            return false;
        }

        Fooyin::CodecContextPtr avCodecContext{avcodec_alloc_context3(avCodec)};
        if(!avCodecContext) {
            Utils::printError(QStringLiteral("Could not allocate context"));
            m_error = Error::ResourceError;
            return false;
        }

        if(avCodecContext->codec_type != AVMEDIA_TYPE_AUDIO) {
            return false;
        }

        if(avcodec_parameters_to_context(avCodecContext.get(), avStream->codecpar) < 0) {
            Utils::printError(QStringLiteral("Could not obtain codec parameters"));
            m_error = Error::ResourceError;
            return {};
        }

        avCodecContext.get()->pkt_timebase = m_timeBase;

        if(avcodec_open2(avCodecContext.get(), avCodec, nullptr) < 0) {
            Utils::printError(QStringLiteral("Could not initialise codec context"));
            m_error = Error::ResourceError;
            return {};
        }

        m_codec = {std::move(avCodecContext), avStream};

        return true;
    }

    void decodeAudio(const Packet& packet)
    {
        if(!m_isDecoding) {
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
        return m_error != Error::NoError;
    }

    [[nodiscard]] int sendAVPacket(const Packet& packet) const
    {
        if(hasError() || !m_isDecoding) {
            return -1;
        }

        return avcodec_send_packet(m_codec.context(), !packet.isValid() || m_draining ? nullptr : packet.avPacket());
    }

    void receiveAVFrames()
    {
        if(hasError() || !m_isDecoding) {
            return;
        }

        auto avFrame     = FramePtr(av_frame_alloc());
        const int result = avcodec_receive_frame(m_codec.context(), avFrame.get());

        if(result == AVERROR_EOF) {
            return;
        }

        if(result == AVERROR(EAGAIN)) {
            readNext();
            return;
        }

        if(result < 0) {
            qWarning() << "Error receiving decoded frame";
            return;
        }

        const Frame frame{std::move(avFrame), m_timeBase};

        m_currentPts = frame.ptsMs();

        const auto sampleCount = m_audioFormat.bytesPerFrame() * frame.sampleCount();

        if(av_sample_fmt_is_planar(frame.format())) {
            m_buffer = {m_audioFormat, frame.ptsMs()};
            m_buffer.resize(static_cast<size_t>(sampleCount));
            interleave(frame.avFrame()->data, m_buffer);
        }
        else {
            m_buffer = {frame.avFrame()->data[0], static_cast<size_t>(sampleCount), m_audioFormat, frame.ptsMs()};
        }
    }

    void readNext()
    {
        if(!m_isDecoding) {
            return;
        }

        const Packet packet(PacketPtr{av_packet_alloc()});
        const int readResult = av_read_frame(m_context.get(), packet.avPacket());
        if(readResult < 0) {
            if(readResult != AVERROR_EOF) {
                Utils::printError(readResult);
            }
            else if(!m_draining) {
                m_draining = true;
                decodeAudio(packet);
                return;
            }
            return;
        }

        if(packet.avPacket()->stream_index != m_codec.streamIndex()) {
            readNext();
            return;
        }

        decodeAudio(packet);
    }

    void seek(uint64_t pos) const
    {
        if(!m_context || !m_isSeekable || hasError()) {
            return;
        }

        const int64_t timestamp = av_rescale_q(static_cast<int64_t>(pos), {1, 1000}, m_stream.avStream()->time_base);

        const int flags = pos < m_currentPts ? AVSEEK_FLAG_BACKWARD : 0;
        if(av_seek_frame(m_context.get(), m_stream.index(), timestamp, flags) < 0) {
            qWarning() << "Could not seek to position: " << pos;
            return;
        }

        avcodec_flush_buffers(m_codec.context());
    }
};

FFmpegDecoder::FFmpegDecoder()
    : p{std::make_unique<Private>(this)}
{ }

QStringList FFmpegDecoder::supportedExtensions() const
{
    return FFmpegDecoder::extensions();
}

FFmpegDecoder::~FFmpegDecoder() = default;

bool FFmpegDecoder::init(const QString& source)
{
    return p->setup(source);
}

void FFmpegDecoder::start()
{
    p->m_isDecoding = true;
}

void FFmpegDecoder::stop()
{
    p->seek(0);
    p->m_isDecoding = false;
    p->m_draining   = false;
    p->m_currentPts = 0;
    p->m_bufferPos  = 0;
    p->m_buffer.clear();
}

AudioFormat FFmpegDecoder::format() const
{
    return p->m_audioFormat;
}

bool FFmpegDecoder::isSeekable() const
{
    return p->m_isSeekable;
}

void FFmpegDecoder::seek(uint64_t pos)
{
    p->seek(pos);
}

AudioBuffer FFmpegDecoder::readBuffer(size_t bytes)
{
    if(!p->m_isDecoding || p->hasError()) {
        return {};
    }

    if(!p->m_buffer.isValid()) {
        p->readNext();
    }

    AudioBuffer buffer;

    const int bytesRequested = static_cast<int>(bytes);
    int bytesWritten{0};

    while(p->m_buffer.isValid() && bytesWritten < bytesRequested) {
        if(!buffer.isValid()) {
            buffer = {p->m_buffer.format(), p->m_buffer.startTime()};
        }
        const int remaining = bytesRequested - bytesWritten;
        const int count     = p->m_buffer.byteCount() - p->m_bufferPos;
        if(count <= remaining) {
            buffer.append(p->m_buffer.data() + p->m_bufferPos, count);
            bytesWritten += count;
            p->m_buffer    = {};
            p->m_bufferPos = 0;
            p->readNext();
        }
        else {
            buffer.append(p->m_buffer.data() + p->m_bufferPos, remaining);
            bytesWritten += remaining;
            p->m_bufferPos += remaining;
        }
    }

    return buffer;
}

AudioDecoder::Error FFmpegDecoder::error() const
{
    return p->m_error;
}

QStringList FFmpegDecoder::extensions()
{
    static QStringList extensions{QStringLiteral("mp3"),  QStringLiteral("ogg"),  QStringLiteral("opus"),
                                  QStringLiteral("oga"),  QStringLiteral("m4a"),  QStringLiteral("wav"),
                                  QStringLiteral("wv"),   QStringLiteral("flac"), QStringLiteral("wma"),
                                  QStringLiteral("mpc"),  QStringLiteral("aiff"), QStringLiteral("ape"),
                                  QStringLiteral("webm"), QStringLiteral("mp4")};
    return extensions;
}
} // namespace Fooyin
