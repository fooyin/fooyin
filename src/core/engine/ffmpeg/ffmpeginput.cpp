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

#if defined(__GNUG__)
#pragma GCC diagnostic ignored "-Wold-style-cast"
#elif defined(__clang__)
#pragma clang diagnostic ignored "-Wold-style-cast"
#endif

constexpr AVRational TimeBaseAv = {1, AV_TIME_BASE};
constexpr AVRational TimeBaseMs = {1, 1000};

using namespace std::chrono_literals;

namespace {
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
            return QStringLiteral("Unknown");
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
class FFmpegInputPrivate
{
public:
    explicit FFmpegInputPrivate(FFmpegInput* self)
        : m_self{self}
    { }

    void reset();
    bool setup(const QString& source);
    [[nodiscard]] bool hasError() const;

    bool createAVFormatContext(const QString& source);
    bool findStream();
    QByteArray findCover(Track::Cover type);
    bool createCodec(AVStream* avStream);

    void decodeAudio(const PacketPtr& packet);
    [[nodiscard]] int sendAVPacket(const PacketPtr& packet) const;
    int receiveAVFrames();

    void readNext();
    void seek(uint64_t pos);

    FFmpegInput* m_self;

    FormatContextPtr m_context;
    Stream m_stream;
    Codec m_codec;
    AudioFormat m_audioFormat;

    AudioInput::Error m_error{AudioInput::Error::NoError};
    AVRational m_timeBase{0, 0};
    bool m_isSeekable{false};
    bool m_draining{false};
    bool m_eof{false};
    bool m_isDecoding{false};

    AudioBuffer m_buffer;
    int m_bufferPos{0};
    int64_t m_seekPos{0};
    int m_skipBytes{0};
};

void FFmpegInputPrivate::reset()
{
    m_eof        = false;
    m_isDecoding = false;
    m_draining   = false;
    m_bufferPos  = 0;
    m_buffer.clear();

    m_context.reset();
    m_stream = {};
    m_codec  = {};
    m_buffer = {};

    m_error = AudioInput::Error::NoError;
}

bool FFmpegInputPrivate::setup(const QString& source)
{
    reset();

    if(!createAVFormatContext(source)) {
        return false;
    }

    m_isSeekable = !(m_context->ctx_flags & AVFMTCTX_UNSEEKABLE);

    if(!findStream()) {
        return false;
    }

    m_audioFormat = Utils::audioFormatFromCodec(m_stream.avStream()->codecpar);

    return createCodec(m_stream.avStream());
}

bool FFmpegInputPrivate::hasError() const
{
    return m_error != AudioInput::Error::NoError;
}

bool FFmpegInputPrivate::createAVFormatContext(const QString& source)
{
    AVFormatContext* avContext{nullptr};

    const int ret = avformat_open_input(&avContext, source.toUtf8().constData(), nullptr, nullptr);
    if(ret < 0) {
        if(ret == AVERROR(EACCES)) {
            Utils::printError(QStringLiteral("Access denied: ") + source);
            m_error = AudioInput::Error::AccessDeniedError;
        }
        else if(ret == AVERROR(EINVAL)) {
            Utils::printError(QStringLiteral("Invalid format: ") + source);
            m_error = AudioInput::Error::FormatError;
        }
        return false;
    }

    if(avformat_find_stream_info(avContext, nullptr) < 0) {
        Utils::printError(QStringLiteral("Could not find stream info"));
        avformat_close_input(&avContext);
        m_error = AudioInput::Error::ResourceError;
        return false;
    }

    // Needed for seeking of APE files
    avContext->flags |= AVFMT_FLAG_GENPTS;

    // av_dump_format(avContext, 0, source.toUtf8().constData(), 0);

    m_context.reset(avContext);

    return true;
}

bool FFmpegInputPrivate::findStream()
{
    const auto count = static_cast<int>(m_context->nb_streams);

    for(int i{0}; i < count; ++i) {
        AVStream* avStream = m_context->streams[i];
        const auto type    = avStream->codecpar->codec_type;

        if(type == AVMEDIA_TYPE_AUDIO) {
            m_timeBase = avStream->time_base;
            m_stream   = Fooyin::Stream{avStream};
            return true;
        }
    }

    m_error = AudioInput::Error::ResourceError;

    return false;
}

QByteArray FFmpegInputPrivate::findCover(Track::Cover type)
{
    const auto count = static_cast<int>(m_context->nb_streams);

    for(int i{0}; i < count; ++i) {
        AVStream* avStream = m_context->streams[i];

        if(avStream->disposition & AV_DISPOSITION_ATTACHED_PIC) {
            AVDictionaryEntry* tag{nullptr};
            QString coverType;
            while((tag = av_dict_get(avStream->metadata, "", tag, AV_DICT_IGNORE_SUFFIX))) {
                if(convertString(tag->key) == u"comment") {
                    coverType = convertString(tag->value).toLower();
                    break;
                }
            }

            if((type == Track::Cover::Front && (coverType.isEmpty() || coverType.contains(u"front")))
               || (type == Track::Cover::Back && coverType.contains(u"back"))
               || (type == Track::Cover::Artist && coverType.contains(u"artist"))) {
                const AVPacket pkt = avStream->attached_pic;
                return {reinterpret_cast<const char*>(pkt.data), pkt.size};
            }
        }
    }

    return {};
}

bool FFmpegInputPrivate::createCodec(AVStream* avStream)
{
    if(!avStream) {
        return false;
    }

    const AVCodec* avCodec = avcodec_find_decoder(avStream->codecpar->codec_id);
    if(!avCodec) {
        Utils::printError(QStringLiteral("Could not find a decoder for stream"));
        m_error = AudioInput::Error::ResourceError;
        return false;
    }

    CodecContextPtr avCodecContext{avcodec_alloc_context3(avCodec)};
    if(!avCodecContext) {
        Utils::printError(QStringLiteral("Could not allocate context"));
        m_error = AudioInput::Error::ResourceError;
        return false;
    }

    if(avCodecContext->codec_type != AVMEDIA_TYPE_AUDIO) {
        return false;
    }

    if(avcodec_parameters_to_context(avCodecContext.get(), avStream->codecpar) < 0) {
        Utils::printError(QStringLiteral("Could not obtain codec parameters"));
        m_error = AudioInput::Error::ResourceError;
        return {};
    }

    avCodecContext.get()->pkt_timebase = m_timeBase;

    if(avcodec_open2(avCodecContext.get(), avCodec, nullptr) < 0) {
        Utils::printError(QStringLiteral("Could not initialise codec context"));
        m_error = AudioInput::Error::ResourceError;
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
    if(hasError() || !m_isDecoding) {
        return -1;
    }

    if(!packet || m_draining) {
        return avcodec_send_packet(m_codec.context(), nullptr);
    }

    return avcodec_send_packet(m_codec.context(), packet.get());
}

int FFmpegInputPrivate::receiveAVFrames()
{
    if(hasError() || !m_isDecoding) {
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
        m_error = AudioInput::Error::ResourceError;
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
    if(!m_context || !m_isSeekable || hasError()) {
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

FFmpegInput::FFmpegInput()
    : p{std::make_unique<FFmpegInputPrivate>(this)}
{ }

FFmpegInput::~FFmpegInput() = default;

QStringList FFmpegInput::supportedExtensions() const
{
    static const QStringList extensions{QStringLiteral("mp3"),  QStringLiteral("ogg"),  QStringLiteral("opus"),
                                        QStringLiteral("oga"),  QStringLiteral("m4a"),  QStringLiteral("wav"),
                                        QStringLiteral("wv"),   QStringLiteral("flac"), QStringLiteral("wma"),
                                        QStringLiteral("mpc"),  QStringLiteral("aiff"), QStringLiteral("ape"),
                                        QStringLiteral("webm"), QStringLiteral("mp4"),  QStringLiteral("mka")};
    return extensions;
}

bool FFmpegInput::canReadCover() const
{
    return true;
}

bool FFmpegInput::canWriteMetaData() const
{
    return false;
}

bool FFmpegInput::init(const QString& source)
{
    return p->setup(source);
}

void FFmpegInput::start()
{
    p->m_isDecoding = true;
}

void FFmpegInput::stop()
{
    p->reset();
}

AudioFormat FFmpegInput::format() const
{
    return p->m_audioFormat;
}

bool FFmpegInput::isSeekable() const
{
    return p->m_isSeekable;
}

void FFmpegInput::seek(uint64_t pos)
{
    p->seek(pos);
}

AudioBuffer FFmpegInput::readBuffer(size_t bytes)
{
    if(!p->m_isDecoding || p->hasError()) {
        return {};
    }

    while(!p->m_buffer.isValid() && !p->m_eof && !p->hasError()) {
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

bool FFmpegInput::readMetaData(Track& track)
{
    const QString source = track.filepath();

    const bool uninitialised = !p->m_context;
    if(uninitialised) {
        if(!p->createAVFormatContext(source) || !p->findStream()) {
            return false;
        }
    }

    const auto* codec = p->m_stream.avStream()->codecpar;
    const auto format = Utils::audioFormatFromCodec(p->m_stream.avStream()->codecpar);

    track.setCodec(getCodec(codec->codec_id));
    track.setSampleRate(format.sampleRate());
    track.setChannels(format.channelCount());
    track.setBitDepth(format.bytesPerSample() * 8);

    const auto* stream = p->m_stream.avStream();

    if(track.duration() == 0) {
        AVRational timeBase = stream->time_base;
        auto duration       = stream->duration;
        if(duration <= 0 || std::cmp_equal(duration, AV_NOPTS_VALUE)) {
            duration = p->m_context->duration;
            timeBase = TimeBaseAv;
        }
        const uint64_t durationMs = av_rescale_q(duration, timeBase, TimeBaseMs);
        track.setDuration(durationMs);
    }

    auto bitrate = static_cast<int>(codec->bit_rate / 1000);
    if(bitrate <= 0) {
        bitrate = static_cast<int>(p->m_context->bit_rate / 1000);
    }
    if(bitrate > 0) {
        track.setBitrate(bitrate);
    }

    AVDictionaryEntry* tag{nullptr};
    while((tag = av_dict_get(p->m_context->metadata, "", tag, AV_DICT_IGNORE_SUFFIX))) {
        parseTag(track, tag);
    }

    if(uninitialised) {
        p->reset();
    }

    return true;
}

QByteArray FFmpegInput::readCover(const Track& track, Track::Cover cover)
{
    const QString source = track.filepath();

    const bool uninitialised = !p->m_context;
    if(uninitialised) {
        if(!p->createAVFormatContext(source)) {
            return {};
        }
    }

    auto coverData = p->findCover(cover);

    if(uninitialised) {
        p->reset();
    }

    return coverData;
}

bool FFmpegInput::writeMetaData(const Track& /*track*/, const WriteOptions& /*options*/)
{
    return false;
}

AudioInput::Error FFmpegInput::error() const
{
    return p->m_error;
}
} // namespace Fooyin
