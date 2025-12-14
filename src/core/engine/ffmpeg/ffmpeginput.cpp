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
#include "internalcoresettings.h"

#include <core/coresettings.h>
#include <core/engine/audiobuffer.h>

#include <QDebug>
#include <QFile>
#include <QIODevice>

#if defined(Q_OS_WINDOWS)
#define snprintf _snprintf
#define vsnprintf _vsnprintf
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#endif

#if defined(__GNUG__)
#pragma GCC diagnostic ignored "-Wold-style-cast"
#elif defined(__clang__)
#pragma clang diagnostic ignored "-Wold-style-cast"
#endif

using namespace Qt::StringLiterals;

constexpr AVRational TimeBaseAv = {1, AV_TIME_BASE};
constexpr AVRational TimeBaseMs = {1, 1000};

using namespace std::chrono_literals;

namespace {
QStringList fileExtensions(bool allSupported)
{
    QStringList extensions{u"mp3"_s,  u"ogg"_s, u"opus"_s, u"oga"_s, u"m4a"_s,  u"wav"_s, u"wv"_s,
                           u"flac"_s, u"wma"_s, u"asf"_s,  u"mpc"_s, u"aiff"_s, u"ape"_s, u"webm"_s,
                           u"mp4"_s,  u"mka"_s, u"dsf"_s,  u"dff"_s, u"wv"_s};

    if(!allSupported) {
        return extensions;
    }

    const AVInputFormat* format{nullptr};
    void* i{nullptr};

    while((format = av_demuxer_iterate(&i))) {
        if(format->extensions) {
            const QString exts{QString::fromLatin1(format->extensions)};
            const QStringList extList = exts.split(u',', Qt::SkipEmptyParts);
            extensions.append(extList);
        }
    }

    static constexpr std::array extensionBlacklist{
        "ans",       "apc",        "aqt",       "aqtitle", "art",      "asc",  "ass",        "bin",
        "bmp_pipe",  "bmp",        "dds_pipe",  "diz",     "dpx_pipe", "dss",  "dvbsub",     "exr_pipe",
        "exr",       "ffmetadata", "gif",       "ice",     "ico",      "ilbc", "image2pipe", "jacosub",
        "jpeg_pipe", "jpeg",       "jpg",       "mpl2",    "mpsub",    "nfo",  "pjs",        "png_pipe",
        "png",       "sami",       "smi",       "srt",     "stl",      "sub",  "sub",        "subviewer1",
        "sup",       "tif",        "tiff_pipe", "tiff",    "txt",      "vt",   "vtt",        "webvtt"};

    for(const auto* ext : extensionBlacklist) {
        extensions.removeAll(QLatin1String{ext});
    }
    extensions.removeDuplicates();

    return extensions;
}

QString getCodec(AVCodecID codec)
{
    switch(codec) {
        case(AV_CODEC_ID_AAC):
            return u"AAC"_s;
        case(AV_CODEC_ID_ALAC):
            return u"ALAC"_s;
        case(AV_CODEC_ID_MP2):
            return u"MP2"_s;
        case(AV_CODEC_ID_MP3):
            return u"MP3"_s;
        case(AV_CODEC_ID_WAVPACK):
            return u"WavPack"_s;
        case(AV_CODEC_ID_FLAC):
            return u"FLAC"_s;
        case(AV_CODEC_ID_OPUS):
            return u"Opus"_s;
        case(AV_CODEC_ID_VORBIS):
            return u"Vorbis"_s;
        case(AV_CODEC_ID_WMAV2):
            return u"WMA"_s;
        case(AV_CODEC_ID_DTS):
            return u"DTS"_s;
        default:
            return {};
    }
}

bool isLossless(AVCodecID codec)
{
    switch(codec) {
        case(AV_CODEC_ID_ALAC):
        case(AV_CODEC_ID_WAVPACK):
        case(AV_CODEC_ID_FLAC):
            return true;
        default:
            return false;
    }
}

void readTrackTotalPair(const QString& trackNumbers, Fooyin::Track& track)
{
    const qsizetype splitIdx = trackNumbers.indexOf(u'/');
    if(splitIdx >= 0) {
        track.setTrackNumber(trackNumbers.first(splitIdx));
        track.setTrackTotal(trackNumbers.sliced(splitIdx + 1));
    }
    else if(trackNumbers.size() > 0) {
        track.setTrackNumber(trackNumbers);
    }
}

void readDiscTotalPair(const QString& discNumbers, Fooyin::Track& track)
{
    const qsizetype splitIdx = discNumbers.indexOf(u'/');
    if(splitIdx >= 0) {
        track.setDiscNumber(discNumbers.first(splitIdx));
        track.setDiscTotal(discNumbers.sliced(splitIdx + 1));
    }
    else if(discNumbers.size() > 0) {
        track.setDiscNumber(discNumbers);
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
        track.setTrackTotal(convertString(tag->value));
    }
    else if(strcasecmp(tag->key, "disc") == 0 || strcasecmp(tag->key, "discnumber") == 0) {
        readDiscTotalPair(convertString(tag->value), track);
    }
    else if(strcasecmp(tag->key, "disctotal") == 0 || strcasecmp(tag->key, "totaldiscs") == 0) {
        track.setDiscTotal(convertString(tag->value));
    }
    else if(strcasecmp(tag->key, "date") == 0 || strcasecmp(tag->key, "TDRC") == 0
            || strcasecmp(tag->key, "TDRL") == 0) {
        track.setDate(convertString(tag->value));
    }
    else if(strcasecmp(tag->key, "year") == 0) {
        track.setYear(convertString(tag->value).toInt());
    }
    else if(strcasecmp(tag->key, "composer") == 0) {
        track.setComposers({convertString(tag->value)});
    }
    else if(strcasecmp(tag->key, "performer") == 0) {
        track.setPerformers({convertString(tag->value)});
    }
    else if(strcasecmp(tag->key, "comment") == 0) {
        track.setComment(convertString(tag->value));
    }
    else if(strcasecmp(tag->key, "FMPS_Rating") == 0) {
        track.setRating(convertString(tag->value).toFloat());
    }
    else if(strcasecmp(tag->key, "TFLT") == 0) {
        track.addExtraTag(u"FILETYPE"_s, convertString(tag->value));
    }
    else if(strcasecmp(tag->key, "TOLY") == 0) {
        track.addExtraTag(u"ORIGINALLYRICIST"_s, convertString(tag->value));
    }
    else if(strcasecmp(tag->key, "TLEN") == 0) {
        track.addExtraTag(u"LENGTH"_s, convertString(tag->value));
        track.setDuration(convertString(tag->value).toULongLong());
    }
    else if(strcasecmp(tag->key, "TGID") == 0) {
        track.addExtraTag(u"PODCASTDID"_s, convertString(tag->value));
    }
    else if(strcasecmp(tag->key, "TDES") == 0) {
        track.addExtraTag(u"PODCASTDESC"_s, convertString(tag->value));
    }
    else if(strcasecmp(tag->key, "TCAT") == 0) {
        track.addExtraTag(u"PODCASTCATEGORY"_s, convertString(tag->value));
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

int ffRead(void* data, uint8_t* buffer, int size)
{
    auto* device        = static_cast<QIODevice*>(data);
    const auto sizeRead = device->read(reinterpret_cast<char*>(buffer), size);
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

FormatContext createAVFormatContext(const Fooyin::AudioSource& source)
{
    FormatContext fc;

    fc.ioContext.reset(avio_alloc_context(nullptr, 0, 0, source.device, ffRead, nullptr, ffSeek));
    if(!fc.ioContext) {
        qCWarning(FFMPEG) << "Failed to allocate AVIO context";
        return {};
    }

    AVFormatContext* avContext = avformat_alloc_context();
    if(!avContext) {
        qCWarning(FFMPEG) << "Unable to allocate AVFormat context";
        return {};
    }

    avContext->pb = fc.ioContext.get();

    QByteArray filepathData;
    const char* filepath{nullptr};
    if(!source.filepath.isEmpty()) {
        filepathData = source.filepath.toLocal8Bit();
        filepath     = filepathData.constData();
    }

    const int ret = avformat_open_input(&avContext, filepath, nullptr, nullptr);
    if(ret < 0) {
        // Format context is freed on failure
        char err[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, err, AV_ERROR_MAX_STRING_SIZE);
        qCWarning(FFMPEG) << "Error opening input:" << err;
        return {};
    }

    fc.formatContext.reset(avContext);

    if(avformat_find_stream_info(avContext, nullptr) < 0) {
        qCWarning(FFMPEG) << "Could not find stream info";
        return {};
    }

    // Needed for seeking of APE files
    fc.formatContext->flags |= AVFMT_FLAG_GENPTS;

    // av_dump_format(avContext, 0, source.toUtf8().constData(), 0);

    return fc;
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
                if(convertString(tag->key) == "comment"_L1) {
                    coverType = convertString(tag->value).toLower();
                    break;
                }
            }

            if((type == Cover::Front && (coverType.isEmpty() || coverType.contains("front"_L1)))
               || (type == Cover::Back && coverType.contains("back"_L1))
               || (type == Cover::Artist && coverType.contains("artist"_L1))) {
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
    bool setup(const AudioSource& source, uint64_t duration, unsigned int subsong);
    void checkIsVbr(const Track& track);

    bool createCodec(AVStream* avStream);

    void decodeAudio(const PacketPtr& packet);
    [[nodiscard]] int sendAVPacket(const PacketPtr& packet) const;
    int receiveAVFrames();

    void readNext();
    void seek(uint64_t pos);

    FFmpegDecoder* m_self;

    IOContextPtr m_ioContext;
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
    bool m_isVbr{false};
    bool m_returnFrame{false};

    AudioDecoder::DecoderOptions m_options;
    AudioBuffer m_buffer;
    Frame m_frame;
    int m_bufferPos{0};
    int64_t m_seekPos{0};
    uint64_t m_currentPos{0};
    int m_bitrate{0};
    int m_skipBytes{0};
    uint64_t m_startTimeMs;
    uint64_t m_endTimeMs;
};

void FFmpegInputPrivate::reset()
{
    m_error      = false;
    m_eof        = false;
    m_isDecoding = false;
    m_draining   = false;
    m_isVbr      = false;
    m_bitrate    = 0;
    m_bufferPos  = 0;
    m_currentPos = 0;
    m_skipBytes  = 0;
    m_buffer.clear();

    m_context.reset();
    m_ioContext.reset();
    m_stream = {};
    m_codec  = {};
    m_buffer = {};
}

bool FFmpegInputPrivate::setup(const AudioSource& source, uint64_t duration, unsigned int subsong)
{
    reset();

    FormatContext context = createAVFormatContext(source);
    m_context             = std::move(context.formatContext);
    m_ioContext           = std::move(context.ioContext);

    if(!m_context) {
        return false;
    }

    m_isSeekable = !(m_context->ctx_flags & AVFMTCTX_UNSEEKABLE);

    m_stream = Utils::findAudioStream(m_context.get());
    if(!m_stream.isValid()) {
        return false;
    }

    m_timeBase = m_stream.avStream()->time_base;

    if(subsong < m_context->nb_chapters) {
        AVChapter* chapter  = m_context->chapters[subsong];
        AVRational timeBase = chapter->time_base;
        m_startTimeMs       = av_rescale_q(chapter->start, timeBase, TimeBaseMs);
        m_endTimeMs         = av_rescale_q(chapter->end, timeBase, TimeBaseMs);
    }
    else {
        m_startTimeMs = 0;
        m_endTimeMs   = duration;
    }

    if(createCodec(m_stream.avStream())) {
        m_audioFormat = Utils::audioFormatFromCodec(m_stream.avStream()->codecpar, m_codec.context()->sample_fmt);
        if(subsong < m_context->nb_chapters) {
            seek(0);
        }
        return true;
    }

    return false;
}

void FFmpegInputPrivate::checkIsVbr(const Track& track)
{
    const auto codec = m_codec.context()->codec_id;
    m_isVbr          = track.codecProfile().contains("VBR"_L1) || track.codecProfile().contains("ABR"_L1)
           || codec == AV_CODEC_ID_OPUS || codec == AV_CODEC_ID_VORBIS;
}

bool FFmpegInputPrivate::createCodec(AVStream* avStream)
{
    if(!avStream) {
        return false;
    }

    const AVCodec* avCodec = avcodec_find_decoder(avStream->codecpar->codec_id);
    if(!avCodec) {
        Utils::printError(u"Could not find a decoder for stream"_s);
        m_error = true;
        return false;
    }

    CodecContextPtr avCodecContext{avcodec_alloc_context3(avCodec)};
    if(!avCodecContext) {
        Utils::printError(u"Could not allocate context"_s);
        m_error = true;
        return false;
    }

    if(avCodecContext->codec_type != AVMEDIA_TYPE_AUDIO) {
        return false;
    }

    if(avcodec_parameters_to_context(avCodecContext.get(), avStream->codecpar) < 0) {
        Utils::printError(u"Could not obtain codec parameters"_s);
        m_error = true;
        return {};
    }

    avCodecContext.get()->pkt_timebase = m_timeBase;

    if(avcodec_open2(avCodecContext.get(), avCodec, nullptr) < 0) {
        Utils::printError(u"Could not initialise codec context"_s);
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
            Utils::printError(u"Unexpected decoder behavior"_s);
        }
    }

    if(result == 0) {
        receiveAVFrames();
    }
    else if(result < 0) {
        m_frame = {};
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

    if(!m_codec.isValid()) {
        return -1;
    }

    m_frame          = Frame{m_timeBase};
    const int result = avcodec_receive_frame(m_codec.context(), m_frame.avFrame());

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

    if(!m_returnFrame) {
        auto sampleCount             = m_audioFormat.bytesPerFrame() * m_frame.sampleCount();
        const uint64_t startTime     = m_codec.context()->codec_id == AV_CODEC_ID_APE ? m_currentPos : m_frame.ptsMs();
        const uint64_t frameDuration = m_frame.sampleCount() * 1000 / m_audioFormat.sampleRate();
        if(frameDuration > m_endTimeMs - startTime) {
            const auto newSampleCount = (m_endTimeMs - startTime) * m_audioFormat.sampleRate() / 1000;
            sampleCount               = m_audioFormat.bytesPerFrame() * newSampleCount;
            m_eof                     = true;
        }

        if(m_codec.isPlanar()) {
            m_buffer = {m_audioFormat, startTime - m_startTimeMs};
            m_buffer.resize(static_cast<size_t>(sampleCount));
            interleave(m_frame.avFrame()->data, m_buffer);
        }
        else {
            m_buffer = {m_frame.avFrame()->data[0], static_cast<size_t>(sampleCount), m_audioFormat,
                        startTime - m_startTimeMs};
        }

        if(!(m_options & AudioDecoder::NoSeeking)) {
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
        }

        m_currentPos += m_audioFormat.durationForBytes(m_buffer.byteCount());
    }

    return result;
}

void FFmpegInputPrivate::readNext()
{
    if(!m_isDecoding) {
        return;
    }

    if(m_currentPos >= m_endTimeMs) {
        m_eof = true;
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

    if(m_isVbr && packet && packet->duration > 0) {
        const auto durSecs = static_cast<double>(av_rescale_q_rnd(packet->duration, m_timeBase, TimeBaseAv,
                                                                  AVRounding::AV_ROUND_NEAR_INF))
                           / static_cast<double>(AV_TIME_BASE);
        if(durSecs > 0) {
            const int bitrate = static_cast<int>((packet->size * 8) / durSecs);
            if(bitrate > 0) {
                m_bitrate = bitrate / 1000;
            }
        }
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

    pos += m_startTimeMs;

    constexpr static auto min = std::numeric_limits<int64_t>::min();
    constexpr static auto max = std::numeric_limits<int64_t>::max();
    m_seekPos = static_cast<int64_t>(pos);

    const auto target = av_rescale_q_rnd(m_seekPos, TimeBaseMs, TimeBaseAv, AVRounding::AV_ROUND_DOWN);

    if(auto ret = avformat_seek_file(m_context.get(), -1, min, target, max, 0) < 0) {
        Utils::printError(ret);
    }
    avcodec_flush_buffers(m_codec.context());

    m_bufferPos  = 0;
    m_buffer     = {};
    m_eof        = false;
    m_draining   = false;
    m_skipBytes  = 0;
    m_currentPos = pos;
}

FFmpegDecoder::FFmpegDecoder()
    : p{std::make_unique<FFmpegInputPrivate>(this)}
{ }

FFmpegDecoder::~FFmpegDecoder() = default;

QStringList FFmpegDecoder::extensions() const
{
    const FySettings settings;
    return fileExtensions(settings.value(Settings::Core::Internal::FFmpegAllExtensions).toBool());
}

int FFmpegDecoder::bitrate() const
{
    return p->m_bitrate;
}

std::optional<AudioFormat> FFmpegDecoder::init(const AudioSource& source, const Track& track, DecoderOptions options)
{
    p->m_options = options;

    if(p->setup(source, track.duration(), track.subsong())) {
        p->checkIsVbr(track);
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

Frame FFmpegDecoder::readFrame()
{
    if(!p->m_isDecoding || p->m_error || !p->m_context) {
        return {};
    }

    p->m_returnFrame = true;

    if(!p->m_eof && !p->m_error) {
        p->readNext();
        return p->m_frame;
    }

    return {};
}

AudioBuffer FFmpegDecoder::readBuffer(size_t bytes)
{
    if(!p->m_isDecoding || p->m_error || !p->m_context) {
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
    const FySettings settings;
    return fileExtensions(settings.value(Settings::Core::Internal::FFmpegAllExtensions).toBool());
}

bool FFmpegReader::canReadCover() const
{
    return true;
}

bool FFmpegReader::canWriteMetaData() const
{
    return false;
}

int FFmpegReader::subsongCount() const
{
    return m_subsongCount;
}

bool FFmpegReader::init(const AudioSource& source)
{
    const FormatContext context = createAVFormatContext(source);
    if(!context.formatContext) {
        return false;
    }

    const Stream stream = Utils::findAudioStream(context.formatContext.get());
    if(!stream.isValid()) {
        return false;
    }

    const auto* avStream = stream.avStream();

    const AVCodec* avCodec = avcodec_find_decoder(avStream->codecpar->codec_id);
    if(!avCodec) {
        Utils::printError(u"Could not find a decoder for stream"_s);
        return false;
    }

    CodecContextPtr avCodecContext{avcodec_alloc_context3(avCodec)};
    if(!avCodecContext) {
        Utils::printError(u"Could not allocate context"_s);
        return false;
    }

    if(avCodecContext->codec_type != AVMEDIA_TYPE_AUDIO) {
        return false;
    }

    m_subsongCount = 1;
    if(context.formatContext->nb_chapters > 0) {
        m_subsongCount = context.formatContext->nb_chapters;
    }

    return true;
}

bool FFmpegReader::readTrack(const AudioSource& source, Track& track)
{
    const FormatContext context = createAVFormatContext(source);
    if(!context.formatContext) {
        return false;
    }

    const Stream stream = Utils::findAudioStream(context.formatContext.get());
    if(!stream.isValid()) {
        return false;
    }

    const auto* avStream = stream.avStream();
    const auto* codecPar = stream.avStream()->codecpar;

    const AVCodec* avCodec = avcodec_find_decoder(avStream->codecpar->codec_id);
    if(!avCodec) {
        Utils::printError(u"Could not find a decoder for stream"_s);
        return false;
    }

    CodecContextPtr avCodecContext{avcodec_alloc_context3(avCodec)};
    if(!avCodecContext) {
        Utils::printError(u"Could not allocate context"_s);
        return false;
    }

    if(avCodecContext->codec_type != AVMEDIA_TYPE_AUDIO) {
        return false;
    }

    if(avcodec_parameters_to_context(avCodecContext.get(), avStream->codecpar) < 0) {
        Utils::printError(u"Could not obtain codec parameters"_s);
        return {};
    }

    if(avcodec_open2(avCodecContext.get(), avCodec, nullptr) < 0) {
        Utils::printError(u"Could not initialise codec context"_s);
        return {};
    }

    const auto format = Utils::audioFormatFromCodec(stream.avStream()->codecpar, avCodecContext->sample_fmt);

    track.setCodec(getCodec(codecPar->codec_id));
    track.setSampleRate(format.sampleRate());
    track.setChannels(format.channelCount());
    track.setBitDepth(format.bitsPerSample());
    track.setEncoding(isLossless(codecPar->codec_id) ? u"Lossless"_s : u"Lossy"_s);

    int _subsong     = track.subsong();
    int _nb_chapters = (int)context.formatContext->nb_chapters;

    if(track.duration() == 0) {
        uint64_t durationMs;
        if(_subsong < _nb_chapters) {
            AVChapter* chapter         = context.formatContext->chapters[_subsong];
            AVRational timeBase        = chapter->time_base;
            const uint64_t startTimeMs = av_rescale_q(chapter->start, timeBase, TimeBaseMs);
            const uint64_t endTimeMs   = av_rescale_q(chapter->end, timeBase, TimeBaseMs);
            durationMs                 = endTimeMs - startTimeMs;
        }
        else {
            AVRational timeBase = avStream->time_base;
            auto duration       = avStream->duration;
            if(duration <= 0 || std::cmp_equal(duration, AV_NOPTS_VALUE)) {
                duration = context.formatContext->duration;
                timeBase = TimeBaseAv;
            }
            durationMs = av_rescale_q(duration, timeBase, TimeBaseMs);
        }
        track.setDuration(durationMs);
    }

    auto bitrate = static_cast<int>(codecPar->bit_rate / 1000);
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
    const FormatContext context = createAVFormatContext(source);
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
