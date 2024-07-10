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

#include "ffmpegparser.h"

#include "engine/ffmpeg/ffmpegstream.h"
#include "engine/ffmpeg/ffmpegutils.h"

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

#include <QDebug>

namespace {
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

Fooyin::Track::Type getCodec(AVCodecID codec)
{
    using Type = Fooyin::Track::Type;

    switch(codec) {
        case(AV_CODEC_ID_MP3):
            return Type::MPEG;
        case(AV_CODEC_ID_WAVPACK):
            return Type::WavPack;
        case(AV_CODEC_ID_FLAC):
            return Type::FLAC;
        case(AV_CODEC_ID_OPUS):
            return Type::OggOpus;
        case(AV_CODEC_ID_VORBIS):
            return Type::OggVorbis;
        case(AV_CODEC_ID_WMAV2):
            return Type::ASF;
        case(AV_CODEC_ID_DTS):
            return Type::DTS;
        default:
            return Type::Unknown;
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
} // namespace

namespace Fooyin {
QStringList FFmpegParser::supportedExtensions() const
{
    // TODO: Test and add more formats
    static const QStringList extensions{QStringLiteral("mka")};

    return extensions;
}

bool FFmpegParser::canReadCover() const
{
    return true;
}

bool FFmpegParser::canWriteMetaData() const
{
    return false;
}

bool FFmpegParser::readMetaData(Track& track) const
{
    AVFormatContext* avContext{nullptr};

    const QString filepath = track.filepath();

    const int ret = avformat_open_input(&avContext, filepath.toUtf8().constData(), nullptr, nullptr);
    if(ret < 0) {
        if(ret == AVERROR(EACCES)) {
            Utils::printError(QStringLiteral("Access denied: ") + filepath);
        }
        else if(ret == AVERROR(EINVAL)) {
            Utils::printError(QStringLiteral("Invalid format: ") + filepath);
        }
        return false;
    }

    if(avformat_find_stream_info(avContext, nullptr) < 0) {
        Utils::printError(QStringLiteral("Could not find stream info"));
        avformat_close_input(&avContext);
        return false;
    }

    FormatContextPtr context;
    context.reset(avContext);

    Stream stream;
    for(unsigned int i{0}; i < context->nb_streams; ++i) {
        AVStream* avStream = context->streams[i];
        const auto type    = avStream->codecpar->codec_type;

        if(type == AVMEDIA_TYPE_AUDIO) {
            stream = Stream{avStream};
            break;
        }
    }

    if(!stream.isValid()) {
        return false;
    }

    const auto* codec = stream.avStream()->codecpar;

    track.setType(getCodec(codec->codec_id));
    const int sampleRate = codec->sample_rate;
    if(sampleRate > 0) {
        track.setSampleRate(sampleRate);
    }

#if OLD_CHANNEL_LAYOUT
    const int channels = codec->channels;
#else
    const int channels = codec->ch_layout.nb_channels;
#endif
    if(channels > 0) {
        track.setChannels(channels);
    }

    int bps = codec->bits_per_raw_sample;
    if(bps <= 0) {
        bps = codec->bits_per_coded_sample;
    }
    if(bps > 0) {
        track.setBitDepth(bps);
    }

    if(track.duration() <= 0) {
        AVRational timeBase = stream.avStream()->time_base;
        auto duration       = stream.avStream()->duration;
        if(duration <= 0 || std::cmp_equal(duration, AV_NOPTS_VALUE)) {
            duration = context->duration;
            timeBase = {1, AV_TIME_BASE};
        }
        const uint64_t durationMs = av_rescale_q(duration, timeBase, {1, 1000});
        track.setDuration(durationMs);
    }

    const auto bitrate = static_cast<int>(codec->bit_rate / 1000);
    if(bitrate > 0) {
        track.setBitrate(bitrate);
    }

    AVDictionaryEntry* tag{nullptr};
    while((tag = av_dict_get(avContext->metadata, "", tag, AV_DICT_IGNORE_SUFFIX))) {
        parseTag(track, tag);
    }

    return true;
}

QByteArray FFmpegParser::readCover(const Track& track, Track::Cover cover) const
{
    AVFormatContext* avContext{nullptr};

    const QString filepath = track.filepath();

    const int ret = avformat_open_input(&avContext, filepath.toUtf8().constData(), nullptr, nullptr);
    if(ret < 0) {
        if(ret == AVERROR(EACCES)) {
            Utils::printError(QStringLiteral("Access denied: ") + filepath);
        }
        else if(ret == AVERROR(EINVAL)) {
            Utils::printError(QStringLiteral("Invalid format: ") + filepath);
        }
        return {};
    }

    FormatContextPtr context{avContext};

    if(avformat_find_stream_info(context.get(), nullptr) < 0) {
        Utils::printError(QStringLiteral("Could not find stream info"));
        return {};
    }

    for(unsigned int i{0}; i < context->nb_streams; ++i) {
        AVStream* avStream = context->streams[i];
        if(avStream->disposition & AV_DISPOSITION_ATTACHED_PIC) {
            AVDictionaryEntry* tag = nullptr;
            QString coverType;
            while((tag = av_dict_get(avStream->metadata, "", tag, AV_DICT_IGNORE_SUFFIX))) {
                if(convertString(tag->key) == u"comment") {
                    coverType = convertString(tag->value).toLower();
                    break;
                }
            }

            if((cover == Track::Cover::Front && (coverType.isEmpty() || coverType.contains(u"front")))
               || (cover == Track::Cover::Back && coverType.contains(u"back"))
               || (cover == Track::Cover::Artist && coverType.contains(u"artist"))) {
                const AVPacket pkt = avStream->attached_pic;
                return {reinterpret_cast<const char*>(pkt.data), pkt.size};
            }
        }
    }

    return {};
}

bool FFmpegParser::writeMetaData(const Track& /*track*/, const WriteOptions& /*options*/) const
{
    return false;
}
} // namespace Fooyin
