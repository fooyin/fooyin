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
#include "input/id3utils.h"
#include "input/tagpolicy.h"
#include "internalcoresettings.h"

#include <core/constants.h>
#include <core/coresettings.h>
#include <core/engine/audiobuffer.h>
#include <core/engine/tagdefs.h>

#include <QDebug>
#include <QFile>
#include <QIODevice>
#include <QtEndian>

#include <cstring>

#ifdef Q_OS_WINDOWS
#define snprintf _snprintf
#define vsnprintf _vsnprintf
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#endif

#ifdef __GNUG__
#pragma GCC diagnostic ignored "-Wold-style-cast"
#elifdef __clang__
#pragma clang diagnostic ignored "-Wold-style-cast"
#endif

using namespace Qt::StringLiterals;

constexpr AVRational TimeBaseAv           = {.num = 1, .den = AV_TIME_BASE};
constexpr AVRational TimeBaseMs           = {.num = 1, .den = 1000};
constexpr auto MaxConsecutiveDecodeErrors = 8;
constexpr auto ApeTagFooterSize           = 32;

using namespace std::chrono_literals;

namespace Fooyin {
namespace {
enum class TagType : uint8_t
{
    Unknown,
    APE,
    ID3v1_1,
    ID3v1_2,
    ID3v2_2,
    ID3v2_3,
    ID3v2_4,
};

bool isRecoverableDecodeError(int error)
{
    return error == AVERROR_INVALIDDATA;
}

QStringList fileExtensions(bool allSupported)
{
    QStringList extensions{u"mp3"_s,  u"ogg"_s,  u"opus"_s, u"oga"_s, u"m4a"_s, u"m4b"_s,  u"wav"_s,
                           u"wv"_s,   u"flac"_s, u"wma"_s,  u"asf"_s, u"mpc"_s, u"aiff"_s, u"ape"_s,
                           u"webm"_s, u"mp4"_s,  u"mka"_s,  u"dsf"_s, u"dff"_s, u"tak"_s};

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
        "ans", "apc",      "aqt",   "aqtitle",  "art",        "asc",      "ass",       "bin",       "bmp_pipe",
        "bmp", "dds_pipe", "diz",   "dpx_pipe", "dss",        "dvbsub",   "exr_pipe",  "exr",       "ffmetadata",
        "gif", "ice",      "ico",   "ilbc",     "image2pipe", "jacosub",  "jpeg_pipe", "jpeg",      "jpg",
        "jxl", "mpl2",     "mpsub", "nfo",      "pjs",        "png_pipe", "png",       "sami",      "smi",
        "srt", "stl",      "sub",   "sub",      "subviewer1", "sup",      "tif",       "tiff_pipe", "tiff",
        "txt", "vt",       "vtt",   "webvtt"};

    for(const auto* ext : extensionBlacklist) {
        extensions.removeAll(QLatin1String{ext});
    }
    extensions.removeDuplicates();

    return extensions;
}

QStringList ffmpegPreferredExtensions()
{
    const FySettings settings;
    return settings
        .value(Settings::Core::Internal::FFmpegPriorityExtensions,
               Settings::Core::Internal::defaultFFmpegPriorityExtensions())
        .toStringList();
}

QString getCodec(AVCodecID codec)
{
    switch(codec) {
        case AV_CODEC_ID_AAC:
            return u"AAC"_s;
        case AV_CODEC_ID_ALAC:
            return u"ALAC"_s;
        case AV_CODEC_ID_MP2:
            return u"MP2"_s;
        case AV_CODEC_ID_MP3:
            return u"MP3"_s;
        case AV_CODEC_ID_WAVPACK:
            return u"WavPack"_s;
        case AV_CODEC_ID_FLAC:
            return u"FLAC"_s;
        case AV_CODEC_ID_TAK:
            return u"TAK"_s;
        case AV_CODEC_ID_OPUS:
            return u"Opus"_s;
        case AV_CODEC_ID_VORBIS:
            return u"Vorbis"_s;
        case AV_CODEC_ID_WMAV2:
            return u"WMA"_s;
        case AV_CODEC_ID_DTS:
            return u"DTS"_s;
        default:
            return {};
    }
}

bool isLossless(AVCodecID codec)
{
    switch(codec) {
        case AV_CODEC_ID_ALAC:
        case AV_CODEC_ID_WAVPACK:
        case AV_CODEC_ID_FLAC:
        case AV_CODEC_ID_TAK:
            return true;
        default:
            return false;
    }
}

bool canBeVBR(const Track& track, AVCodecID codec)
{
    if(track.codecProfile().contains("VBR"_L1) || track.codecProfile().contains("ABR"_L1)) {
        return true;
    }

    switch(codec) {
        case AV_CODEC_ID_AAC: // Assume AAC is always VBR
        case AV_CODEC_ID_ALAC:
        case AV_CODEC_ID_FLAC:
        case AV_CODEC_ID_OPUS:
        case AV_CODEC_ID_VORBIS:
        case AV_CODEC_ID_WAVPACK:
            return true;
        default:
            return false;
    }
}

void readTrackTotalPair(const QString& trackNumbers, Track& track)
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

void readDiscTotalPair(const QString& discNumbers, Track& track)
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

float parseReplayGain(const QString& gainStr)
{
    QString string = gainStr.trimmed();
    if(string.endsWith("dB"_L1, Qt::CaseInsensitive)) {
        string.chop(2);
        string = string.trimmed();
    }

    bool ok{false};
    const float gain = string.toFloat(&ok);
    return (ok && std::isfinite(gain)) ? gain : Constants::InvalidGain;
}

float parseReplayPeak(const QString& peakStr)
{
    bool ok{false};
    const float peak = peakStr.toFloat(&ok);
    return (ok && std::isfinite(peak)) ? peak : Constants::InvalidPeak;
}

enum class TagScope : uint8_t
{
    Global,
    Track
};

struct ApeTextItem
{
    QString key;
    QStringList values;
};

struct ApeItem
{
    QString key;
    uint32_t flags{0};
    QByteArray value;
};

uint32_t readLe32(const QByteArray& data, qsizetype offset)
{
    return qFromLittleEndian<uint32_t>(data.constData() + offset);
}

bool isApeBinaryItem(uint32_t flags)
{
    const uint32_t itemType = (flags >> 1) & 0x3;
    return itemType == 1 || itemType == 2;
}

QString normaliseTagKey(const QString& key)
{
    QString normalised = key.toUpper();
    normalised.replace(u' ', u'_');
    return normalised;
}

bool isAnyTag(const QString& key, std::initializer_list<QLatin1StringView> tags)
{
    return std::ranges::any_of(tags, [&key](QLatin1StringView tag) { return key == tag; });
}

TagType getTagType(QIODevice* device)
{
    TagType tagType = TagType::Unknown;

    //
    // ID3 1.x
    //

    auto apeOffset = device->size();

    if(device->size() >= 128 && device->seek(device->size() - 128)) {
        auto footer = device->read(8);
        if(footer.startsWith("TAG")) {
            if(device->size() >= 256 && device->seek(device->size() - 256)) {
                auto extFooter = device->read(8);
                // We may have both v1 and v2. Don't return here to let v2 take precedence.
                if(extFooter.startsWith("EXT")) {
                    tagType   = TagType::ID3v1_2;
                    apeOffset = device->size() - 256;
                }
                else {
                    tagType   = TagType::ID3v1_1;
                    apeOffset = device->size() - 128;
                }
            }
        }
    }

    //
    // APE
    //

    if(device->size() >= apeOffset && device->seek(apeOffset - 32)) {
        auto footer = device->read(32);
        if(footer.startsWith("APETAGEX")) {
            return TagType::APE;
        }
    }

    //
    // ID3 2.x
    //

    if(device->seek(0)) {
        auto header = device->read(8);
        if(header.startsWith("ID3")) {
            auto major = static_cast<int>(static_cast<unsigned char>(header.at(3)));
            if(major == 2) {
                return TagType::ID3v2_2;
            }
            if(major == 3) {
                return TagType::ID3v2_3;
            }
            if(major == 4) {
                return TagType::ID3v2_4;
            }
        }
    }

    return tagType;
}

bool supportsSlashSeparatedTextFields(TagType tagType)
{
    using enum TagType;
    return tagType == ID3v1_1 || tagType == ID3v1_2 || tagType == ID3v2_2 || tagType == ID3v2_3;
}

std::optional<std::vector<ApeItem>> readApeItems(QIODevice* device)
{
    if(device->size() < ApeTagFooterSize) {
        return {};
    }

    auto apeOffset = device->size();

    if(device->size() >= 128 && device->seek(device->size() - 128)) {
        const auto footer = device->read(8);

        if(footer.startsWith("TAG")) {
            apeOffset = device->size() - 128;

            if(device->size() >= 256 && device->seek(device->size() - 256)) {
                const auto extFooter = device->read(8);

                if(extFooter.startsWith("EXT")) {
                    apeOffset = device->size() - 256;
                }
            }
        }
    }

    if(apeOffset < ApeTagFooterSize || !device->seek(apeOffset - ApeTagFooterSize)) {
        return {};
    }

    const QByteArray footer = device->read(ApeTagFooterSize);
    if(footer.size() != ApeTagFooterSize || !footer.startsWith("APETAGEX"_L1)) {
        return {};
    }

    static constexpr uint32_t ApeTagFlagIsHeader = 1 << 29;
    static constexpr uint32_t ApeTagMaxSize      = 1024 * 1024 * 16;
    static constexpr uint32_t ApeTagMaxFields    = 65536;

    const uint32_t tagBytes = readLe32(footer, 12);
    const uint32_t fields   = readLe32(footer, 16);
    const uint32_t flags    = readLe32(footer, 20);
    if((flags & ApeTagFlagIsHeader) != 0 || tagBytes < ApeTagFooterSize || tagBytes > ApeTagMaxSize
       || fields > ApeTagMaxFields || std::cmp_greater(tagBytes, apeOffset)) {
        return {};
    }

    const qint64 itemPos = apeOffset - tagBytes;
    const qint64 itemEnd = apeOffset - ApeTagFooterSize;
    if(itemPos < 0 || itemPos > itemEnd || !device->seek(itemPos)) {
        return {};
    }

    std::vector<ApeItem> items;
    for(uint32_t i{0}; i < fields && device->pos() < itemEnd; ++i) {
        const QByteArray header = device->read(8);
        if(header.size() != 8) {
            return {};
        }

        const uint32_t valueSize = readLe32(header, 0);
        const uint32_t itemFlags = readLe32(header, 4);

        QByteArray key;
        while(device->pos() < itemEnd) {
            char c{'\0'};
            if(device->read(&c, 1) != 1) {
                return {};
            }
            if(c == '\0'_L1) {
                break;
            }
            if(static_cast<unsigned char>(c) < 0x20 || static_cast<unsigned char>(c) > 0x7e || key.size() >= 1023) {
                return {};
            }
            key.append(c);
        }

        if(key.isEmpty() || std::cmp_greater(valueSize, itemEnd - device->pos())) {
            return {};
        }

        const QByteArray value = device->read(valueSize);
        if(std::cmp_not_equal(value.size(), valueSize)) {
            return {};
        }

        items.emplace_back(QString::fromLatin1(key), itemFlags, value);
    }

    return items;
}

std::optional<std::vector<ApeTextItem>> readApeTextItems(QIODevice* device)
{
    const auto items = readApeItems(device);
    if(!items) {
        return {};
    }

    std::vector<ApeTextItem> textItems;
    for(const ApeItem& item : *items) {
        if(isApeBinaryItem(item.flags)) {
            continue;
        }

        QStringList values;
        const auto parts = item.value.split('\0');
        for(const QByteArray& part : parts) {
            const QString text = QString::fromUtf8(part).trimmed();
            if(!text.isEmpty()) {
                values.emplace_back(text);
            }
        }

        if(!values.empty()) {
            textItems.emplace_back(item.key, values);
        }
    }

    return textItems;
}

QByteArray readApeCover(QIODevice* device, Track::Cover cover)
{
    const auto items = readApeItems(device);
    if(!items) {
        return {};
    }

    QString targetKey;
    switch(cover) {
        case Track::Cover::Front:
            targetKey = u"COVER ART (FRONT)"_s;
            break;
        case Track::Cover::Back:
            targetKey = u"COVER ART (BACK)"_s;
            break;
        case Track::Cover::Artist:
            targetKey = u"COVER ART (ARTIST)"_s;
            break;
        case Track::Cover::Other:
            return {};
    }

    for(const ApeItem& item : *items) {
        if(!isApeBinaryItem(item.flags) || item.key.compare(targetKey, Qt::CaseInsensitive) != 0) {
            continue;
        }

        const auto separator = item.value.indexOf('\0');
        if(separator >= 0 && separator + 1 < item.value.size()) {
            return item.value.sliced(separator + 1);
        }
    }

    return {};
}

QStringList splitStandardField(TagType tagType, const TagPolicy& policy, const QString& field,
                               const QStringList& values)
{
    if(!supportsSlashSeparatedTextFields(tagType)) {
        return values;
    }
    return Id3Utils::splitStandardField(field, values,
                                        tagType == TagType::ID3v2_3 && policy.splitId3v23SemicolonSeparatedTags);
}

QStringList splitExtraField(TagType tagType, const TagPolicy& policy, const QString& field, const QStringList& values)
{
    return tagType == TagType::ID3v2_3
             ? Id3Utils::splitExtraField(field, values, policy.splitId3v23SemicolonSeparatedTags)
             : values;
}

bool parseStandardTag(Track& track, TagType tagType, const TagPolicy& policy, const QString& key,
                      const QStringList& values, TagScope scope, int chapterCount)
{
    const QString& firstValue = values.front();

    if(key == "ALBUM"_L1) {
        track.setAlbum(firstValue);
        return true;
    }
    if(key == "ARTIST"_L1) {
        track.setArtists(splitStandardField(tagType, policy, QString::fromLatin1(Tag::Artist), values));
        return true;
    }
    if(isAnyTag(key, {"ALBUM_ARTIST"_L1, "ALBUMARTIST"_L1})) {
        track.setAlbumArtists(splitStandardField(tagType, policy, QString::fromLatin1(Tag::AlbumArtist), values));
        return true;
    }
    if(key == "TITLE"_L1) {
        if(scope == TagScope::Global && chapterCount > 1 && track.album().isEmpty()) {
            track.setAlbum(firstValue);
        }
        else {
            track.setTitle(firstValue);
        }
        return true;
    }
    if(key == "GENRE"_L1) {
        track.setGenres(splitStandardField(tagType, policy, QString::fromLatin1(Fooyin::Tag::Genre), values));
        return true;
    }
    if(isAnyTag(key, {"COMPOSER"_L1, "TCOM"_L1})) {
        track.setComposers(splitStandardField(tagType, policy, QString::fromLatin1(Fooyin::Tag::Composer), values));
        return true;
    }
    if(key == "PERFORMER"_L1) {
        track.setPerformers(splitStandardField(tagType, policy, QString::fromLatin1(Fooyin::Tag::Performer), values));
        return true;
    }
    if(key == "COMMENT"_L1) {
        track.setComment(firstValue);
        return true;
    }

    return false;
}

bool parseNumberOrDateTag(Track& track, const QString& key, const QString& value)
{
    if(isAnyTag(key, {"PART_NUMBER"_L1, "TRACK"_L1, "TRACKNUMBER"_L1})) {
        readTrackTotalPair(value, track);
        return true;
    }
    if(isAnyTag(key, {"TRACKTOTAL"_L1, "TOTALTRACKS"_L1})) {
        track.setTrackTotal(value);
        return true;
    }
    if(isAnyTag(key, {"DISC"_L1, "DISCNUMBER"_L1})) {
        readDiscTotalPair(value, track);
        return true;
    }
    if(isAnyTag(key, {"DISCTOTAL"_L1, "TOTALDISCS"_L1})) {
        track.setDiscTotal(value);
        return true;
    }
    if(isAnyTag(key, {"DATE"_L1, "TDRC"_L1, "TDRL"_L1})) {
        track.setDate(value);
        return true;
    }
    if(key == "YEAR"_L1) {
        track.setYear(value.toInt());
        if(track.date().isEmpty()) {
            track.setDate(value);
        }
        return true;
    }

    return false;
}

bool parseId3ExtraTag(Track& track, TagType tagType, const TagPolicy& policy, const QString& key,
                      const QStringList& values)
{
    const QString& firstValue = values.front();

    if(key == "TFLT"_L1) {
        track.addExtraTag(u"FILETYPE"_s, firstValue);
        return true;
    }
    if(key == "TOLY"_L1) {
        const auto splitValues = splitExtraField(tagType, policy, u"ORIGINALLYRICIST"_s, values);
        for(const QString& value : splitValues) {
            track.addExtraTag(u"ORIGINALLYRICIST"_s, value);
        }
        return true;
    }
    if(key == "TLEN"_L1) {
        track.addExtraTag(u"LENGTH"_s, firstValue);
        track.setDuration(firstValue.toULongLong());
        return true;
    }
    if(key == "TGID"_L1) {
        track.addExtraTag(u"PODCASTDID"_s, firstValue);
        return true;
    }
    if(key == "TDES"_L1) {
        track.addExtraTag(u"PODCASTDESC"_s, firstValue);
        return true;
    }
    if(key == "TCAT"_L1) {
        track.addExtraTag(u"PODCASTCATEGORY"_s, firstValue);
        return true;
    }
    if(key.startsWith("ID3V2_PRIV"_L1)) {
        return true;
    }

    return false;
}

bool parseReplayGainTag(Track& track, const QString& key, const QString& value, TagScope scope)
{
    if(key == "REPLAYGAIN_TRACK_GAIN"_L1) {
        track.setRGTrackGain(parseReplayGain(value));
        return true;
    }
    if(key == "REPLAYGAIN_TRACK_PEAK"_L1) {
        track.setRGTrackPeak(parseReplayPeak(value));
        return true;
    }
    if(key == "REPLAYGAIN_ALBUM_GAIN"_L1) {
        track.setRGAlbumGain(parseReplayGain(value));
        return true;
    }
    if(key == "REPLAYGAIN_ALBUM_PEAK"_L1) {
        track.setRGAlbumPeak(parseReplayPeak(value));
        return true;
    }
    if(key == "REPLAYGAIN_GAIN"_L1) {
        const float gain = parseReplayGain(value);
        if(scope == TagScope::Global) {
            track.setRGAlbumGain(gain);
        }
        else {
            track.setRGTrackGain(gain);
        }
        return true;
    }
    if(key == "REPLAYGAIN_PEAK"_L1) {
        const float peak = parseReplayPeak(value);
        if(scope == TagScope::Global) {
            track.setRGAlbumPeak(peak);
        }
        else {
            track.setRGTrackPeak(peak);
        }
        return true;
    }

    return false;
}

void addExtraTagValues(Track& track, TagType tagType, const TagPolicy& policy, const QString& key,
                       const QStringList& values)
{
    const auto splitValues = splitExtraField(tagType, policy, key, values);
    for(const QString& value : splitValues) {
        track.addExtraTag(key, value);
    }
}

void parseTagValues(Track& track, TagType tagType, const QString& rawKey, const QStringList& values, TagScope scope,
                    int chapterCount, const TagPolicy& policy)
{
    if(rawKey.isEmpty() || values.empty()) {
        return;
    }

    const QString key         = normaliseTagKey(rawKey);
    const QString& firstValue = values.front();

    if(parseStandardTag(track, tagType, policy, key, values, scope, chapterCount)) {
        return;
    }
    if(parseNumberOrDateTag(track, key, firstValue)) {
        return;
    }
    if(parseId3ExtraTag(track, tagType, policy, key, values)) {
        return;
    }
    if(parseReplayGainTag(track, key, firstValue, scope)) {
        return;
    }

    if(key == "ENCODER"_L1) {
        track.setTool(firstValue);
    }
    else if(key == "FMPS_RATING"_L1) {
        track.setRating(firstValue.toFloat());
    }
    else {
        addExtraTagValues(track, tagType, policy, key, values);
    }
}

void parseTag(Track& track, TagType tagType, const AVDictionaryEntry* tag, TagScope scope, int chapterCount,
              const TagPolicy& policy)
{
    parseTagValues(track, tagType, QString::fromUtf8(tag->key), {convertString(tag->value)}, scope, chapterCount,
                   policy);
}

bool interleaveSamples(uint8_t* const* in, AudioBuffer& buffer)
{
    const auto format  = buffer.format();
    const int channels = format.channelCount();
    const int samples  = buffer.frameCount();
    const int bps      = format.bytesPerSample();
    auto* out          = buffer.data();

    for(int channel{0}; channel < channels; ++channel) {
        if(!in[channel]) {
            return false;
        }
    }

    const int totalSamples = samples * channels;

    for(int i{0}; i < totalSamples; ++i) {
        const int sampleIndex  = i / channels;
        const int channelIndex = i % channels;

        const auto inOffset  = sampleIndex * bps;
        const auto outOffset = i * bps;
        std::memmove(out + outOffset, in[channelIndex] + inOffset, bps);
    }

    return true;
}

bool interleave(uint8_t* const* in, AudioBuffer& buffer)
{
    if(!in || !buffer.isValid()) {
        return false;
    }

    return interleaveSamples(in, buffer);
}

int ffRead(void* data, uint8_t* buffer, int size)
{
    auto* device = static_cast<QIODevice*>(data);
    QByteArray temp(size, Qt::Uninitialized);
    const auto sizeRead = device->read(temp.data(), size);
    if(sizeRead == 0) {
        return AVERROR_EOF;
    }
    if(sizeRead > 0) {
        std::memcpy(buffer, temp.constData(), sizeRead);
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

FormatContext createAVFormatContext(const AudioSource& source)
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

QByteArray findCover(AVFormatContext* context, Track::Cover type)
{
    const auto count = static_cast<int>(context->nb_streams);

    for(int i{0}; i < count; ++i) {
        const AVStream* avStream = context->streams[i];

        if(avStream->disposition & AV_DISPOSITION_ATTACHED_PIC) {
            QString coverType;
            const AVDictionaryEntry* tag{nullptr};
            if((tag = av_dict_get(avStream->metadata, "comment", tag, AV_DICT_IGNORE_SUFFIX))) {
                coverType = convertString(tag->value).toLower();
            }

            if((type == Track::Cover::Front && (coverType.isEmpty() || coverType.contains("front"_L1)))
               || (type == Track::Cover::Back && coverType.contains("back"_L1))
               || (type == Track::Cover::Artist && coverType.contains("artist"_L1))) {
                const AVPacket pkt = avStream->attached_pic;
                if(pkt.size <= 0 || !pkt.data) {
                    return {};
                }

                QByteArray cover(pkt.size, Qt::Uninitialized);
                std::memcpy(cover.data(), pkt.data, static_cast<size_t>(pkt.size));
                return cover;
            }
        }
    }

    return {};
}
} // namespace

class FFmpegInputPrivate
{
public:
    explicit FFmpegInputPrivate(FFmpegDecoder* self)
        : m_self{self}
    { }

    void reset();
    bool setup(const AudioSource& source);
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

    AVRational m_timeBase{.num = 0, .den = 0};
    bool m_isSeekable{false};
    bool m_draining{false};
    bool m_error{false};
    bool m_eof{false};
    bool m_isDecoding{false};
    bool m_isVbr{false};
    bool m_returnFrame{false};
    bool m_lastErrorRecoverable{false};

    AudioDecoder::DecoderOptions m_options;
    AudioBuffer m_buffer;
    Frame m_frame;
    int m_bufferPos{0};
    int64_t m_seekPos{-1};
    uint64_t m_currentPos{0};
    int m_bitrate{0};
    int m_skipBytes{0};
    int m_consecutiveDecodeErrors{0};
};

void FFmpegInputPrivate::reset()
{
    m_error                   = false;
    m_eof                     = false;
    m_isDecoding              = false;
    m_draining                = false;
    m_isVbr                   = false;
    m_bitrate                 = 0;
    m_bufferPos               = 0;
    m_currentPos              = 0;
    m_seekPos                 = -1;
    m_skipBytes               = 0;
    m_consecutiveDecodeErrors = 0;
    m_lastErrorRecoverable    = false;
    m_buffer.clear();

    if(m_context) {
        m_context.reset();
    }

    if(m_ioContext) {
        m_ioContext.reset();
    }

    m_stream = {};
    m_codec  = {};
    m_buffer = {};
}

bool FFmpegInputPrivate::setup(const AudioSource& source)
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

    if(createCodec(m_stream.avStream())) {
        m_audioFormat = Utils::audioFormatFromCodec(m_stream.avStream()->codecpar, m_codec.context()->sample_fmt);
        return true;
    }

    return false;
}

void FFmpegInputPrivate::checkIsVbr(const Track& track)
{
    const auto codec = m_codec.context()->codec_id;
    m_isVbr          = canBeVBR(track, codec);
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
        m_lastErrorRecoverable = isRecoverableDecodeError(result);

        if(m_lastErrorRecoverable) {
            ++m_consecutiveDecodeErrors;
            if(m_consecutiveDecodeErrors <= MaxConsecutiveDecodeErrors) {
                return result;
            }
        }

        m_error = true;
        return result;
    }

    m_consecutiveDecodeErrors = 0;
    m_lastErrorRecoverable    = false;

    if(!m_returnFrame) {
        const auto sampleCount   = m_audioFormat.bytesPerFrame() * m_frame.sampleCount();
        const uint64_t startTime = m_codec.context()->codec_id == AV_CODEC_ID_APE ? m_currentPos : m_frame.ptsMs();

        if(m_codec.isPlanar()) {
            m_buffer = {m_audioFormat, startTime};
            m_buffer.resize(static_cast<size_t>(sampleCount));

            if(!interleave(m_frame.avFrame()->extended_data, m_buffer)) {
                qCWarning(FFMPEG) << "Invalid planar audio frame";
                m_buffer.clear();
                m_error = true;
                return AVERROR_INVALIDDATA;
            }
        }
        else {
            m_buffer = {m_frame.avFrame()->data[0], static_cast<size_t>(sampleCount), m_audioFormat, startTime};
        }

        if(m_skipBytes > 0) {
            const auto len = std::min(sampleCount, m_skipBytes);
            m_skipBytes -= len;

            if(sampleCount - len == 0) {
                m_buffer.clear();
            }
            else {
                std::memmove(m_buffer.data(), m_buffer.data() + len, sampleCount - len);
                m_buffer.resize(sampleCount - len);
                m_buffer.setStartTime(m_buffer.startTime()
                                      + m_audioFormat.durationForBytes(static_cast<uint64_t>(len)));
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
            const int bitrate = static_cast<int>((packet->size * 8) / durSecs) / 1000;
            if(bitrate > 0) {
                m_bitrate = bitrate;
            }
        }
    }

    if(m_seekPos > 0 && packet->pts != AV_NOPTS_VALUE) {
        const auto packetPts = av_rescale_q_rnd(packet->pts, m_timeBase, TimeBaseMs, AVRounding::AV_ROUND_DOWN);
        if(packetPts < m_seekPos) {
            m_skipBytes = m_audioFormat.bytesForDuration(static_cast<uint64_t>(m_seekPos - packetPts));
        }
    }
    m_seekPos = -1;

    decodeAudio(packet);
}

void FFmpegInputPrivate::seek(uint64_t pos)
{
    if(!m_context || !m_isSeekable) {
        return;
    }

    // Allow recovery from transient decode errors
    if(m_error && m_lastErrorRecoverable) {
        m_error = false;
    }
    m_consecutiveDecodeErrors = 0;
    m_lastErrorRecoverable    = false;

    m_seekPos = static_cast<int64_t>(pos);

    constexpr static auto min = std::numeric_limits<int64_t>::min();
    constexpr static auto max = std::numeric_limits<int64_t>::max();

    const auto target = av_rescale_q_rnd(m_seekPos, TimeBaseMs, TimeBaseAv, AVRounding::AV_ROUND_DOWN);

    if(auto ret = avformat_seek_file(m_context.get(), -1, min, target, max, 0); ret < 0) {
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

QStringList FFmpegDecoder::preferredExtensions() const
{
    return ffmpegPreferredExtensions();
}

int FFmpegDecoder::bitrate() const
{
    return p->m_bitrate;
}

std::optional<AudioFormat> FFmpegDecoder::init(const AudioSource& source, const Track& track, DecoderOptions options)
{
    p->m_options = options;

    if(p->setup(source)) {
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
            buffer.reserve(static_cast<size_t>(bytesRequested));
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

std::optional<bool> FFmpegDecoder::isPlanar() const
{
    if(p->m_codec.isValid()) {
        return p->m_codec.isPlanar();
    }

    return {};
}

class FFmpegReaderPrivate
{
public:
    void reset()
    {
        if(m_context) {
            m_context.reset();
        }

        if(m_ioContext) {
            m_ioContext.reset();
        }

        m_stream       = {};
        m_chapterCount = 0;
    }

    bool setup(const AudioSource& source)
    {
        reset();

        FormatContext context = createAVFormatContext(source);
        m_context             = std::move(context.formatContext);
        m_ioContext           = std::move(context.ioContext);

        if(!m_context) {
            return false;
        }

        m_stream = Utils::findAudioStream(m_context.get());
        if(!m_stream.isValid()) {
            return false;
        }

        if(m_context->nb_chapters >= 1) {
            m_chapterCount = static_cast<int>(m_context->nb_chapters);
        }

        return true;
    }

    IOContextPtr m_ioContext;
    FormatContextPtr m_context;
    Stream m_stream;
    int m_chapterCount{0};
};

FFmpegReader::FFmpegReader()
    : p{std::make_unique<FFmpegReaderPrivate>()}
{ }

FFmpegReader::~FFmpegReader() = default;

QStringList FFmpegReader::extensions() const
{
    const FySettings settings;
    return fileExtensions(settings.value(Settings::Core::Internal::FFmpegAllExtensions).toBool());
}

QStringList FFmpegReader::preferredExtensions() const
{
    return ffmpegPreferredExtensions();
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
    return p->m_chapterCount > 1 ? p->m_chapterCount : 1;
}

bool FFmpegReader::init(const AudioSource& source)
{
    return p->setup(source);
}

bool FFmpegReader::readTrack(const AudioSource& source, Track& track)
{
    if(!p->m_context || !p->m_stream.isValid()) {
        return false;
    }

    const auto* avStream = p->m_stream.avStream();
    const auto* codecPar = p->m_stream.avStream()->codecpar;

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

    const auto format = Utils::audioFormatFromCodec(p->m_stream.avStream()->codecpar, avCodecContext->sample_fmt);

    track.setCodec(getCodec(codecPar->codec_id));
    track.setSampleRate(format.sampleRate());
    track.setChannels(format.channelCount());
    track.setBitDepth(format.bitdepth());
    track.setEncoding(isLossless(codecPar->codec_id) ? u"Lossless"_s : u"Lossy"_s);

    if(track.duration() == 0) {
        AVRational timeBase = avStream->time_base;
        auto duration       = avStream->duration;
        if(duration <= 0 || std::cmp_equal(duration, AV_NOPTS_VALUE)) {
            duration = p->m_context->duration;
            timeBase = TimeBaseAv;
        }
        const auto durationMs = static_cast<uint64_t>(av_rescale_q(duration, timeBase, TimeBaseMs));
        track.setDuration(durationMs);
    }

    auto bitrate = static_cast<int>(codecPar->bit_rate / 1000);
    if(bitrate <= 0) {
        bitrate = static_cast<int>(p->m_context->bit_rate / 1000);
    }
    if(bitrate > 0) {
        track.setBitrate(bitrate);
    }

    const auto tagType     = getTagType(source.device);
    const TagPolicy policy = tagPolicy();

    bool readGlobalTagsFromContext{true};
    if(tagType == TagType::APE) {
        if(const auto apeItems = readApeTextItems(source.device); apeItems && !apeItems->empty()) {
            for(const auto& item : *apeItems) {
                parseTagValues(track, tagType, item.key, item.values, TagScope::Global, p->m_chapterCount, policy);
            }
            readGlobalTagsFromContext = false;
        }
    }

    if(readGlobalTagsFromContext) {
        const AVDictionaryEntry* tag{nullptr};
        while((tag = av_dict_get(p->m_context->metadata, "", tag, AV_DICT_IGNORE_SUFFIX))) {
            parseTag(track, tagType, tag, TagScope::Global, p->m_chapterCount, policy);
        }
    }

    const AVDictionaryEntry* streamTag{nullptr};
    while((streamTag = av_dict_get(avStream->metadata, "", streamTag, AV_DICT_IGNORE_SUFFIX))) {
        parseTag(track, tagType, streamTag, TagScope::Track, p->m_chapterCount, policy);
    }

    const int subsong = track.subsong();
    if(p->m_chapterCount > 1 && subsong >= 0 && subsong < p->m_chapterCount) {
        const AVChapter* chapter = p->m_context->chapters[subsong];

        const auto startMs = static_cast<uint64_t>(av_rescale_q(chapter->start, chapter->time_base, TimeBaseMs));
        const auto endMs   = static_cast<uint64_t>(av_rescale_q(chapter->end, chapter->time_base, TimeBaseMs));

        track.setOffset(startMs);

        // Set default track number before parsing chapter tags
        track.setTrackNumber(QString::number(subsong + 1));

        // Read chapter-level metadata (overrides container tags where defined)
        const AVDictionaryEntry* chapterTag{nullptr};
        while((chapterTag = av_dict_get(chapter->metadata, "", chapterTag, AV_DICT_IGNORE_SUFFIX))) {
            parseTag(track, tagType, chapterTag, TagScope::Track, p->m_chapterCount, policy);
        }

        // Ensure chapter timing and totals are authoritative
        track.setDuration(endMs - startMs);
        track.setTrackTotal(QString::number(p->m_chapterCount));

        track.setIsChapter(true);
    }

    return true;
}

QByteArray FFmpegReader::readCover(const AudioSource& source, const Track& /*track*/, Track::Cover cover)
{
    if(source.device) {
        const int64_t pos         = source.device->pos();
        const QByteArray apeCover = readApeCover(source.device, cover);
        source.device->seek(pos);
        if(!apeCover.isEmpty()) {
            return apeCover;
        }
    }

    if(!p->m_context) {
        return {};
    }

    return findCover(p->m_context.get(), cover);
}

bool FFmpegReader::writeTrack(const AudioSource& /*source*/, const Track& /*track*/, WriteOptions /*options*/)
{
    return false;
}
} // namespace Fooyin
