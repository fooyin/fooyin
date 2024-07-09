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

constexpr AVRational TimeBaseMs = {1, 1000};

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

struct PacketDeleter
{
    void operator()(AVPacket* packet) const
    {
        if(packet) {
            av_packet_free(&packet);
        }
    }
};
using PacketPtr = std::unique_ptr<AVPacket, PacketDeleter>;
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
    bool m_eof{false};
    bool m_isDecoding{false};

    AudioBuffer m_buffer;
    int m_bufferPos{0};
    int64_t m_seekPos{0};
    int m_skipBytes{0};

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

        // Needed for seeking of APE files
        avContext->flags |= AVFMT_FLAG_GENPTS;

        // av_dump_format(avContext, 0, source.toUtf8().constData(), 0);

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

        CodecContextPtr avCodecContext{avcodec_alloc_context3(avCodec)};
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

    void decodeAudio(const PacketPtr& packet)
    {
        if(!m_isDecoding) {
            return;
        }

        int result = sendAVPacket(packet);

        if(result == AVERROR(EAGAIN)) {
            receiveAVFrames();
            result = sendAVPacket(packet);

            if(result != AVERROR(EAGAIN)) {
                Utils::printError(QStringLiteral("Unexpected decoder behavior"));
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

    [[nodiscard]] int sendAVPacket(const PacketPtr& packet) const
    {
        if(hasError() || !m_isDecoding) {
            return -1;
        }

        if(!packet || m_draining) {
            return avcodec_send_packet(m_codec.context(), nullptr);
        }

        return avcodec_send_packet(m_codec.context(), packet.get());
    }

    int receiveAVFrames()
    {
        if(hasError() || !m_isDecoding) {
            return 0;
        }

        const Frame frame{m_timeBase};
        const int result = avcodec_receive_frame(m_codec.context(), frame.avFrame());

        if(result == AVERROR_EOF) {
            m_draining = true;
            decodeAudio({});
            return result;
        }

        if(result == AVERROR(EAGAIN)) {
            readNext();
            return result;
        }

        if(result < 0) {
            Utils::printError(QStringLiteral("Error receiving decoded frame"));
            return result;
        }

        const auto sampleCount = m_audioFormat.bytesPerFrame() * frame.sampleCount();

        if(m_codec.isPlanar()) {
            m_buffer = {m_audioFormat, frame.ptsMs()};
            m_buffer.resize(static_cast<size_t>(sampleCount));
            interleave(frame.avFrame()->data, m_buffer);
        }
        else {
            m_buffer = {frame.avFrame()->data[0], static_cast<size_t>(sampleCount), m_audioFormat, frame.ptsMs()};
        }

        // Handle seeking of APE files
        if(m_skipBytes > 0) {
            const auto len = std::min(sampleCount, m_skipBytes);
            m_skipBytes -= len;

            if(sampleCount - len == 0) {
                m_buffer = {};
            }
            else {
                std::memmove(m_buffer.data(), m_buffer.data() + len, sampleCount - len);
                m_buffer.resize(sampleCount - len);
            }
        }

        return result;
    }

    void readNext()
    {
        if(!m_isDecoding) {
            return;
        }

        // Exhaust the current packet first
        if(receiveAVFrames() == 0) {
            return;
        }

        const PacketPtr packet{av_packet_alloc()};
        const int readResult = av_read_frame(m_context.get(), packet.get());
        if(readResult < 0) {
            if(readResult == AVERROR_EOF && !m_eof) {
                decodeAudio(packet);
                m_eof = true;
            }
            else {
                receiveAVFrames();
                return;
            }
            return;
        }

        m_eof = false;

        if(packet->stream_index != m_codec.streamIndex()) {
            readNext();
            return;
        }

        if(m_seekPos > 0 && m_codec.context()->codec_id == AV_CODEC_ID_APE) {
            const auto packetPts = av_rescale_q_rnd(packet->pts, m_timeBase, TimeBaseMs, AVRounding::AV_ROUND_DOWN);
            m_skipBytes          = m_audioFormat.bytesForDuration(std::abs(m_seekPos - packetPts));
        }
        m_seekPos = -1;

        decodeAudio(packet);

        while(!m_buffer.isValid()) {
            readNext();
        }
    }

    void seek(uint64_t pos)
    {
        if(!m_context || !m_isSeekable || hasError()) {
            return;
        }

        m_seekPos = static_cast<int64_t>(pos);

        constexpr static AVRational avTb{1, AV_TIME_BASE};

        constexpr static auto min = std::numeric_limits<int64_t>::min();
        constexpr static auto max = std::numeric_limits<int64_t>::max();

        const auto target = av_rescale_q_rnd(m_seekPos, TimeBaseMs, avTb, AVRounding::AV_ROUND_DOWN);

        if(auto ret = avformat_seek_file(m_context.get(), -1, min, target, max, 0) < 0) {
            Utils::printError(ret);
        }
        avcodec_flush_buffers(m_codec.context());

        m_bufferPos = 0;
        m_buffer.clear();
        m_eof       = false;
        m_skipBytes = 0;
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
    p->m_eof        = false;
    p->m_isDecoding = false;
    p->m_draining   = false;
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
    static const QStringList extensions{QStringLiteral("mp3"),  QStringLiteral("ogg"),  QStringLiteral("opus"),
                                        QStringLiteral("oga"),  QStringLiteral("m4a"),  QStringLiteral("wav"),
                                        QStringLiteral("wv"),   QStringLiteral("flac"), QStringLiteral("wma"),
                                        QStringLiteral("mpc"),  QStringLiteral("aiff"), QStringLiteral("ape"),
                                        QStringLiteral("webm"), QStringLiteral("mp4"),  QStringLiteral("mka")};
    return extensions;
}
} // namespace Fooyin
