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

#include "ffmpeginput.h"

#include "ffmpegcodec.h"
#include "ffmpegframe.h"
#include "ffmpegstream.h"
#include "ffmpegutils.h"

#include <core/engine/audiobuffer.h>
#include <utils/worker.h>

#include <QDebug>
#include <QFile>
#include <QIODevice>

#if defined(__GNUG__)
#pragma GCC diagnostic ignored "-Wold-style-cast"
#elif defined(__clang__)
#pragma clang diagnostic ignored "-Wold-style-cast"
#endif

constexpr AVRational TimeBaseAv = {1, AV_TIME_BASE};
constexpr AVRational TimeBaseMs = {1, 1000};

using namespace std::chrono_literals;

namespace {
QStringList fileExtensions()
{
    static const QStringList extensions{QStringLiteral("mp3"),  QStringLiteral("ogg"),  QStringLiteral("opus"),
                                        QStringLiteral("oga"),  QStringLiteral("m4a"),  QStringLiteral("wav"),
                                        QStringLiteral("wv"),   QStringLiteral("flac"), QStringLiteral("wma"),
                                        QStringLiteral("mpc"),  QStringLiteral("aiff"), QStringLiteral("ape"),
                                        QStringLiteral("webm"), QStringLiteral("mp4"),  QStringLiteral("mka")};
    return extensions;
}

QString getCodec(AVCodecID codec)
{
    switch(codec) {
        case(AV_CODEC_ID_AAC):
            return QStringLiteral("AAC");
        case(AV_CODEC_ID_ALAC):
            return QStringLiteral("ALAC");
        case(AV_CODEC_ID_MP3):
            return QStringLiteral("MP3");
        case(AV_CODEC_ID_WAVPACK):
            return QStringLiteral("WavPack");
        case(AV_CODEC_ID_FLAC):
            return QStringLiteral("FLAC");
        case(AV_CODEC_ID_OPUS):
            return QStringLiteral("Opus");
        case(AV_CODEC_ID_VORBIS):
            return QStringLiteral("Vorbis");
        case(AV_CODEC_ID_WMAV2):
            return QStringLiteral("WMA");
        case(AV_CODEC_ID_DTS):
            return QStringLiteral("DTS");
        default:
            return {};
    }
}

void readTrackTotalPair(const QString& trackNumbers, Fooyin::Track& track)
{
    const qsizetype splitIdx = trackNumbers.indexOf(QStringLiteral("/"));
    if(splitIdx >= 0) {
        track.setTrackNumber(trackNumbers.first(splitIdx).toInt());
        track.setTrackTotal(trackNumbers.sliced(splitIdx + 1).toInt());
    }
    else if(trackNumbers.size() > 0) {
        track.setTrackNumber(trackNumbers.toInt());
    }
}

void readDiscTotalPair(const QString& discNumbers, Fooyin::Track& track)
{
    const qsizetype splitIdx = discNumbers.indexOf(QStringLiteral("/"));
    if(splitIdx >= 0) {
        track.setDiscNumber(discNumbers.first(splitIdx).toInt());
        track.setDiscTotal(discNumbers.sliced(splitIdx + 1).toInt());
    }
    else if(discNumbers.size() > 0) {
        track.setDiscNumber(discNumbers.toInt());
    }
}

QString convertString(const char* str)
{
    return QString::fromUtf8(str);
};

void parseTag(Fooyin::Track& track, AVDictionaryEntry* tag)
{
    if(strcasecmp(tag->key, "album") == 0) {
        track.setAlbum(convertString(tag->value));
    }
    else if(strcasecmp(tag->key, "artist") == 0) {
        track.setArtists({convertString(tag->value)});
    }
    else if(strcasecmp(tag->key, "album_artist") == 0 || strcasecmp(tag->key, "album artist") == 0) {
        track.setAlbumArtists({convertString(tag->value)});
    }
    else if(strcasecmp(tag->key, "title") == 0) {
        track.setTitle(convertString(tag->value));
    }
    else if(strcasecmp(tag->key, "genre") == 0) {
        track.setGenres({convertString(tag->value)});
    }
    else if(strcasecmp(tag->key, "part_number") == 0 || strcasecmp(tag->key, "track") == 0) {
        readTrackTotalPair(convertString(tag->value), track);
    }
    else if(strcasecmp(tag->key, "tracktotal") == 0 || strcasecmp(tag->key, "totaltracks") == 0) {
        track.setTrackTotal(convertString(tag->value).toInt());
    }
    else if(strcasecmp(tag->key, "disc") == 0 || strcasecmp(tag->key, "discnumber") == 0) {
        readDiscTotalPair(convertString(tag->value), track);
    }
    else if(strcasecmp(tag->key, "disctotal") == 0 || strcasecmp(tag->key, "totaldiscs") == 0) {
        track.setDiscTotal(convertString(tag->value).toInt());
    }
    else if(strcasecmp(tag->key, "date") == 0 || strcasecmp(tag->key, "TDRC") == 0
            || strcasecmp(tag->key, "TDRL") == 0) {
        track.setDate(convertString(tag->value));
    }
    else if(strcasecmp(tag->key, "year") == 0) {
        track.setYear(convertString(tag->value).toInt());
    }
    else if(strcasecmp(tag->key, "composer") == 0) {
        track.setComposer(convertString(tag->value));
    }
    else if(strcasecmp(tag->key, "performer") == 0) {
        track.setPerformer(convertString(tag->value));
    }
    else if(strcasecmp(tag->key, "comment") == 0) {
        track.setComment(convertString(tag->value));
    }
    else if(strcasecmp(tag->key, "FMPS_Rating") == 0) {
        track.setRating(convertString(tag->value).toFloat());
    }
    else if(strcasecmp(tag->key, "TFLT") == 0) {
        track.addExtraTag(QStringLiteral("FILETYPE"), convertString(tag->value));
    }
    else if(strcasecmp(tag->key, "TOLY") == 0) {
        track.addExtraTag(QStringLiteral("ORIGINALLYRICIST"), convertString(tag->value));
    }
    else if(strcasecmp(tag->key, "TLEN") == 0) {
        track.addExtraTag(QStringLiteral("LENGTH"), convertString(tag->value));
        track.setDuration(convertString(tag->value).toULongLong());
    }
    else if(strcasecmp(tag->key, "TGID") == 0) {
        track.addExtraTag(QStringLiteral("PODCASTDID"), convertString(tag->value));
    }
    else if(strcasecmp(tag->key, "TDES") == 0) {
        track.addExtraTag(QStringLiteral("PODCASTDESC"), convertString(tag->value));
    }
    else if(strcasecmp(tag->key, "TCAT") == 0) {
        track.addExtraTag(QStringLiteral("PODCASTCATEGORY"), convertString(tag->value));
    }
    else if(strncasecmp(tag->key, "ID3V2_PRIV", 10) == 0) { }
    else {
        track.addExtraTag(convertString(tag->key).toUpper(), convertString(tag->value));
    }
};

void interleaveSamples(uint8_t** in, Fooyin::AudioBuffer& buffer)
{
    const auto format  = buffer.format();
    const int channels = format.channelCount();
    const int samples  = buffer.frameCount();
    const int bps      = format.bytesPerSample();
    auto* out          = buffer.data();

    const int totalSamples = samples * channels;

    for(int i{0}; i < totalSamples; ++i) {
        const int sampleIndex  = i / channels;
        const int channelIndex = i % channels;

        const auto inOffset  = sampleIndex * bps;
        const auto outOffset = i * bps;
        std::memmove(out + outOffset, in[channelIndex] + inOffset, bps);
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

struct AVIOContextDeleter
{
    void operator()(AVIOContext* context) const
    {
        if(context) {
            if(context->buffer) {
                av_freep(static_cast<void*>(&context->buffer));
            }
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57, 80, 100)
            avio_context_free(&context);
#else
            av_free(context);
#endif
        }
    }
};

using AVIOContextPtr = std::unique_ptr<AVIOContext, AVIOContextDeleter>;

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

int ffRead(void* data, uint8_t* buffer, int size)
{
    auto* device        = static_cast<QIODevice*>(data);
    const auto sizeRead = device->read(std::bit_cast<char*>(buffer), size);
    if(sizeRead == 0) {
        return AVERROR_EOF;
    }
    return static_cast<int>(sizeRead);
}

int64_t ffSeek(void* data, int64_t offset, int whence)
{
    auto* device = static_cast<QIODevice*>(data);
    int64_t seekPos{0};

    switch(whence) {
        case(AVSEEK_SIZE):
            return device->size();
        case(SEEK_SET):
            seekPos = offset;
            break;
        case(SEEK_CUR):
            seekPos = device->pos() + offset;
            break;
        case(SEEK_END):
            seekPos = device->size() - offset;
            break;
        default:
            return -1;
    }

    if(seekPos < 0 || seekPos > device->size()) {
        return -1;
    }

    return device->seek(seekPos);
}

struct FormatContext
{
    FormatContextPtr formatContext;
    AVIOContextPtr ioContext;
};

FormatContext createAVFormatContext(QIODevice* source)
{
    FormatContext fc;

    fc.ioContext.reset(avio_alloc_context(nullptr, 0, 0, source, ffRead, nullptr, ffSeek));
    if(!fc.ioContext) {
        qCWarning(FFMPEG) << "Failed to allocate AVIO context";
        return {};
    }

    AVFormatContext* avContext = avformat_alloc_context();
    if(!avContext) {
        qCWarning(FFMPEG) << "Unable to allocate AVFormat context";
        return {};
    }
    fc.formatContext.reset(avContext);
    auto* context = fc.formatContext.get();

    fc.formatContext->pb = fc.ioContext.get();

    const int ret = avformat_open_input(&context, "", nullptr, nullptr);
    if(ret < 0) {
        if(ret == AVERROR(EACCES)) {
            qCWarning(FFMPEG) << "Access denied";
        }
        else if(ret == AVERROR(EINVAL)) {
            qCWarning(FFMPEG) << "Invalid format";
        }
        return {};
    }

    if(avformat_find_stream_info(avContext, nullptr) < 0) {
        qCWarning(FFMPEG) << "Could not find stream info";
        return {};
    }

    // Needed for seeking of APE files
    fc.formatContext->flags |= AVFMT_FLAG_GENPTS;

    // av_dump_format(avContext, 0, source.toUtf8().constData(), 0);

    return fc;
}

Fooyin::Stream findStream(AVFormatContext* context)
{
    const auto count = static_cast<int>(context->nb_streams);

    for(int i{0}; i < count; ++i) {
        AVStream* avStream = context->streams[i];
        const auto type    = avStream->codecpar->codec_type;

        if(type == AVMEDIA_TYPE_AUDIO) {
            return Fooyin::Stream{avStream};
        }
    }

    return {};
}

QByteArray findCover(AVFormatContext* context, Fooyin::Track::Cover type)
{
    using Cover = Fooyin::Track::Cover;

    const auto count = static_cast<int>(context->nb_streams);

    for(int i{0}; i < count; ++i) {
        AVStream* avStream = context->streams[i];

        if(avStream->disposition & AV_DISPOSITION_ATTACHED_PIC) {
            AVDictionaryEntry* tag{nullptr};
            QString coverType;
            while((tag = av_dict_get(avStream->metadata, "", tag, AV_DICT_IGNORE_SUFFIX))) {
                if(convertString(tag->key) == u"comment") {
                    coverType = convertString(tag->value).toLower();
                    break;
                }
            }

            if((type == Cover::Front && (coverType.isEmpty() || coverType.contains(u"front")))
               || (type == Cover::Back && coverType.contains(u"back"))
               || (type == Cover::Artist && coverType.contains(u"artist"))) {
                const AVPacket pkt = avStream->attached_pic;
                return {reinterpret_cast<const char*>(pkt.data), pkt.size};
            }
        }
    }

    return {};
}
} // namespace

namespace Fooyin {
class FFmpegInputPrivate
{
public:
    explicit FFmpegInputPrivate(FFmpegDecoder* self)
        : m_self{self}
    { }

    void reset();
    bool setup(QIODevice* source);

    bool createCodec(AVStream* avStream);

    void decodeAudio(const PacketPtr& packet);
    [[nodiscard]] int sendAVPacket(const PacketPtr& packet) const;
    int receiveAVFrames();

    void readNext();
    void seek(uint64_t pos);

    FFmpegDecoder* m_self;

    AVIOContextPtr m_ioContext;
    FormatContextPtr m_context;
    Stream m_stream;
    Codec m_codec;
    AudioFormat m_audioFormat;

    AVRational m_timeBase{0, 0};
    bool m_isSeekable{false};
    bool m_draining{false};
    bool m_error{false};
    bool m_eof{false};
    bool m_isDecoding{false};

    AudioBuffer m_buffer;
    int m_bufferPos{0};
    int64_t m_seekPos{0};
    int m_skipBytes{0};
};

void FFmpegInputPrivate::reset()
{
    m_error      = false;
    m_eof        = false;
    m_isDecoding = false;
    m_draining   = false;
    m_bufferPos  = 0;
    m_buffer.clear();

    m_context.reset();
    m_ioContext.reset();
    m_stream = {};
    m_codec  = {};
    m_buffer = {};
}

bool FFmpegInputPrivate::setup(QIODevice* source)
{
    reset();

    FormatContext context = createAVFormatContext(source);
    m_context             = std::move(context.formatContext);
    m_ioContext           = std::move(context.ioContext);

    if(!m_context) {
        return false;
    }

    m_isSeekable = !(m_context->ctx_flags & AVFMTCTX_UNSEEKABLE);

    m_stream = findStream(m_context.get());
    if(!m_stream.isValid()) {
        return false;
    }

    m_timeBase    = m_stream.avStream()->time_base;
    m_audioFormat = Utils::audioFormatFromCodec(m_stream.avStream()->codecpar);

    return createCodec(m_stream.avStream());
}

bool FFmpegInputPrivate::createCodec(AVStream* avStream)
{
    if(!avStream) {
        return false;
    }

    const AVCodec* avCodec = avcodec_find_decoder(avStream->codecpar->codec_id);
    if(!avCodec) {
        Utils::printError(QStringLiteral("Could not find a decoder for stream"));
        m_error = true;
        return false;
    }

    CodecContextPtr avCodecContext{avcodec_alloc_context3(avCodec)};
    if(!avCodecContext) {
        Utils::printError(QStringLiteral("Could not allocate context"));
        m_error = true;
        return false;
    }

    if(avCodecContext->codec_type != AVMEDIA_TYPE_AUDIO) {
        return false;
    }

    if(avcodec_parameters_to_context(avCodecContext.get(), avStream->codecpar) < 0) {
        Utils::printError(QStringLiteral("Could not obtain codec parameters"));
        m_error = true;
        return {};
    }

    avCodecContext.get()->pkt_timebase = m_timeBase;

    if(avcodec_open2(avCodecContext.get(), avCodec, nullptr) < 0) {
        Utils::printError(QStringLiteral("Could not initialise codec context"));
        m_error = true;
        return {};
    }

    m_codec = {std::move(avCodecContext), avStream};

    return true;
}

void FFmpegInputPrivate::decodeAudio(const PacketPtr& packet)
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

int FFmpegInputPrivate::sendAVPacket(const PacketPtr& packet) const
{
    if(m_error || !m_isDecoding) {
        return -1;
    }

    if(!packet || m_draining) {
        return avcodec_send_packet(m_codec.context(), nullptr);
    }

    return avcodec_send_packet(m_codec.context(), packet.get());
}

int FFmpegInputPrivate::receiveAVFrames()
{
    if(m_error || !m_isDecoding) {
        return -1;
    }

    const Frame frame{m_timeBase};
    const int result = avcodec_receive_frame(m_codec.context(), frame.avFrame());

    if(result == AVERROR_EOF) {
        m_draining = true;
        decodeAudio({});
        return result;
    }

    if(result == AVERROR(EAGAIN)) {
        return result;
    }

    if(result < 0) {
        Utils::printError(result);
        m_error = true;
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
            m_buffer.clear();
        }
        else {
            std::memmove(m_buffer.data(), m_buffer.data() + len, sampleCount - len);
            m_buffer.resize(sampleCount - len);
        }
    }

    return result;
}

void FFmpegInputPrivate::readNext()
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
}

void FFmpegInputPrivate::seek(uint64_t pos)
{
    if(!m_context || !m_isSeekable || m_error) {
        return;
    }

    m_seekPos = static_cast<int64_t>(pos);

    constexpr static auto min = std::numeric_limits<int64_t>::min();
    constexpr static auto max = std::numeric_limits<int64_t>::max();

    const auto target = av_rescale_q_rnd(m_seekPos, TimeBaseMs, TimeBaseAv, AVRounding::AV_ROUND_DOWN);

    if(auto ret = avformat_seek_file(m_context.get(), -1, min, target, max, 0) < 0) {
        Utils::printError(ret);
    }
    avcodec_flush_buffers(m_codec.context());

    m_bufferPos = 0;
    m_buffer    = {};
    m_eof       = false;
    m_skipBytes = 0;
}

FFmpegDecoder::FFmpegDecoder()
    : p{std::make_unique<FFmpegInputPrivate>(this)}
{ }

FFmpegDecoder::~FFmpegDecoder() = default;

QStringList FFmpegDecoder::extensions() const
{
    return fileExtensions();
}

std::optional<AudioFormat> FFmpegDecoder::init(const AudioSource& source, const Track& /*track*/,
                                               DecoderOptions /*options*/)
{
    if(p->setup(source.device)) {
        return p->m_audioFormat;
    }
    p->m_error = true;
    return {};
}

void FFmpegDecoder::start()
{
    p->m_isDecoding = true;
}

void FFmpegDecoder::stop()
{
    p->reset();
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
    if(!p->m_isDecoding || p->m_error) {
        return {};
    }

    while(!p->m_buffer.isValid() && !p->m_eof && !p->m_error) {
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

QStringList FFmpegReader::extensions() const
{
    return fileExtensions();
}

bool FFmpegReader::canReadCover() const
{
    return true;
}

bool FFmpegReader::canWriteMetaData() const
{
    return false;
}

bool FFmpegReader::readTrack(const AudioSource& source, Track& track)
{
    const FormatContext context = createAVFormatContext(source.device);
    if(!context.formatContext) {
        return false;
    }

    const Stream stream = findStream(context.formatContext.get());
    if(!stream.isValid()) {
        return false;
    }

    const auto* avStream = stream.avStream();
    const auto* codec    = stream.avStream()->codecpar;
    const auto format    = Utils::audioFormatFromCodec(stream.avStream()->codecpar);

    track.setCodec(getCodec(codec->codec_id));
    track.setSampleRate(format.sampleRate());
    track.setChannels(format.channelCount());
    track.setBitDepth(format.bitsPerSample());

    if(track.duration() == 0) {
        AVRational timeBase = avStream->time_base;
        auto duration       = avStream->duration;
        if(duration <= 0 || std::cmp_equal(duration, AV_NOPTS_VALUE)) {
            duration = context.formatContext->duration;
            timeBase = TimeBaseAv;
        }
        const uint64_t durationMs = av_rescale_q(duration, timeBase, TimeBaseMs);
        track.setDuration(durationMs);
    }

    auto bitrate = static_cast<int>(codec->bit_rate / 1000);
    if(bitrate <= 0) {
        bitrate = static_cast<int>(context.formatContext->bit_rate / 1000);
    }
    if(bitrate > 0) {
        track.setBitrate(bitrate);
    }

    AVDictionaryEntry* tag{nullptr};
    while((tag = av_dict_get(context.formatContext->metadata, "", tag, AV_DICT_IGNORE_SUFFIX))) {
        parseTag(track, tag);
    }

    return true;
}

QByteArray FFmpegReader::readCover(const AudioSource& source, const Track& /*track*/, Track::Cover cover)
{
    const FormatContext context = createAVFormatContext(source.device);
    if(!context.formatContext) {
        return {};
    }

    auto coverData = findCover(context.formatContext.get(), cover);

    return coverData;
}

bool FFmpegReader::writeTrack(const AudioSource& /*source*/, const Track& /*track*/, WriteOptions /*options*/)
{
    return false;
}
} // namespace Fooyin
