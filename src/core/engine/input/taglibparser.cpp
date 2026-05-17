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

#include "taglibparser.h"

#include "ratingtagpolicy.h"
#include "tagdefs.h"
#include "tagpolicy.h"

#include <core/constants.h>
#include <core/track.h>
#include <utils/helpers.h>

#include <taglib/aifffile.h>
#include <taglib/apefile.h>
#include <taglib/apefooter.h>
#include <taglib/apetag.h>
#include <taglib/asffile.h>
#include <taglib/asfpicture.h>
#include <taglib/asftag.h>
#include <taglib/attachedpictureframe.h>
#if (TAGLIB_MAJOR_VERSION >= 2)
#include <taglib/dsdifffile.h>
#include <taglib/dsffile.h>
#endif
#include <taglib/fileref.h>
#include <taglib/flacfile.h>
#include <taglib/id3v1tag.h>
#include <taglib/id3v2framefactory.h>
#include <taglib/id3v2tag.h>
#include <taglib/mp4file.h>
#include <taglib/mp4tag.h>
#include <taglib/mpcfile.h>
#include <taglib/mpegfile.h>
#include <taglib/mpegheader.h>
#include <taglib/oggfile.h>
#include <taglib/opusfile.h>
#include <taglib/popularimeterframe.h>
#include <taglib/tag.h>
#include <taglib/textidentificationframe.h>
#include <taglib/tfilestream.h>
#include <taglib/tpropertymap.h>
#include <taglib/vorbisfile.h>
#include <taglib/wavfile.h>
#include <taglib/wavpackfile.h>
#include <taglib/xingheader.h>

#include <QBuffer>
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QLoggingCategory>
#include <QMimeDatabase>
#include <QPixmap>

#include <cmath>
#include <cstring>
#include <limits>
#include <optional>
#include <set>

Q_LOGGING_CATEGORY(TAGLIB, "fy.taglib")

using namespace Qt::StringLiterals;

constexpr auto OpusR128Scale     = 256.0;
constexpr auto RGReferenceOffset = 5.0;

constexpr auto BufferSize = 1024;

namespace Fooyin {
namespace {
class IODeviceStream : public TagLib::IOStream
{
public:
    IODeviceStream(QIODevice* input, const QString& filename)
        : m_input{input}
        , m_fileName{filename.toLocal8Bit()}
    {
        m_input->seek(0);
    }

    [[nodiscard]] TagLib::FileName name() const override
    {
        return m_fileName.constData();
    }

#if TAGLIB_MAJOR_VERSION >= 2
    TagLib::ByteVector readBlock(size_t length) override
#else
    TagLib::ByteVector readBlock(unsigned long length) override
#endif
    {
        if(!isOpen()) {
            qCDebug(TAGLIB) << "Unable to read unopened file";
            return {};
        }

        std::vector<char> data(length);
        const auto lenRead = m_input->read(data.data(), static_cast<qint64>(length));
        if(lenRead < 0) {
            m_input->close();
            return {};
        }
        return TagLib::ByteVector{data.data(), static_cast<unsigned int>(lenRead)};
    }

    void writeBlock(const TagLib::ByteVector& data) override
    {
        if(!isOpen()) {
            qCDebug(TAGLIB) << "Unable to write to unopened file";
            return;
        }

        if(readOnly()) {
            qCDebug(TAGLIB) << "Unable to write to read-only file";
            return;
        }

        m_input->write(data.data(), data.size());
    }

#if TAGLIB_MAJOR_VERSION >= 2
    void insert(const TagLib::ByteVector& data, TagLib::offset_t start, size_t replace) override
#else
    void insert(const TagLib::ByteVector& data, unsigned long start, unsigned long replace) override
#endif
    {
        if(!isOpen()) {
            qCDebug(TAGLIB) << "Unable to write to unopened file";
            return;
        }

        if(readOnly()) {
            qCDebug(TAGLIB) << "Unable to write to read-only file";
            return;
        }

        if(data.size() == replace) {
            seek(start, Beginning);
            writeBlock(data);
            return;
        }

        if(data.size() < replace) {
            seek(start, Beginning);
            writeBlock(data);
            removeBlock(start + data.size(), replace - data.size());
            return;
        }

        size_t bufferLength = BufferSize;
        while(data.size() - replace > bufferLength) {
            bufferLength += BufferSize;
        }

        auto readPosition  = static_cast<qint64>(start) + static_cast<qint64>(replace);
        auto writePosition = static_cast<qint64>(start);

        TagLib::ByteVector buffer{data};
        TagLib::ByteVector aboutToOverwrite(static_cast<unsigned int>(bufferLength));

        qint64 bytesRead{-1};
        while(bytesRead != 0) {
            seek(readPosition, Beginning);
            bytesRead = m_input->read(aboutToOverwrite.data(), aboutToOverwrite.size());
            aboutToOverwrite.resize(bytesRead);
            readPosition += static_cast<qint64>(bufferLength);

            if(std::cmp_less(bytesRead, bufferLength)) {
                clear();
            }

            seek(writePosition, Beginning);
            writeBlock(buffer);

            writePosition += static_cast<qint64>(buffer.size());

            buffer = aboutToOverwrite;
        }
    }

#if TAGLIB_MAJOR_VERSION >= 2
    void removeBlock(TagLib::offset_t start, size_t length) override
#else
    void removeBlock(unsigned long start, unsigned long length) override
#endif
    {
        if(!isOpen()) {
            qCDebug(TAGLIB) << "Unable to write to unopened file";
            return;
        }

        if(readOnly()) {
            qCDebug(TAGLIB) << "Unable to write to read-only file";
            return;
        }

        QByteArray buffer(BufferSize, 0);

        auto readPosition  = static_cast<qint64>(start) + static_cast<qint64>(length);
        auto writePosition = static_cast<qint64>(start);

        qint64 bytesRead{std::numeric_limits<qint64>::max()};
        while(bytesRead != 0) {
            m_input->seek(readPosition);
            bytesRead = m_input->peek(buffer.data(), BufferSize);

            if(bytesRead < buffer.size()) {
                clear();
                buffer.resize(bytesRead);
            }

            m_input->seek(writePosition);
            m_input->write(buffer);

            writePosition += bytesRead;
            readPosition += bytesRead;
        }

        truncate(writePosition);
    }

    [[nodiscard]] bool readOnly() const override
    {
        return !m_input->isWritable();
    }

    [[nodiscard]] bool isOpen() const override
    {
        return m_input->isOpen();
    }

#if TAGLIB_MAJOR_VERSION >= 2
    void seek(TagLib::offset_t offset, Position p) override
#else
    void seek(long offset, Position p) override
#endif
    {
        if(!isOpen()) {
            qCDebug(TAGLIB) << "Unable to write to unopened file";
            return;
        }

        const auto seekPos = static_cast<qint64>(offset);
        switch(p) {
            case Beginning:
                m_input->seek(seekPos);
                break;
            case Current:
                m_input->seek(m_input->pos() + seekPos);
                break;
            case End:
                m_input->seek(m_input->size() + seekPos);
                break;
        }
    }

    void clear() override
    {
        m_input->seek(0);
        TagLib::IOStream::clear();
    }

#if TAGLIB_MAJOR_VERSION >= 2
    [[nodiscard]] TagLib::offset_t tell() const override
#else
    [[nodiscard]] long tell() const override
#endif
    {
        return m_input->pos();
    }

#if TAGLIB_MAJOR_VERSION >= 2
    TagLib::offset_t length() override
#else
    long length() override
#endif
    {
        return m_input->size();
    }

#if TAGLIB_MAJOR_VERSION >= 2
    void truncate(TagLib::offset_t length) override
#else
    void truncate(long length) override
#endif
    {
        if(!isOpen()) {
            qCDebug(TAGLIB) << "Unable to write to unopened file";
            return;
        }

        if(readOnly()) {
            qCDebug(TAGLIB) << "Unable to write to read-only file";
            return;
        }

        if(length < 0) {
            return;
        }

        const auto currPos = m_input->pos();

        if(auto* file = qobject_cast<QFile*>(m_input)) {
            file->resize(length);
        }
        else if(auto* buffer = qobject_cast<QBuffer*>(m_input)) {
            buffer->buffer().resize(length);
        }
        else {
            qCDebug(TAGLIB) << "Unable to truncate device";
        }

        if(currPos < length) {
            m_input->seek(currPos);
        }
        else {
            m_input->seek(length);
        }
    }

private:
    QIODevice* m_input;
    QByteArray m_fileName;
};

constexpr std::array mp4ToTag{
    std::pair(Mp4::Title, Tag::Title),
    std::pair(Mp4::Artist, Tag::Artist),
    std::pair(Mp4::Album, Tag::Album),
    std::pair(Mp4::AlbumArtist, Tag::AlbumArtist),
    std::pair(Mp4::Genre, Tag::Genre),
    std::pair(Mp4::Composer, Tag::Composer),
    std::pair(Mp4::Performer, Tag::Performer),
    std::pair(Mp4::Comment, Tag::Comment),
    std::pair(Mp4::Date, Tag::Date),
    std::pair(Mp4::Rating, Tag::Rating),
    std::pair(Mp4::RatingAlt, Tag::Rating),
    std::pair(Mp4::TrackNumber, Tag::TrackNumber),
    std::pair(Mp4::Disc, Tag::Disc),
    std::pair("cpil", "COMPILATION"),
    std::pair("tmpo", "BPM"),
    std::pair("cprt", "COPYRIGHT"),
    std::pair("\251too", "ENCODING"),
    std::pair("\251enc", "ENCODEDBY"),
    std::pair("\251grp", "GROUPING"),
    std::pair("soal", "ALBUMSORT"),
    std::pair("soaa", "ALBUMARTISTSORT"),
    std::pair("soar", "ARTISTSORT"),
    std::pair("sonm", "TITLESORT"),
    std::pair("soco", "COMPOSERSORT"),
    std::pair("sosn", "SHOWSORT"),
    std::pair("shwm", "SHOWWORKMOVEMENT"),
    std::pair("pgap", "GAPLESSPLAYBACK"),
    std::pair("pcst", "PODCAST"),
    std::pair("catg", "PODCASTCATEGORY"),
    std::pair("desc", "PODCASTDESC"),
    std::pair("egid", "PODCASTID"),
    std::pair("purl", "PODCASTURL"),
    std::pair("tves", "TVEPISODE"),
    std::pair("tven", "TVEPISODEID"),
    std::pair("tvnn", "TVNETWORK"),
    std::pair("tvsn", "TVSEASON"),
    std::pair("tvsh", "TVSHOW"),
    std::pair("\251wrk", "WORK"),
    std::pair("\251mvn", "MOVEMENTNAME"),
    std::pair("\251mvi", "MOVEMENTNUMBER"),
    std::pair("\251mvc", "MOVEMENTCOUNT"),
    std::pair("----:com.apple.iTunes:MusicBrainz Track Id", "MUSICBRAINZ_TRACKID"),
    std::pair("----:com.apple.iTunes:MusicBrainz Artist Id", "MUSICBRAINZ_ARTISTID"),
    std::pair("----:com.apple.iTunes:MusicBrainz Album Id", "MUSICBRAINZ_ALBUMID"),
    std::pair("----:com.apple.iTunes:MusicBrainz Album Artist Id", "MUSICBRAINZ_ALBUMARTISTID"),
    std::pair("----:com.apple.iTunes:MusicBrainz Release Group Id", "MUSICBRAINZ_RELEASEGROUPID"),
    std::pair("----:com.apple.iTunes:MusicBrainz Release Track Id", "MUSICBRAINZ_RELEASETRACKID"),
    std::pair("----:com.apple.iTunes:MusicBrainz Work Id", "MUSICBRAINZ_WORKID"),
    std::pair("----:com.apple.iTunes:MusicBrainz Album Release Country", "RELEASECOUNTRY"),
    std::pair("----:com.apple.iTunes:MusicBrainz Album Status", "RELEASESTATUS"),
    std::pair("----:com.apple.iTunes:MusicBrainz Album Type", "RELEASETYPE"),
    std::pair("----:com.apple.iTunes:ARTISTS", "ARTISTS"),
    std::pair("----:com.apple.iTunes:ORIGINALDATE", "ORIGINALDATE"),
    std::pair("----:com.apple.iTunes:ASIN", "ASIN"),
    std::pair("----:com.apple.iTunes:LABEL", "LABEL"),
    std::pair("----:com.apple.iTunes:LYRICIST", "LYRICIST"),
    std::pair("----:com.apple.iTunes:CONDUCTOR", "CONDUCTOR"),
    std::pair("----:com.apple.iTunes:REMIXER", "REMIXER"),
    std::pair("----:com.apple.iTunes:ENGINEER", "ENGINEER"),
    std::pair("----:com.apple.iTunes:PRODUCER", "PRODUCER"),
    std::pair("----:com.apple.iTunes:DJMIXER", "DJMIXER"),
    std::pair("----:com.apple.iTunes:MIXER", "MIXER"),
    std::pair("----:com.apple.iTunes:SUBTITLE", "SUBTITLE"),
    std::pair("----:com.apple.iTunes:DISCSUBTITLE", "DISCSUBTITLE"),
    std::pair("----:com.apple.iTunes:MOOD", "MOOD"),
    std::pair("----:com.apple.iTunes:ISRC", "ISRC"),
    std::pair("----:com.apple.iTunes:CATALOGNUMBER", "CATALOGNUMBER"),
    std::pair("----:com.apple.iTunes:BARCODE", "BARCODE"),
    std::pair("----:com.apple.iTunes:SCRIPT", "SCRIPT"),
    std::pair("----:com.apple.iTunes:LANGUAGE", "LANGUAGE"),
    std::pair("----:com.apple.iTunes:LICENSE", "LICENSE"),
    std::pair("----:com.apple.iTunes:MEDIA", "MEDIA"),
    std::pair("----:com.apple.iTunes:replaygain_album_gain", "REPLAYGAIN_ALBUM_GAIN"),
    std::pair("----:com.apple.iTunes:replaygain_album_peak", "REPLAYGAIN_ALBUM_PEAK"),
    std::pair("----:com.apple.iTunes:replaygain_track_gain", "REPLAYGAIN_TRACK_GAIN"),
    std::pair("----:com.apple.iTunes:replaygain_track_peak", "REPLAYGAIN_TRACK_PEAK"),
};

constexpr std::array tagToMp4{
    std::pair(Fooyin::Tag::Title, Fooyin::Mp4::Title),
    std::pair(Fooyin::Tag::Artist, Fooyin::Mp4::Artist),
    std::pair(Fooyin::Tag::Album, Fooyin::Mp4::Album),
    std::pair(Fooyin::Tag::AlbumArtist, Fooyin::Mp4::AlbumArtist),
    std::pair(Fooyin::Tag::Genre, Fooyin::Mp4::Genre),
    std::pair(Fooyin::Tag::Composer, Fooyin::Mp4::Composer),
    std::pair(Fooyin::Tag::Performer, Fooyin::Mp4::Performer),
    std::pair(Fooyin::Tag::Comment, Fooyin::Mp4::Comment),
    std::pair(Fooyin::Tag::Date, Fooyin::Mp4::Date),
    std::pair(Fooyin::Tag::Rating, Fooyin::Mp4::Rating),
    std::pair(Fooyin::Tag::TrackNumber, Fooyin::Mp4::TrackNumber),
    std::pair(Tag::Disc, Mp4::Disc),
    std::pair("COMPILATION", "cpil"),
    std::pair("BPM", "tmpo"),
    std::pair("COPYRIGHT", "cprt"),
    std::pair("ENCODING", "\251too"),
    std::pair("ENCODEDBY", "\251enc"),
    std::pair("GROUPING", "\251grp"),
    std::pair("ALBUMSORT", "soal"),
    std::pair("ALBUMARTISTSORT", "soaa"),
    std::pair("ARTISTSORT", "soar"),
    std::pair("TITLESORT", "sonm"),
    std::pair("COMPOSERSORT", "soco"),
    std::pair("SHOWSORT", "sosn"),
    std::pair("SHOWWORKMOVEMENT", "shwm"),
    std::pair("GAPLESSPLAYBACK", "pgap"),
    std::pair("PODCAST", "pcst"),
    std::pair("PODCASTCATEGORY", "catg"),
    std::pair("PODCASTDESC", "desc"),
    std::pair("PODCASTID", "egid"),
    std::pair("PODCASTURL", "purl"),
    std::pair("TVEPISODE", "tves"),
    std::pair("TVEPISODEID", "tven"),
    std::pair("TVNETWORK", "tvnn"),
    std::pair("TVSEASON", "tvsn"),
    std::pair("TVSHOW", "tvsh"),
    std::pair("WORK", "\251wrk"),
    std::pair("MOVEMENTNAME", "\251mvn"),
    std::pair("MOVEMENTNUMBER", "\251mvi"),
    std::pair("MOVEMENTCOUNT", "\251mvc"),
    std::pair("MUSICBRAINZ_TRACKID", "----:com.apple.iTunes:MusicBrainz Track Id"),
    std::pair("MUSICBRAINZ_ARTISTID", "----:com.apple.iTunes:MusicBrainz Artist Id"),
    std::pair("MUSICBRAINZ_ALBUMID", "----:com.apple.iTunes:MusicBrainz Album Id"),
    std::pair("MUSICBRAINZ_ALBUMARTISTID", "----:com.apple.iTunes:MusicBrainz Album Artist Id"),
    std::pair("MUSICBRAINZ_RELEASEGROUPID", "----:com.apple.iTunes:MusicBrainz Release Group Id"),
    std::pair("MUSICBRAINZ_RELEASETRACKID", "----:com.apple.iTunes:MusicBrainz Release Track Id"),
    std::pair("MUSICBRAINZ_WORKID", "----:com.apple.iTunes:MusicBrainz Work Id"),
    std::pair("RELEASECOUNTRY", "----:com.apple.iTunes:MusicBrainz Album Release Country"),
    std::pair("RELEASESTATUS", "----:com.apple.iTunes:MusicBrainz Album Status"),
    std::pair("RELEASETYPE", "----:com.apple.iTunes:MusicBrainz Album Type"),
    std::pair("ARTISTS", "----:com.apple.iTunes:ARTISTS"),
    std::pair("ORIGINALDATE", "----:com.apple.iTunes:ORIGINALDATE"),
    std::pair("ASIN", "----:com.apple.iTunes:ASIN"),
    std::pair("LABEL", "----:com.apple.iTunes:LABEL"),
    std::pair("LYRICIST", "----:com.apple.iTunes:LYRICIST"),
    std::pair("CONDUCTOR", "----:com.apple.iTunes:CONDUCTOR"),
    std::pair("REMIXER", "----:com.apple.iTunes:REMIXER"),
    std::pair("ENGINEER", "----:com.apple.iTunes:ENGINEER"),
    std::pair("PRODUCER", "----:com.apple.iTunes:PRODUCER"),
    std::pair("DJMIXER", "----:com.apple.iTunes:DJMIXER"),
    std::pair("MIXER", "----:com.apple.iTunes:MIXER"),
    std::pair("SUBTITLE", "----:com.apple.iTunes:SUBTITLE"),
    std::pair("DISCSUBTITLE", "----:com.apple.iTunes:DISCSUBTITLE"),
    std::pair("MOOD", "----:com.apple.iTunes:MOOD"),
    std::pair("ISRC", "----:com.apple.iTunes:ISRC"),
    std::pair("CATALOGNUMBER", "----:com.apple.iTunes:CATALOGNUMBER"),
    std::pair("BARCODE", "----:com.apple.iTunes:BARCODE"),
    std::pair("SCRIPT", "----:com.apple.iTunes:SCRIPT"),
    std::pair("LANGUAGE", "----:com.apple.iTunes:LANGUAGE"),
    std::pair("LICENSE", "----:com.apple.iTunes:LICENSE"),
    std::pair("MEDIA", "----:com.apple.iTunes:MEDIA"),
    std::pair("REPLAYGAIN_ALBUM_GAIN", "----:com.apple.iTunes:replaygain_album_gain"),
    std::pair("REPLAYGAIN_ALBUM_PEAK", "----:com.apple.iTunes:replaygain_album_peak"),
    std::pair("REPLAYGAIN_TRACK_GAIN", "----:com.apple.iTunes:replaygain_track_gain"),
    std::pair("REPLAYGAIN_TRACK_PEAK", "----:com.apple.iTunes:replaygain_track_peak"),
};

QString findMp4Tag(const TagLib::String& tag)
{
    for(const auto& [key, value] : mp4ToTag) {
        if(tag == key) {
            return QString::fromUtf8(value);
        }
    }
    return {};
}

TagLib::String findMp4Tag(const QString& tag)
{
    for(const auto& [key, value] : tagToMp4) {
        if(tag == QLatin1String(key)) {
            return value;
        }
    }
    return {};
}

QString convertString(const TagLib::String& str)
{
    return QString::fromStdString(str.to8Bit(true));
}

TagLib::String convertString(const QString& str)
{
    return QStringToTString(str);
}

QStringList convertStringList(const TagLib::StringList& strList)
{
    QStringList list;
    list.reserve(strList.size());

    for(const auto& str : strList) {
        if(!str.isEmpty()) {
            list.append(convertString(str));
        }
    }

    return list;
}

TagLib::StringList convertStringList(const QStringList& strList)
{
    TagLib::StringList list;

    for(const QString& str : strList) {
        list.append(convertString(str));
    }

    return list;
}

float gainStringToFloat(const TagLib::String& gainString)
{
    QString string = convertString(gainString).trimmed();
    if(string.endsWith("dB"_L1, Qt::CaseInsensitive)) {
        string.chop(2);
        string = string.trimmed();
    }

    bool ok{false};
    const float gain = string.toFloat(&ok);
    return (ok && std::isfinite(gain)) ? gain : Constants::InvalidGain;
};

QString gainToString(const float gain)
{
    return u"%1 dB"_s.arg(QString::number(gain, 'f', 2));
};

float peakStringToFloat(const TagLib::String& peakString)
{
    const QString string = convertString(peakString).trimmed();

    bool ok{false};
    const float peak = string.toFloat(&ok);
    return (ok && std::isfinite(peak)) ? peak : Constants::InvalidPeak;
};

std::optional<float> opusR128ToReplayGain(const TagLib::String& gainString)
{
    bool ok{false};
    const int gainQ78 = convertString(gainString).trimmed().toInt(&ok);
    if(!ok) {
        return {};
    }

    return static_cast<float>((static_cast<double>(gainQ78) / OpusR128Scale) + RGReferenceOffset);
}

std::optional<int16_t> replayGainToOpusR128Q78(float gainDb)
{
    if(!std::isfinite(gainDb)) {
        return {};
    }

    const auto gainQ78 = std::lround((static_cast<double>(gainDb) - RGReferenceOffset) * OpusR128Scale);

    if(gainQ78 < std::numeric_limits<int16_t>::min() || gainQ78 > std::numeric_limits<int16_t>::max()) {
        return {};
    }

    return static_cast<int16_t>(gainQ78);
}

struct OpusHeadPage
{
    QByteArray page;
    qsizetype packetOffset{0};
    qsizetype gainOffset{0};
};

uint32_t oggPageCrc(QByteArray page)
{
    page[22] = 0;
    page[23] = 0;
    page[24] = 0;
    page[25] = 0;

    uint32_t crc{0};
    for(const char byte : std::as_const(page)) {
        crc ^= static_cast<uint32_t>(static_cast<unsigned char>(byte)) << 24;
        for(int bit{0}; bit < 8; ++bit) {
            crc = (crc & 0x80000000) != 0 ? (crc << 1) ^ 0x04C11DB7 : (crc << 1);
        }
    }

    return crc;
}

std::optional<OpusHeadPage> readOpusHeadPage(QIODevice* device)
{
    if(!device || !device->isOpen() || !device->isReadable() || device->isSequential()) {
        return {};
    }

    const qint64 originalPos = device->pos();

    if(!device->seek(0)) {
        return {};
    }

    struct RestorePos
    {
        QIODevice* device;
        qint64 pos;

        ~RestorePos()
        {
            if(device) {
                device->seek(pos);
            }
        }
    };

    const RestorePos restore{.device = device, .pos = originalPos};

    QByteArray header(27, Qt::Uninitialized);
    if(device->read(header.data(), header.size()) != header.size()) {
        return {};
    }

    if(std::memcmp(header.constData(), "OggS", 4) != 0) {
        return {};
    }

    const int segmentCount = static_cast<unsigned char>(header[26]);
    QByteArray segmentTable(segmentCount, Qt::Uninitialized);
    if(device->read(segmentTable.data(), segmentTable.size()) != segmentTable.size()) {
        return {};
    }

    int packetSize{0};
    for(const char laceValue : std::as_const(segmentTable)) {
        packetSize += static_cast<unsigned char>(laceValue);
    }

    const auto packetOffset = header.size() + segmentTable.size();
    if(packetSize < 19) {
        return {};
    }

    QByteArray page(packetOffset + packetSize, Qt::Uninitialized);
    std::memcpy(page.data(), header.constData(), static_cast<size_t>(header.size()));
    std::memcpy(page.data() + header.size(), segmentTable.constData(), static_cast<size_t>(segmentTable.size()));

    if(device->read(page.data() + packetOffset, packetSize) != packetSize) {
        return {};
    }

    const auto storedCrc = static_cast<uint32_t>(static_cast<unsigned char>(page[22]))
                         | (static_cast<uint32_t>(static_cast<unsigned char>(page[23])) << 8)
                         | (static_cast<uint32_t>(static_cast<unsigned char>(page[24])) << 16)
                         | (static_cast<uint32_t>(static_cast<unsigned char>(page[25])) << 24);
    if(oggPageCrc(page) != storedCrc) {
        return {};
    }

    if(std::memcmp(page.constData() + packetOffset, "OpusHead", 8) != 0) {
        return {};
    }

    return OpusHeadPage{
        .page         = std::move(page),
        .packetOffset = packetOffset,
        .gainOffset   = packetOffset + 16,
    };
}

QString codecForMime(const QString& mimeType)
{
    if(mimeType == "audio/mpeg"_L1 || mimeType == "audio/mpeg3"_L1 || mimeType == "audio/x-mpeg"_L1) {
        return u"MP3"_s;
    }
    if(mimeType == "audio/x-aiff"_L1 || mimeType == "audio/x-aifc"_L1) {
        return u"AIFF"_s;
    }
    if(mimeType == "audio/vnd.wave"_L1 || mimeType == "audio/wav"_L1 || mimeType == "audio/x-wav"_L1) {
        return u"PCM"_s;
    }
    if(mimeType == "audio/x-musepack"_L1) {
        return u"MPC"_s;
    }
    if(mimeType == "audio/x-ape"_L1) {
        return u"Monkey's Audio"_s;
    }
    if(mimeType == "audio/x-wavpack"_L1) {
        return u"WavPack"_s;
    }
    if(mimeType == "audio/flac"_L1 || mimeType == "audio/x-flac"_L1) {
        return u"FLAC"_s;
    }
    if(mimeType == "audio/ogg"_L1 || mimeType == "audio/x-vorbis+ogg"_L1 || mimeType == "application/ogg"_L1) {
        return u"Vorbis"_s;
    }
    if(mimeType == "audio/opus"_L1 || mimeType == "audio/x-opus+ogg"_L1) {
        return u"Opus"_s;
    }
    if(mimeType == "audio/x-dsf"_L1 || mimeType == "audio/x-dff"_L1) {
        return u"DSD"_s;
    }

    return {};
}

void readAudioProperties(const TagLib::File& file, Track& track)
{
    if(TagLib::AudioProperties* props = file.audioProperties()) {
        const uint64_t duration = props->lengthInMilliseconds();
        const int bitrate       = props->bitrate();
        const int sampleRate    = props->sampleRate();
        const int channels      = props->channels();

        if(duration > 0) {
            track.setDuration(duration);
        }
        if(bitrate > 0) {
            track.setBitrate(bitrate);
        }
        if(sampleRate > 0) {
            track.setSampleRate(sampleRate);
        }
        if(channels > 0) {
            track.setChannels(channels);
        }
    }
}

bool hasPopulatedValue(const TagLib::StringList& value)
{
    return std::ranges::any_of(value, [](const TagLib::String& entry) { return !entry.stripWhiteSpace().isEmpty(); });
}

bool useRatingTagFallback(const RatingTagPolicy& policy)
{
    return policy.readScale == RatingScale::Automatic;
}

void readTextRatingTag(Track& track, const RatingTagPolicy& policy, const QString& policyTag, const QString& rawTag,
                       const QString& rawRating, bool ratingTagFallback)
{
    track.setRawRatingTag(rawTag, rawRating);
    if(policy.shouldReadTextTag(policyTag, track.rating() > 0)) {
        if(const auto rating = normalisedTextRating(rawRating, policy.readScale, ratingTagFallback);
           rating.has_value()) {
            track.setRating(*rating);
        }
    }
}

void readTextRatingTag(Track& track, const RatingTagPolicy& policy, const QString& tag, const QString& rawRating,
                       bool ratingTagFallback)
{
    readTextRatingTag(track, policy, tag, tag, rawRating, ratingTagFallback);
}

struct TextRatingWrite
{
    QString tag;
    QString value;
};

TextRatingWrite textRatingWrite(const Track& track, const RatingTagPolicy& policy)
{
    const QString tag = policy.effectiveWriteTag();
    if(tag.isEmpty()) {
        return {};
    }
    return {.tag = tag, .value = formatTextRating(track.rating(), policy.writeScale)};
}

void readGeneralProperties(const TagLib::PropertyMap& props, Track& track, bool skipEmptyValues, bool clearExtraTags,
                           const TagPolicy& policy)
{
    if(clearExtraTags) {
        track.clearExtraTags();
    }

    using namespace Tag;

    for(const auto& [field, value] : props) {
        if(skipEmptyValues && !hasPopulatedValue(value)) {
            continue;
        }

        if(field == Title) {
            track.setTitle(convertString(value.toString()));
        }
        else if(field == Artist) {
            track.setArtists(convertStringList(value));
        }
        else if(field == Album) {
            track.setAlbum(convertString(value.toString()));
        }
        else if(field == AlbumArtist) {
            track.setAlbumArtists(convertStringList(value));
        }
        else if(field == Genre) {
            track.setGenres(convertStringList(value));
        }
        else if(field == Composer) {
            track.setComposers(convertStringList(value));
        }
        else if(field == Performer) {
            track.setPerformers(convertStringList(value));
        }
        else if(field == Comment) {
            track.setComment(convertString(value.toString()));
        }
        else if(field == Date) {
            track.setDate(convertString(value.toString()));
        }
        else if(field == Year) {
            track.setYear(convertString(value.toString()).toInt());
        }
        else if(field == TrackNumber || field == TrackAlt) {
            track.setTrackNumber(convertString(value.toString()));
        }
        else if(field == TrackTotal || field == TrackTotalAlt) {
            track.setTrackTotal(convertString(value.toString()));
        }
        else if(field == Disc || field == DiscAlt) {
            track.setDiscNumber(convertString(value.toString()));
        }
        else if(field == DiscTotal || field == DiscTotalAlt) {
            track.setDiscTotal(convertString(value.toString()));
        }
        else if(field == Rating && !value.isEmpty()) {
            const QString rawRating = convertString(value.front());
            const QString tag       = QString::fromLatin1(Rating);
            readTextRatingTag(track, policy.rating, tag, rawRating, true);
        }
        else if(field == RatingAlt) {
            const QString rawRating = convertString(value.toString());
            const QString tag       = QString::fromLatin1(RatingAlt);
            readTextRatingTag(track, policy.rating, tag, rawRating, false);
        }
        else if(field == PlayCount) {
            const int count = convertString(value.toString()).toInt();
            if(count > 0) {
                track.setPlayCount(count);
            }
        }
        else if(field.startsWith(ReplayGain::ReplayGainStart)) {
            if(field == ReplayGain::TrackGain || field == ReplayGain::TrackGainAlt) {
                track.setRGTrackGain(gainStringToFloat(value.toString()));
            }
            else if(field == ReplayGain::AlbumGain || field == ReplayGain::AlbumGainAlt) {
                track.setRGAlbumGain(gainStringToFloat(value.toString()));
            }
            else if(field == ReplayGain::TrackPeak || field == ReplayGain::TrackPeakAlt) {
                track.setRGTrackPeak(peakStringToFloat(value.toString()));
            }
            else if(field == ReplayGain::AlbumPeak || field == ReplayGain::AlbumPeakAlt) {
                track.setRGAlbumPeak(peakStringToFloat(value.toString()));
            }
        }
        else {
            const auto tagEntry = convertString(field);
            if(!policy.rating.automaticRead() && tagEntry == policy.rating.readTag && !value.isEmpty()) {
                readTextRatingTag(track, policy.rating, policy.rating.readTag, tagEntry, convertString(value.front()),
                                  useRatingTagFallback(policy.rating));
            }
            else {
                for(const auto& tagValue : value) {
                    track.addExtraTag(tagEntry, convertString(tagValue));
                }
            }
        }
    }
}

template <typename T>
void replaceOrErase(TagLib::PropertyMap& props, const TagLib::String& key, const T& value)
{
    if(value.isEmpty()) {
        props.erase(key);
    }
    else {
        if constexpr(std::is_same_v<T, QStringList>) {
            props.replace(key, convertStringList(value));
        }
        else if constexpr(std::is_same_v<T, QString>) {
            props.replace(key, convertString(value));
        }
    }
}

void writeGenericProperties(TagLib::PropertyMap& oldProperties, const Track& track, AudioReader::WriteOptions options,
                            bool skipExtra = false)
{
    if(!track.isValid() || !(options & AudioReader::Metadata)) {
        return;
    }

    using namespace Tag;

    replaceOrErase(oldProperties, Title, track.title());
    replaceOrErase(oldProperties, Artist, track.artists());
    replaceOrErase(oldProperties, Album, track.album());
    replaceOrErase(oldProperties, AlbumArtist, track.albumArtists());
    replaceOrErase(oldProperties, Genre, track.genres());
    replaceOrErase(oldProperties, Composer, track.composer());
    replaceOrErase(oldProperties, Performer, track.performer());
    replaceOrErase(oldProperties, Comment, track.comment());
    replaceOrErase(oldProperties, Date, track.date());

    const auto handleAltProp = [&oldProperties](const QString& value, auto tag, auto tagAlt) {
        if(!value.isEmpty()) {
            if(oldProperties.contains(tagAlt)) {
                replaceOrErase(oldProperties, tagAlt, value);
            }
            else {
                replaceOrErase(oldProperties, tag, value);
            }
        }
        else {
            replaceOrErase(oldProperties, tag, QString{});
            replaceOrErase(oldProperties, tagAlt, QString{});
        }
    };

    handleAltProp(track.trackNumber(), TrackNumber, TrackAlt);
    handleAltProp(track.trackTotal(), TrackTotal, TrackTotalAlt);
    handleAltProp(track.discNumber(), Disc, DiscAlt);
    handleAltProp(track.discTotal(), DiscTotal, DiscTotalAlt);

    const auto handleRGProperty
        = [&oldProperties](bool hasProperty, float property, auto tag, auto tagAlt, bool isGain = true) {
              if(hasProperty) {
                  const QString value = isGain ? gainToString(property) : QString::number(property);
                  if(oldProperties.contains(tagAlt)) {
                      replaceOrErase(oldProperties, tagAlt, value);
                  }
                  else {
                      replaceOrErase(oldProperties, tag, value);
                  }
              }
              else {
                  replaceOrErase(oldProperties, tag, QString{});
                  replaceOrErase(oldProperties, tagAlt, QString{});
              }
          };

    // MP4 writes custom freeform tags through `writeMp4Tags()`, so the generic property writer must not
    // also emit ReplayGain or other extra tags here. Doing both would create duplicate MP4 atoms.
    if(!skipExtra) {
        handleRGProperty(track.hasTrackGain(), track.rgTrackGain(), ReplayGain::TrackGain, ReplayGain::TrackGainAlt);
        handleRGProperty(track.hasTrackPeak(), track.rgTrackPeak(), ReplayGain::TrackPeak, ReplayGain::TrackPeakAlt,
                         false);
        handleRGProperty(track.hasAlbumGain(), track.rgAlbumGain(), ReplayGain::AlbumGain, ReplayGain::AlbumGainAlt);
        handleRGProperty(track.hasAlbumPeak(), track.rgAlbumPeak(), ReplayGain::AlbumPeak, ReplayGain::AlbumPeakAlt,
                         false);

        const auto customTags = track.extraTags();
        for(const auto& [tag, values] : customTags) {
            const TagLib::String name = convertString(tag);
            oldProperties.replace(name, convertStringList(values));
        }

        const auto removedTags = track.removedTags();
        for(const auto& tag : removedTags) {
            const TagLib::String name = convertString(tag);
            oldProperties.erase(name);
        }
    }
}

QString getTrackNumber(const Track& track)
{
    QString trackNumber = track.trackNumber();
    if(!track.trackTotal().isEmpty()) {
        trackNumber += u"/"_s + track.trackTotal();
    }
    return trackNumber;
}

QString getDiscNumber(const Track& track)
{
    QString discNumber = track.discNumber();
    if(!track.discTotal().isEmpty()) {
        discNumber += u"/"_s + track.discTotal();
    }
    return discNumber;
}

void readTrackTotalPair(const QString& trackNumbers, Track& track)
{
    const qsizetype splitIdx = trackNumbers.indexOf("/"_L1);
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
    const qsizetype splitIdx = discNumbers.indexOf("/"_L1);
    if(splitIdx >= 0) {
        track.setDiscNumber(discNumbers.first(splitIdx));
        track.setDiscTotal(discNumbers.sliced(splitIdx + 1));
    }
    else if(discNumbers.size() > 0) {
        track.setDiscNumber(discNumbers);
    }
}

TagLib::ID3v2::UserTextIdentificationFrame* userTextFrame(TagLib::ID3v2::Tag* id3Tags,
                                                          const TagLib::String& description)
{
    return TagLib::ID3v2::UserTextIdentificationFrame::find(id3Tags, description);
}

void removeUserTextFrame(TagLib::ID3v2::Tag* id3Tags, const TagLib::String& description)
{
    while(auto* frame = userTextFrame(id3Tags, description)) {
        id3Tags->removeFrame(frame, true);
    }
}

void writeId3TextRating(TagLib::ID3v2::Tag* id3Tags, const TextRatingWrite& rating)
{
    id3Tags->removeFrames("FMPS_Rating");
    removeUserTextFrame(id3Tags, "RATING");
    if(!rating.tag.isEmpty() && rating.tag != "FMPS_RATING"_L1 && rating.tag != "RATING"_L1) {
        removeUserTextFrame(id3Tags, convertString(rating.tag));
    }

    if(rating.tag.isEmpty() || rating.value.isEmpty()) {
        return;
    }

    if(rating.tag == "FMPS_RATING"_L1) {
        auto ratingFrame
            = std::make_unique<TagLib::ID3v2::TextIdentificationFrame>("FMPS_Rating", TagLib::String::UTF8);
        ratingFrame->setText(convertString(rating.value));
        id3Tags->addFrame(ratingFrame.release());
    }
    else if(rating.tag == "RATING"_L1) {
        auto ratingFrame = std::make_unique<TagLib::ID3v2::UserTextIdentificationFrame>(TagLib::String::UTF8);
        ratingFrame->setDescription("RATING");
        ratingFrame->setText(convertString(rating.value));
        id3Tags->addFrame(ratingFrame.release());
    }
    else {
        removeUserTextFrame(id3Tags, convertString(rating.tag));
        auto ratingFrame = std::make_unique<TagLib::ID3v2::UserTextIdentificationFrame>(TagLib::String::UTF8);
        ratingFrame->setDescription(convertString(rating.tag));
        ratingFrame->setText(convertString(rating.value));
        id3Tags->addFrame(ratingFrame.release());
    }
}

void removePopmFrame(TagLib::ID3v2::Tag* id3Tags, const TagLib::String& owner)
{
    const TagLib::ID3v2::FrameListMap& map = id3Tags->frameListMap();
    if(!map.contains("POPM")) {
        return;
    }

    for(auto* popmFrame : map["POPM"]) {
        const auto* ratingFrame = dynamic_cast<TagLib::ID3v2::PopularimeterFrame*>(popmFrame);
        if(ratingFrame && (owner.isEmpty() || ratingFrame->email() == owner)) {
            id3Tags->removeFrame(popmFrame, true);
            return;
        }
    }
}

void readId3Tags(const TagLib::ID3v2::Tag* id3Tags, Track& track, const TagPolicy& policy, bool id3PopmSupported)
{
    if(id3Tags->isEmpty()) {
        return;
    }

    const TagLib::ID3v2::FrameListMap& frames = id3Tags->frameListMap();
    const unsigned int majorVersion           = id3Tags->header()->majorVersion();

    if(majorVersion == 4) {
        // TDRC replaces TYER
        if(frames.contains("TDRC") && frames.contains("TYER")) {
            const TagLib::ID3v2::FrameList& dateFrame = frames["TDRC"];
            if(!dateFrame.isEmpty()) {
                track.setDate(convertString(dateFrame.front()->toString()));
            }
        }
    }
    else if(majorVersion == 3) {
        // Handle '/' being used as a separator for artists in v2.3
        if(frames.contains("TPE1")) {
            const TagLib::ID3v2::FrameList& artistsFrame = frames["TPE1"];
            if(!artistsFrame.isEmpty()) {
                const QString artist = convertString(artistsFrame.front()->toString());
                // Ignore common artist names
                if(artist.contains("/"_L1) && !artist.contains("AC/DC"_L1) && !artist.contains("AC / DC"_L1)) {
                    QStringList artists = artist.split(u'/', Qt::SkipEmptyParts);
                    std::ranges::transform(artists, artists.begin(), [](QString& entry) { return entry.trimmed(); });
                    track.setArtists(artists);
                }
            }
        }
    }

    if(frames.contains("TRCK")) {
        const TagLib::ID3v2::FrameList& trackFrame = frames["TRCK"];
        if(!trackFrame.isEmpty()) {
            const QString trackNumbers = convertString(trackFrame.front()->toString());
            readTrackTotalPair(trackNumbers, track);
        }
    }

    if(frames.contains("TPOS")) {
        const TagLib::ID3v2::FrameList& discFrame = frames["TPOS"];
        if(!discFrame.isEmpty()) {
            const QString discNumbers = convertString(discFrame.front()->toString());
            readDiscTotalPair(discNumbers, track);
        }
    }

    if(frames.contains("FMPS_Rating")) {
        const TagLib::ID3v2::FrameList& ratingFrame = frames["FMPS_Rating"];
        if(!ratingFrame.isEmpty()) {
            const QString rawRating = convertString(ratingFrame.front()->toString());
            readTextRatingTag(track, policy.rating, u"FMPS_RATING"_s, u"FMPS_Rating"_s, rawRating, false);
            track.setRawRatingTag(u"FMPS_RATING"_s, rawRating);
        }
    }

    if(frames.contains("FMPS_Playcount")) {
        const TagLib::ID3v2::FrameList& countFrame = frames["FMPS_Playcount"];
        if(!countFrame.isEmpty()) {
            const int count = convertString(countFrame.front()->toString()).toInt();
            if(count > 0) {
                track.setPlayCount(count);
            }
        }
    }

    if(id3PopmSupported && policy.rating.readId3Popm && frames.contains("POPM")) {
        // Use only first rating
        const TagLib::ID3v2::FrameList& popmFrames = frames["POPM"];
        if(!popmFrames.isEmpty()) {
            auto* ratingFrame = dynamic_cast<TagLib::ID3v2::PopularimeterFrame*>(popmFrames.front());
            if(!policy.rating.popmOwner.isEmpty()) {
                const TagLib::String owner = convertString(policy.rating.popmOwner);
                const auto frameIt         = std::ranges::find_if(popmFrames, [&owner](auto* frame) {
                    const auto* popmFrame = dynamic_cast<TagLib::ID3v2::PopularimeterFrame*>(frame);
                    return popmFrame && popmFrame->email() == owner;
                });
                if(frameIt != popmFrames.end()) {
                    ratingFrame = dynamic_cast<TagLib::ID3v2::PopularimeterFrame*>(*frameIt);
                }
            }

            if(ratingFrame) {
                const QString rawRating = u"%1|%2|%3"_s.arg(convertString(ratingFrame->email()))
                                              .arg(ratingFrame->rating())
                                              .arg(ratingFrame->counter());
                track.setRawRatingTag(u"POPM"_s, rawRating);
                if(track.playCount() <= 0 && ratingFrame->counter() > 0) {
                    track.setPlayCount(static_cast<int>(ratingFrame->counter()));
                }
                if(track.rating() <= 0 && ratingFrame->rating() > 0) {
                    track.setRating(popmToRating(ratingFrame->rating(), policy.rating.popmMapping));
                }
            }
        }
    }
}

QByteArray readId3Cover(const TagLib::ID3v2::Tag* id3Tags, Track::Cover cover)
{
    if(id3Tags->isEmpty()) {
        return {};
    }

    const TagLib::ID3v2::FrameList& frames = id3Tags->frameListMap()["APIC"];

    using PictureFrame = TagLib::ID3v2::AttachedPictureFrame;

    TagLib::ByteVector picture;

    for(const auto& frame : std::as_const(frames)) {
        const auto* coverFrame        = static_cast<PictureFrame*>(frame);
        const PictureFrame::Type type = coverFrame->type();

        if((cover == Track::Cover::Front && (type == PictureFrame::FrontCover || type == PictureFrame::Other))
           || (cover == Track::Cover::Back && type == PictureFrame::BackCover)
           || (cover == Track::Cover::Artist && type == PictureFrame::Artist)) {
            picture = coverFrame->picture();
        }
    }

    if(picture.isEmpty()) {
        return {};
    }

    return {picture.data(), static_cast<qsizetype>(picture.size())};
}

void writeID3v2Tags(TagLib::ID3v2::Tag* id3Tags, const Track& track, AudioReader::WriteOptions options,
                    const TagPolicy& policy, bool id3PopmSupported)
{
    if(options & AudioReader::Metadata) {
        id3Tags->removeFrames("TRCK");

        const QString trackNumber = getTrackNumber(track);
        if(!trackNumber.isEmpty()) {
            auto trackFrame = std::make_unique<TagLib::ID3v2::TextIdentificationFrame>("TRCK", TagLib::String::UTF8);
            trackFrame->setText(convertString(trackNumber));
            id3Tags->addFrame(trackFrame.release());
        }

        id3Tags->removeFrames("TPOS");

        const QString discNumber = getDiscNumber(track);
        if(!discNumber.isEmpty()) {
            auto discFrame = std::make_unique<TagLib::ID3v2::TextIdentificationFrame>("TPOS", TagLib::String::UTF8);
            discFrame->setText(convertString(discNumber));
            id3Tags->addFrame(discFrame.release());
        }
    }

    if(options & AudioReader::Rating) {
        writeId3TextRating(id3Tags, textRatingWrite(track, policy.rating));

        if(id3PopmSupported && policy.rating.writeId3Popm) {
            const TagLib::String owner = convertString(policy.rating.popmOwner);
            if(track.rating() <= 0.0F) {
                removePopmFrame(id3Tags, owner);
                return;
            }

            TagLib::ID3v2::PopularimeterFrame* frame{nullptr};
            const TagLib::ID3v2::FrameListMap& map = id3Tags->frameListMap();
            if(map.contains("POPM")) {
                const TagLib::ID3v2::FrameList& popmFrames = map["POPM"];
                const auto frameIt = std::ranges::find_if(popmFrames, [&owner](auto* popmFrame) {
                    const auto* ratingFrame = dynamic_cast<TagLib::ID3v2::PopularimeterFrame*>(popmFrame);
                    return ratingFrame && (owner.isEmpty() || ratingFrame->email() == owner);
                });
                if(frameIt != popmFrames.end()) {
                    frame = dynamic_cast<TagLib::ID3v2::PopularimeterFrame*>(*frameIt);
                }
            }

            if(!frame) {
                frame = new TagLib::ID3v2::PopularimeterFrame();
                frame->setEmail(owner);
                id3Tags->addFrame(frame);
            }
            else {
                frame->setEmail(owner);
            }
            frame->setRating(ratingToPopm(track.rating(), policy.rating.popmMapping));
        }
    }

    if(options & AudioReader::Playcount) {
        id3Tags->removeFrames("FMPS_Playcount");

        const auto count = QString::number(track.playCount());
        auto countFrame
            = std::make_unique<TagLib::ID3v2::TextIdentificationFrame>("FMPS_Playcount", TagLib::String::UTF8);
        countFrame->setText(convertString(count));
        id3Tags->addFrame(countFrame.release());

        TagLib::ID3v2::PopularimeterFrame* frame{nullptr};
        const TagLib::ID3v2::FrameListMap& map = id3Tags->frameListMap();
        if(map.contains("POPM")) {
            frame = dynamic_cast<TagLib::ID3v2::PopularimeterFrame*>(map["POPM"].front());
        }

        if(!frame) {
            frame = new TagLib::ID3v2::PopularimeterFrame();
            id3Tags->addFrame(frame);
        }

        frame->setCounter(static_cast<unsigned int>(track.playCount()));
    }
}

bool writeId3Cover(TagLib::ID3v2::Tag* id3Tags, const TrackCovers& covers)
{
    using PictureFrame = TagLib::ID3v2::AttachedPictureFrame;

    const auto toCoverType = [](PictureFrame::Type type) {
        switch(type) {
            case PictureFrame::FrontCover:
                return Track::Cover::Front;
            case PictureFrame::BackCover:
                return Track::Cover::Back;
            case PictureFrame::Artist:
                return Track::Cover::Artist;
            default:
                return Track::Cover::Other;
        }
    };

    const auto fromCoverType = [](Track::Cover type) {
        switch(type) {
            case Track::Cover::Front:
                return PictureFrame::FrontCover;
            case Track::Cover::Back:
                return PictureFrame::BackCover;
            case Track::Cover::Artist:
                return PictureFrame::Artist;
            default:
                return PictureFrame::Other;
        }
    };

    bool modified{false};

    auto frames = id3Tags->frameList("APIC");
    for(auto* frame : frames) {
        auto* coverFrame = static_cast<PictureFrame*>(frame);
        if(coverFrame && covers.contains(toCoverType(coverFrame->type()))) {
            id3Tags->removeFrame(coverFrame);
            modified = true;
        }
    }

    for(const auto& [type, cover] : covers) {
        if(!cover.data.isEmpty()) {
            auto* newCoverFrame = new PictureFrame();
            newCoverFrame->setType(fromCoverType(type));
            newCoverFrame->setMimeType(convertString(cover.mimeType));
            newCoverFrame->setPicture(TagLib::ByteVector(cover.data.constData(), cover.data.size()));
            id3Tags->addFrame(newCoverFrame);
            modified = true;
        }
    }

    return modified;
}

void readApeTags(const TagLib::APE::Tag* apeTags, Track& track, const TagPolicy& policy)
{
    if(apeTags->isEmpty()) {
        return;
    }

    const TagLib::APE::ItemListMap& items = apeTags->itemListMap();

    if(items.contains("TRACK")) {
        const TagLib::APE::Item& trackItem = items["TRACK"];
        if(!trackItem.isEmpty() && !trackItem.values().isEmpty()) {
            const QString trackNumbers = convertString(trackItem.values().front());
            readTrackTotalPair(trackNumbers, track);
        }
    }

    if(items.contains("DISC")) {
        const TagLib::APE::Item& discItem = items["DISC"];
        if(!discItem.isEmpty() && !discItem.values().isEmpty()) {
            const QString discNumbers = convertString(discItem.values().front());
            readDiscTotalPair(discNumbers, track);
        }
    }

    if(items.contains("FMPS_RATING")) {
        const QString rawRating = convertString(items["FMPS_RATING"].toString());
        readTextRatingTag(track, policy.rating, u"FMPS_RATING"_s, rawRating, false);
    }

    if(items.contains("FMPS_PLAYCOUNT")) {
        const int count = convertString(items["FMPS_PLAYCOUNT"].toString()).toInt();
        if(count > 0) {
            track.setPlayCount(count);
        }
    }
}

QByteArray readApeCover(const TagLib::APE::Tag* apeTags, Track::Cover cover)
{
    if(apeTags->isEmpty()) {
        return {};
    }

    const TagLib::APE::ItemListMap& items = apeTags->itemListMap();

    const TagLib::String coverType = cover == Track::Cover::Front ? "COVER ART (FRONT)"
                                   : cover == Track::Cover::Back  ? "COVER ART (BACK)"
                                                                  : "COVER ART (ARTIST)";

    auto itemIt = items.find(coverType);

    if(itemIt != items.end()) {
        const auto& picture = itemIt->second.binaryData();
        int position        = picture.find('\0');
        if(position >= 0) {
            position += 1;
            return {picture.data() + position, static_cast<qsizetype>(picture.size() - position)};
        }
    }
    return {};
}

void writeApeTags(TagLib::APE::Tag* apeTags, const Track& track, AudioReader::WriteOptions options,
                  const TagPolicy& policy)
{
    if(options & AudioReader::Metadata) {
        const QString trackNumber = getTrackNumber(track);
        if(trackNumber.isEmpty()) {
            apeTags->removeItem("TRACK");
        }
        else {
            apeTags->addValue("TRACK", convertString(trackNumber), true);
        }

        const QString discNumber = getDiscNumber(track);
        if(discNumber.isEmpty()) {
            apeTags->removeItem("DISC");
        }
        else {
            apeTags->addValue("DISC", convertString(discNumber), true);
        }
    }

    if(options & AudioReader::Rating) {
        apeTags->removeItem("FMPS_RATING");
        apeTags->removeItem("RATING");

        const auto rating = textRatingWrite(track, policy.rating);
        if(!rating.tag.isEmpty()) {
            const TagLib::String tag = convertString(rating.tag);
            apeTags->removeItem(tag);
            if(!rating.value.isEmpty()) {
                apeTags->setItem(tag, {tag, convertString(rating.value)});
            }
        }
    }

    if(options & AudioReader::Playcount) {
        if(track.playCount() <= 0) {
            apeTags->removeItem("FMPS_PLAYCOUNT");
        }
        else {
            apeTags->setItem("FMPS_PLAYCOUNT", {"FMPS_PLAYCOUNT", convertString(QString::number(track.playCount()))});
        }
    }
}

bool writeApeCover(TagLib::APE::Tag* apeTags, const TrackCovers& covers)
{
    const auto fromCoverType = [](Track::Cover type) {
        switch(type) {
            case Track::Cover::Front:
                return "COVER ART (FRONT)";
            case Track::Cover::Back:
                return "COVER ART (BACK)";
            case Track::Cover::Artist:
                return "COVER ART (ARTIST)";
            default:
                return "";
        }
    };

    bool modified{false};

    for(const auto& [type, cover] : covers) {
        const auto* const coverType = fromCoverType(type);

        if(apeTags->itemListMap().contains(coverType)) {
            apeTags->removeItem(coverType);
            modified = true;
        }

        if(!cover.data.isEmpty()) {
            TagLib::ByteVector coverData;
            coverData.append(coverType);
            coverData.append('\0');
            coverData.append(TagLib::ByteVector(cover.data.constData(), cover.data.size()));

            const TagLib::APE::Item newItem(coverType, coverData, true);
            apeTags->setItem(coverType, newItem);
            modified = true;
        }
    }

    return modified;
}

QString stripMp4FreeFormName(const TagLib::String& name)
{
    QString freeFormName = convertString(name);

    if(freeFormName.startsWith("----"_L1)) {
        qsizetype nameStart = freeFormName.lastIndexOf(":"_L1);
        if(nameStart == -1) {
            nameStart = 5;
        }
        else {
            ++nameStart;
        }
        freeFormName = freeFormName.sliced(nameStart);
    }

    return freeFormName;
}

void readMp4Tags(const TagLib::MP4::Tag* mp4Tags, Track& track, const TagPolicy& policy, bool skipExtra = false)
{
    if(mp4Tags->isEmpty()) {
        return;
    }

    const auto& items = mp4Tags->itemMap();

    if(items.contains(Mp4::PerformerAlt)) {
        const auto performer = items[Mp4::PerformerAlt].toStringList();
        if(performer.size() > 0) {
            track.setPerformers(convertStringList(performer));
        }
    }

    if(items.contains(Mp4::TrackNumber)) {
        const TagLib::MP4::Item::IntPair& trackNumbers = items[Mp4::TrackNumber].toIntPair();
        if(trackNumbers.first > 0) {
            track.setTrackNumber(QString::number(trackNumbers.first));
        }
        if(trackNumbers.second > 0) {
            track.setTrackTotal(QString::number(trackNumbers.second));
        }
    }
    if(items.contains(Fooyin::Mp4::TrackAlt)) {
        const auto& trackNumber = items[Fooyin::Mp4::TrackAlt].toStringList();
        if(trackNumber.size() > 0) {
            track.setTrackNumber(convertString(trackNumber.toString()));
        }
    }
    if(items.contains(Fooyin::Mp4::TrackTotal)) {
        const auto& trackTotal = items[Fooyin::Mp4::TrackTotal].toStringList();
        if(trackTotal.size() > 0) {
            track.setTrackTotal(convertString(trackTotal.toString()));
        }
    }
    if(items.contains(Fooyin::Mp4::TrackTotalAlt)) {
        const auto& trackTotal = items[Fooyin::Mp4::TrackTotalAlt].toStringList();
        if(trackTotal.size() > 0) {
            track.setTrackTotal(convertString(trackTotal.toString()));
        }
    }

    if(items.contains(Fooyin::Mp4::Disc)) {
        const TagLib::MP4::Item::IntPair& discNumbers = items[Fooyin::Mp4::Disc].toIntPair();
        if(discNumbers.first > 0) {
            track.setDiscNumber(QString::number(discNumbers.first));
        }
        if(discNumbers.second > 0) {
            track.setDiscTotal(QString::number(discNumbers.second));
        }
    }
    if(items.contains(Fooyin::Mp4::DiscAlt)) {
        const auto& discNumber = items[Fooyin::Mp4::DiscAlt].toStringList();
        if(discNumber.size() > 0) {
            track.setDiscNumber(convertString(discNumber.toString()));
        }
    }
    if(items.contains(Fooyin::Mp4::DiscTotal)) {
        const auto& discTotal = items[Fooyin::Mp4::DiscTotal].toStringList();
        if(discTotal.size() > 0) {
            track.setDiscTotal(convertString(discTotal.toString()));
        }
    }
    if(items.contains(Fooyin::Mp4::DiscTotalAlt)) {
        const auto& discTotal = items[Fooyin::Mp4::DiscTotalAlt].toStringList();
        if(discTotal.size() > 0) {
            track.setDiscTotal(convertString(discTotal.toString()));
        }
    }

    auto convertRating = [](int rating) -> float {
        if(rating < 20) {
            return 0.0;
        }
        if(rating < 40) {
            return 0.2;
        }
        if(rating < 60) {
            return 0.4;
        }
        if(rating < 80) {
            return 0.6;
        }
        if(rating < 100) {
            return 0.8;
        }

        return 1.0;
    };

    if(items.contains(Fooyin::Mp4::Rating) && track.rating() <= 0) {
        const int rating = items[Fooyin::Mp4::Rating].toInt();
        track.setRawRatingTag(u"rate"_s, QString::number(rating));
        track.setRating(convertRating(rating));
    }

    if(items.contains(Fooyin::Mp4::RatingAlt)) {
        const QString rawRating = convertString(items[Fooyin::Mp4::RatingAlt].toStringList().toString("\n"));
        readTextRatingTag(track, policy.rating, u"FMPS_RATING"_s, QString::fromLatin1(Fooyin::Mp4::RatingAlt),
                          rawRating, false);
        track.setRawRatingTag(u"FMPS_RATING"_s, rawRating);
    }

    if(items.contains(Fooyin::Mp4::RatingAlt2)) {
        const QString rawRating = convertString(items[Fooyin::Mp4::RatingAlt2].toStringList().toString("\n"));
        readTextRatingTag(track, policy.rating, u"RATING"_s, QString::fromLatin1(Fooyin::Mp4::RatingAlt2), rawRating,
                          true);
        track.setRawRatingTag(u"RATING"_s, rawRating);
    }

    if(items.contains(Fooyin::Mp4::PlayCount) && track.playCount() <= 0) {
        const int count = items[Fooyin::Mp4::PlayCount].toInt();
        if(count > 0) {
            track.setPlayCount(count);
        }
    }

    if(items.contains(Fooyin::Mp4::ReplayGain::TrackGain)) {
        track.setRGTrackGain(
            gainStringToFloat(items[Fooyin::Mp4::ReplayGain::TrackGain].toStringList().toString("\n")));
    }
    if(items.contains(Fooyin::Mp4::ReplayGain::TrackGainAlt)) {
        track.setRGTrackGain(
            gainStringToFloat(items[Fooyin::Mp4::ReplayGain::TrackGainAlt].toStringList().toString("\n")));
    }

    if(items.contains(Fooyin::Mp4::ReplayGain::TrackPeak)) {
        track.setRGTrackPeak(
            peakStringToFloat(items[Fooyin::Mp4::ReplayGain::TrackPeak].toStringList().toString("\n")));
    }
    if(items.contains(Fooyin::Mp4::ReplayGain::TrackPeakAlt)) {
        track.setRGTrackPeak(
            peakStringToFloat(items[Fooyin::Mp4::ReplayGain::TrackPeakAlt].toStringList().toString("\n")));
    }

    if(items.contains(Fooyin::Mp4::ReplayGain::AlbumGain)) {
        track.setRGAlbumGain(
            gainStringToFloat(items[Fooyin::Mp4::ReplayGain::AlbumGain].toStringList().toString("\n")));
    }
    if(items.contains(Fooyin::Mp4::ReplayGain::AlbumGainAlt)) {
        track.setRGAlbumGain(
            gainStringToFloat(items[Fooyin::Mp4::ReplayGain::AlbumGainAlt].toStringList().toString("\n")));
    }

    if(items.contains(Fooyin::Mp4::ReplayGain::AlbumPeak)) {
        track.setRGAlbumPeak(
            peakStringToFloat(items[Fooyin::Mp4::ReplayGain::AlbumPeak].toStringList().toString("\n")));
    }
    if(items.contains(Fooyin::Mp4::ReplayGain::AlbumPeakAlt)) {
        track.setRGAlbumPeak(
            peakStringToFloat(items[Fooyin::Mp4::ReplayGain::AlbumPeakAlt].toStringList().toString("\n")));
    }

    if(!skipExtra) {
        using namespace Fooyin::Mp4;
        static const std::set<TagLib::String> baseMp4Tags = {
            Title,
            Artist,
            Album,
            AlbumArtist,
            Genre,
            Composer,
            Performer,
            PerformerAlt,
            Comment,
            Date,
            Rating,
            RatingAlt,
            RatingAlt2,
            PlayCount,
            TrackNumber,
            TrackAlt,
            TrackTotal,
            TrackTotalAlt,
            Disc,
            DiscAlt,
            DiscTotal,
            DiscTotalAlt,
            ReplayGain::AlbumGain,
            ReplayGain::AlbumGainAlt,
            ReplayGain::AlbumPeak,
            ReplayGain::AlbumPeakAlt,
            ReplayGain::TrackGain,
            ReplayGain::TrackGainAlt,
            ReplayGain::TrackPeak,
            ReplayGain::TrackPeakAlt,
        };

        track.clearExtraTags();

        for(const auto& [key, item] : items) {
            if(!baseMp4Tags.contains(key)) {
                QString tagName = findMp4Tag(key);
                if(tagName.isEmpty()) {
                    tagName = stripMp4FreeFormName(key);
                }
                const auto values = convertStringList(item.toStringList());
                if(!policy.rating.automaticRead() && tagName == policy.rating.readTag && !values.isEmpty()) {
                    readTextRatingTag(track, policy.rating, policy.rating.readTag, tagName, values.front(),
                                      useRatingTagFallback(policy.rating));
                }
                else {
                    for(const auto& value : values) {
                        track.addExtraTag(tagName, value);
                    }
                }
            }
        }
    }
}

QByteArray readMp4Cover(const TagLib::MP4::Tag* mp4Tags, Fooyin::Track::Cover cover)
{
    if(cover != Fooyin::Track::Cover::Front) {
        // Only front cover is supported for now
        return {};
    }

    const TagLib::MP4::Item coverArtItem = mp4Tags->item(Fooyin::Mp4::Cover);
    if(!coverArtItem.isValid()) {
        return {};
    }

    const TagLib::MP4::CoverArtList coverArtList = coverArtItem.toCoverArtList();

    if(!coverArtList.isEmpty()) {
        const TagLib::MP4::CoverArt& coverArt = coverArtList.front();
        return {coverArt.data().data(), coverArt.data().size()};
    }
    return {};
}

TagLib::String prefixMp4FreeFormName(const QString& name, const TagLib::MP4::ItemMap& items)
{
    if(name.isEmpty()) {
        return {};
    }

    if(name.startsWith("----"_L1) || (name.length() == 4 && name[0] == u'\251')) {
        return {};
    }

    TagLib::String tagKey = convertString(name);

    if(name[0] == u':') {
        tagKey = tagKey.substr(1);
    }

    TagLib::String freeFormName = "----:com.apple.iTunes:" + tagKey;

    const int len = static_cast<int>(name.length());
    // See if we can find another prefix
    for(const auto& [key, value] : items) {
        if(std::cmp_greater_equal(key.length(), len) && key.substr(key.length() - len, len) == tagKey) {
            freeFormName = key;
            break;
        }
    }

    return freeFormName;
}

void writeMp4Tags(TagLib::MP4::Tag* mp4Tags, const Track& track, AudioReader::WriteOptions options,
                  const TagPolicy& policy)
{
    if(options & Fooyin::AudioReader::Metadata) {
        const QString trackNumber = track.trackNumber();
        const QString trackTotal  = track.trackTotal();

        mp4Tags->removeItem(Mp4::TrackNumber);
        mp4Tags->removeItem(Mp4::TrackAlt);
        mp4Tags->removeItem(Mp4::TrackTotal);
        mp4Tags->removeItem(Mp4::TrackTotalAlt);

        if(!trackNumber.isEmpty() || !trackTotal.isEmpty()) {
            bool numOk{false};
            const int num = trackNumber.toInt(&numOk);
            bool totalOk{false};
            const int total = trackTotal.toInt(&totalOk);

            if(numOk && totalOk) {
                mp4Tags->setItem(Mp4::TrackNumber, {num, total});
            }
            else {
                if(!trackNumber.isEmpty()) {
                    mp4Tags->setItem(Mp4::TrackAlt, {convertString(trackNumber)});
                }
                if(!trackTotal.isEmpty()) {
                    mp4Tags->setItem(Mp4::TrackTotalAlt, {convertString(trackTotal)});
                }
            }
        }

        const QString discNumber = track.discNumber();
        const QString discTotal  = track.discTotal();

        mp4Tags->removeItem(Mp4::Disc);
        mp4Tags->removeItem(Mp4::DiscAlt);
        mp4Tags->removeItem(Mp4::DiscTotal);
        mp4Tags->removeItem(Mp4::DiscTotalAlt);

        if(!discNumber.isEmpty() || !discTotal.isEmpty()) {
            bool numOk{false};
            const int num = discNumber.toInt(&numOk);
            bool totalOk{false};
            const int total = discTotal.toInt(&totalOk);

            if(numOk && totalOk) {
                mp4Tags->setItem(Mp4::Disc, {num, total});
            }
            else {
                if(!discNumber.isEmpty()) {
                    mp4Tags->setItem(Mp4::DiscAlt, {convertString(discNumber)});
                }
                if(!discTotal.isEmpty()) {
                    mp4Tags->setItem(Mp4::DiscTotalAlt, {convertString(discTotal)});
                }
            }
        }

        mp4Tags->setItem(Mp4::PerformerAlt, {convertString(track.performer())});
    }

    if(options & AudioReader::Rating) {
        mp4Tags->removeItem(Mp4::RatingAlt);
        mp4Tags->removeItem(Mp4::RatingAlt2);

        const auto rating = textRatingWrite(track, policy.rating);
        if(!rating.tag.isEmpty()) {
            if(rating.tag == "FMPS_RATING"_L1) {
                if(!rating.value.isEmpty()) {
                    mp4Tags->setItem(Mp4::RatingAlt, {convertString(rating.value)});
                }
            }
            else if(rating.tag == "RATING"_L1) {
                if(!rating.value.isEmpty()) {
                    mp4Tags->setItem(Mp4::RatingAlt2, {convertString(rating.value)});
                }
            }
            else {
                const TagLib::String tag = prefixMp4FreeFormName(rating.tag, mp4Tags->itemMap());
                if(!tag.isEmpty()) {
                    mp4Tags->removeItem(tag);
                    if(!rating.value.isEmpty()) {
                        mp4Tags->setItem(tag, {convertString(rating.value)});
                    }
                }
            }
        }
    }

    if(options & AudioReader::Playcount) {
        if(track.playCount() <= 0) {
            mp4Tags->removeItem(Mp4::PlayCount);
        }
        else {
            mp4Tags->setItem(Mp4::PlayCount, {convertString(QString::number(track.playCount()))});
        }
    }

    if(options & Fooyin::AudioReader::Metadata) {
        mp4Tags->removeItem(Fooyin::Mp4::ReplayGain::AlbumGain);
        mp4Tags->removeItem(Fooyin::Mp4::ReplayGain::AlbumGainAlt);
        mp4Tags->removeItem(Fooyin::Mp4::ReplayGain::AlbumPeak);
        mp4Tags->removeItem(Fooyin::Mp4::ReplayGain::AlbumPeakAlt);
        mp4Tags->removeItem(Fooyin::Mp4::ReplayGain::TrackGain);
        mp4Tags->removeItem(Fooyin::Mp4::ReplayGain::TrackGainAlt);
        mp4Tags->removeItem(Fooyin::Mp4::ReplayGain::TrackPeak);
        mp4Tags->removeItem(Fooyin::Mp4::ReplayGain::TrackPeakAlt);

        if(track.hasTrackGain()) {
            mp4Tags->setItem(Fooyin::Mp4::ReplayGain::TrackGain, {convertString(gainToString(track.rgTrackGain()))});
        }
        if(track.hasTrackPeak()) {
            mp4Tags->setItem(Fooyin::Mp4::ReplayGain::TrackPeak, {convertString(QString::number(track.rgTrackPeak()))});
        }
        if(track.hasAlbumGain()) {
            mp4Tags->setItem(Fooyin::Mp4::ReplayGain::AlbumGain, {convertString(gainToString(track.rgAlbumGain()))});
        }
        if(track.hasAlbumPeak()) {
            mp4Tags->setItem(Fooyin::Mp4::ReplayGain::AlbumPeak, {convertString(QString::number(track.rgAlbumPeak()))});
        }

        using namespace Fooyin::Tag;
        static const std::set<QString> baseMp4Tags
            = {QString::fromLatin1(Title),         QString::fromLatin1(Artist),
               QString::fromLatin1(Album),         QString::fromLatin1(AlbumArtist),
               QString::fromLatin1(Genre),         QString::fromLatin1(Composer),
               QString::fromLatin1(Performer),     QString::fromLatin1(Comment),
               QString::fromLatin1(Date),          QString::fromLatin1(Rating),
               QString::fromLatin1(RatingAlt),     QString::fromLatin1(Fooyin::Mp4::RatingAlt2),
               QString::fromLatin1(PlayCount),     QString::fromLatin1(TrackNumber),
               QString::fromLatin1(TrackAlt),      QString::fromLatin1(TrackTotal),
               QString::fromLatin1(TrackTotalAlt), QString::fromLatin1(Disc),
               QString::fromLatin1(DiscAlt),       QString::fromLatin1(DiscTotal),
               QString::fromLatin1(DiscTotalAlt)};

        const auto customTags = track.extraTags();
        for(const auto& [tag, values] : customTags) {
            if(!baseMp4Tags.contains(tag)) {
                TagLib::String tagName = findMp4Tag(tag);
                if(tagName.isEmpty()) {
                    tagName = prefixMp4FreeFormName(tag, mp4Tags->itemMap());
                }
                mp4Tags->setItem(tagName, convertStringList(values));
            }
        }

        const auto removedTags = track.removedTags();
        for(const auto& tag : removedTags) {
            TagLib::String tagName = findMp4Tag(tag);
            if(tagName.isEmpty()) {
                tagName = prefixMp4FreeFormName(tag, mp4Tags->itemMap());
            }
            mp4Tags->removeItem(tagName);
        }
    }
}

bool writeMp4Cover(TagLib::MP4::Tag* mp4Tags, const Fooyin::TrackCovers& covers)
{
    const auto coverIt = covers.find(Fooyin::Track::Cover::Front);
    if(coverIt == covers.end()) {
        return false;
    }

    const auto& cover = coverIt->second;

    bool modified{false};

    using CoverArt = TagLib::MP4::CoverArt;
    using Format   = CoverArt::Format;

    if(cover.data.isEmpty()) {
        if(mp4Tags->contains("covr")) {
            mp4Tags->removeItem("covr");
            return true;
        }
        return false;
    }

    QByteArray coverData{cover.data};
    Format format{CoverArt::JPEG};
    bool supportedFormat{true};

    if(cover.mimeType == "image/jpeg"_L1 || cover.mimeType == "image/jpg"_L1) {
        format = Format::JPEG;
    }
    else if(cover.mimeType == "image/png"_L1) {
        format = Format::PNG;
    }
    else if(cover.mimeType == "image/bmp"_L1 || cover.mimeType == "image/x-ms-bmp"_L1) {
        format = Format::BMP;
    }
    else if(cover.mimeType == "image/gif"_L1) {
        format = Format::GIF;
    }
    else {
        supportedFormat = false;
    }

    if(!supportedFormat) {
        QImage image;
        if(!image.loadFromData(coverData)) {
            qCWarning(TAGLIB) << "Unsupported MP4 cover format and failed to decode image:" << cover.mimeType;
            return false;
        }

        QByteArray convertedData;
        QBuffer outputBuffer{&convertedData};
        if(!outputBuffer.open(QIODevice::WriteOnly) || !image.save(&outputBuffer, "JPG", 90)) {
            qCWarning(TAGLIB) << "Failed to convert MP4 cover image to JPEG from format:" << cover.mimeType;
            return false;
        }

        coverData = convertedData;
        format    = Format::JPEG;
    }

    if(mp4Tags->contains("covr")) {
        mp4Tags->removeItem("covr");
        modified = true;
    }

    const CoverArt newCoverArt(format, TagLib::ByteVector(coverData.constData(), coverData.size()));
    TagLib::MP4::CoverArtList coverArtList;
    coverArtList.append(newCoverArt);
    mp4Tags->setItem("covr", TagLib::MP4::Item(coverArtList));
    modified = true;

    return modified;
}

void readXiphComment(const TagLib::Ogg::XiphComment* xiphTags, Fooyin::Track& track, const TagPolicy& policy)
{
    if(xiphTags->isEmpty()) {
        return;
    }

    const TagLib::Ogg::FieldListMap& fields = xiphTags->fieldListMap();

    using namespace Fooyin::Tag;

    if(fields.contains(TrackNumber)) {
        const TagLib::StringList& trackNumber = fields[TrackNumber];
        if(!trackNumber.isEmpty() && !trackNumber.front().isEmpty()) {
            track.setTrackNumber(convertString(trackNumber.front()));
        }
    }

    if(fields.contains(TrackTotal)) {
        const TagLib::StringList& trackTotal = fields[TrackTotal];
        if(!trackTotal.isEmpty() && !trackTotal.front().isEmpty()) {
            track.setTrackTotal(convertString(trackTotal.front()));
        }
    }

    if(fields.contains(Disc)) {
        const TagLib::StringList& discNumber = fields[Disc];
        if(!discNumber.isEmpty() && !discNumber.front().isEmpty()) {
            track.setDiscNumber(convertString(discNumber.front()));
        }
    }

    if(fields.contains(DiscTotal)) {
        const TagLib::StringList& discTotal = fields[DiscTotal];
        if(!discTotal.isEmpty() && !discTotal.front().isEmpty()) {
            track.setDiscTotal(convertString(discTotal.front()));
        }
    }

    if(fields.contains("FMPS_RATING")) {
        const TagLib::StringList& ratings = fields["FMPS_RATING"];
        if(!ratings.isEmpty()) {
            const QString rawRating = convertString(ratings.front());
            readTextRatingTag(track, policy.rating, u"FMPS_RATING"_s, rawRating, false);
        }
    }

    if(fields.contains("FMPS_PLAYCOUNT")) {
        const TagLib::StringList& countList = fields["FMPS_PLAYCOUNT"];
        if(!countList.isEmpty()) {
            const int count = convertString(countList.front()).toInt();
            if(count > 0) {
                track.setPlayCount(count);
            }
        }
    }
}

void readOpusReplayGain(const TagLib::Ogg::XiphComment* xiphTags, Fooyin::Track& track)
{
    if(xiphTags->isEmpty()) {
        return;
    }

    const TagLib::Ogg::FieldListMap& fields = xiphTags->fieldListMap();
    track.setRGTrackGain(Fooyin::Constants::InvalidGain);
    track.setRGAlbumGain(Fooyin::Constants::InvalidGain);
    track.removeExtraTag(u"R128_TRACK_GAIN"_s);
    track.removeExtraTag(u"R128_ALBUM_GAIN"_s);

    if(fields.contains("R128_TRACK_GAIN")) {
        const TagLib::StringList& gains = fields["R128_TRACK_GAIN"];
        if(!gains.isEmpty()) {
            if(const auto gain = opusR128ToReplayGain(gains.front()); gain.has_value()) {
                track.setRGTrackGain(*gain);
            }
        }
    }

    if(fields.contains("R128_ALBUM_GAIN")) {
        const TagLib::StringList& gains = fields["R128_ALBUM_GAIN"];
        if(!gains.isEmpty()) {
            if(const auto gain = opusR128ToReplayGain(gains.front()); gain.has_value()) {
                track.setRGAlbumGain(*gain);
            }
        }
    }
}

QByteArray readFlacCover(const TagLib::List<TagLib::FLAC::Picture*>& pictures, Fooyin::Track::Cover cover)
{
    if(pictures.isEmpty()) {
        return {};
    }

    TagLib::ByteVector picture;

    using FlacPicture = TagLib::FLAC::Picture;
    for(const auto& pic : pictures) {
        const auto type = pic->type();

        if((cover == Fooyin::Track::Cover::Front && (type == FlacPicture::FrontCover || type == FlacPicture::Other))
           || (cover == Fooyin::Track::Cover::Back && type == FlacPicture::BackCover)
           || (cover == Fooyin::Track::Cover::Artist && type == FlacPicture::Artist)) {
            picture = pic->data();
        }
    }

    if(!picture.isEmpty()) {
        return {picture.data(), static_cast<qsizetype>(picture.size())};
    }

    return {};
}

void writeXiphComment(TagLib::Ogg::XiphComment* xiphTags, const Fooyin::Track& track,
                      Fooyin::AudioReader::WriteOptions options, const TagPolicy& policy)
{
    using namespace Fooyin::Tag;

    if(options & Fooyin::AudioReader::Metadata) {
        if(track.trackNumber().isEmpty()) {
            xiphTags->removeFields(TrackNumber);
        }
        else {
            xiphTags->addField(TrackNumber, convertString(track.trackNumber()), true);
        }

        if(track.trackTotal().isEmpty()) {
            xiphTags->removeFields(TrackTotal);
        }
        else {
            xiphTags->addField(TrackTotal, convertString(track.trackTotal()), true);
        }

        if(track.discNumber().isEmpty()) {
            xiphTags->removeFields(Disc);
        }
        else {
            xiphTags->addField(Disc, convertString(track.discNumber()), true);
        }

        if(track.discTotal().isEmpty()) {
            xiphTags->removeFields(DiscTotal);
        }
        else {
            xiphTags->addField(DiscTotal, convertString(track.discTotal()), true);
        }
    }

    if(options & Fooyin::AudioReader::Rating) {
        xiphTags->removeFields("FMPS_RATING");
        xiphTags->removeFields("RATING");

        if(const auto rating = textRatingWrite(track, policy.rating); !rating.tag.isEmpty()) {
            const TagLib::String tag = convertString(rating.tag);
            xiphTags->removeFields(tag);
            if(!rating.value.isEmpty()) {
                xiphTags->addField(tag, convertString(rating.value), true);
            }
        }
    }

    if(options & Fooyin::AudioReader::Playcount) {
        if(track.playCount() <= 0) {
            xiphTags->removeFields("FMPS_PLAYCOUNT");
        }
        else {
            xiphTags->addField("FMPS_PLAYCOUNT", convertString(QString::number(track.playCount())));
        }
    }
}

void writeOpusReplayGain(TagLib::Ogg::XiphComment* xiphTags, const Fooyin::Track& track,
                         Fooyin::AudioReader::WriteOptions options)
{
    if(!(options & Fooyin::AudioReader::Metadata)) {
        return;
    }

    using namespace Fooyin::Tag::ReplayGain;

    const auto removeField = [xiphTags](QStringView fieldName) {
        QStringList fieldsToRemove;
        const auto& fields = xiphTags->fieldListMap();

        for(const auto& field : fields) {
            if(convertString(field.first).compare(fieldName, Qt::CaseInsensitive) == 0) {
                fieldsToRemove.emplace_back(convertString(field.first));
            }
        }

        for(const QString& field : fieldsToRemove) {
            xiphTags->removeFields(convertString(field));
        }
    };

    removeField(QString::fromLatin1(TrackGain));
    removeField(QString::fromLatin1(TrackGainAlt));
    removeField(QString::fromLatin1(AlbumGain));
    removeField(QString::fromLatin1(AlbumGainAlt));
    removeField(u"R128_TRACK_GAIN"_s);
    removeField(u"R128_ALBUM_GAIN"_s);

    if(track.hasTrackGain()) {
        if(const auto gainQ78 = replayGainToOpusR128Q78(track.rgTrackGain()); gainQ78.has_value()) {
            xiphTags->addField("R128_TRACK_GAIN", convertString(QString::number(*gainQ78)), true);
        }
    }

    if(track.hasAlbumGain()) {
        if(const auto gainQ78 = replayGainToOpusR128Q78(track.rgAlbumGain()); gainQ78.has_value()) {
            xiphTags->addField("R128_ALBUM_GAIN", convertString(QString::number(*gainQ78)), true);
        }
    }
}

bool writeXiphCover(auto* file, const Fooyin::TrackCovers& covers)
{
    using Picture = TagLib::FLAC::Picture;

    const auto toCoverType = [](Picture::Type type) {
        switch(type) {
            case Picture::FrontCover:
                return Fooyin::Track::Cover::Front;
            case Picture::BackCover:
                return Fooyin::Track::Cover::Back;
            case Picture::Artist:
                return Fooyin::Track::Cover::Artist;
            default:
                return Fooyin::Track::Cover::Other;
        }
    };

    const auto fromCoverType = [](Fooyin::Track::Cover type) {
        switch(type) {
            case Fooyin::Track::Cover::Front:
                return Picture::FrontCover;
            case Fooyin::Track::Cover::Back:
                return Picture::BackCover;
            case Fooyin::Track::Cover::Artist:
                return Picture::Artist;
            default:
                return Picture::Other;
        }
    };

    bool modified{false};

    std::vector<TagLib::FLAC::Picture*> picsToRemove;
    {
        const auto pictures = file->pictureList();
        for(auto* picture : pictures) {
            if(covers.contains(toCoverType(picture->type()))) {
                picsToRemove.push_back(picture);
            }
        }
    }

    for(auto* picture : picsToRemove) {
        file->removePicture(picture);
        modified = true;
    }

    for(const auto& [type, cover] : covers) {
        if(!cover.data.isEmpty()) {
            auto* newPicture = new Picture();
            newPicture->setType(fromCoverType(type));
            newPicture->setMimeType(convertString(cover.mimeType));
            newPicture->setData(TagLib::ByteVector(cover.data.constData(), cover.data.size()));

            file->addPicture(newPicture);
            modified = true;
        }
    }

    return modified;
}

void logWriteFailure(QStringView operation, const QString& filepath, const QString& mimeType, QStringView reason)
{
    qCWarning(TAGLIB) << u"Failed to %1 for %2 (%3): %4"_s.arg(operation, filepath, mimeType, reason);
}

bool saveModifiedFile(auto& file, bool modified, QStringView operation, const QString& filepath,
                      const QString& mimeType, bool& failureLogged)
{
    if(!modified) {
        return true;
    }

    if(file.save()) {
        return true;
    }

    logWriteFailure(operation, filepath, mimeType, u"save() returned false"_s);
    failureLogged = true;
    return false;
}

bool isHandledAsfRatingTag(const QString& tag)
{
    return tag == "FMPS_RATING"_L1 || tag == "RATING"_L1 || tag == "WM/SHAREDUSERRATING"_L1;
}

void readAsfTags(const TagLib::ASF::Tag* asfTags, Fooyin::Track& track, const TagPolicy& policy)
{
    if(asfTags->isEmpty()) {
        return;
    }

    const TagLib::ASF::AttributeListMap& map = asfTags->attributeListMap();

    if(map.contains("WM/TrackNumber")) {
        const TagLib::ASF::AttributeList& trackNumber = map["WM/TrackNumber"];
        if(!trackNumber.isEmpty()) {
            const TagLib::ASF::Attribute& num = trackNumber.front();
            if(num.type() == TagLib::ASF::Attribute::UnicodeType) {
                track.setTrackNumber(convertString(num.toString()));
            }
        }
    }

    if(map.contains("WM/PartOfSet")) {
        const TagLib::ASF::AttributeList& discNumber = map["WM/PartOfSet"];
        if(!discNumber.isEmpty()) {
            const TagLib::ASF::Attribute& num = discNumber.front();
            if(num.type() == TagLib::ASF::Attribute::UnicodeType) {
                track.setDiscNumber(convertString(num.toString()));
            }
        }
    }

    if(map.contains("FMPS/Rating")) {
        const TagLib::ASF::AttributeList& ratings = map["FMPS/Rating"];
        if(!ratings.isEmpty()) {
            const QString rawRating = convertString(ratings.front().toString());
            readTextRatingTag(track, policy.rating, u"FMPS_RATING"_s, u"FMPS/Rating"_s, rawRating, false);
            track.setRawRatingTag(u"FMPS_RATING"_s, rawRating);
        }
    }

    if(map.contains("RATING")) {
        const TagLib::ASF::AttributeList& ratings = map["RATING"];
        if(!ratings.isEmpty()) {
            const QString rawRating = convertString(ratings.front().toString());
            readTextRatingTag(track, policy.rating, u"RATING"_s, rawRating, true);
        }
    }

    if(!policy.rating.automaticRead() && !isHandledAsfRatingTag(policy.rating.readTag)) {
        const TagLib::String tag = convertString(policy.rating.readTag);
        const auto ratingIt      = map.find(tag);
        if(ratingIt != map.end() && !ratingIt->second.isEmpty()) {
            const QString rawRating = convertString(ratingIt->second.front().toString());
            readTextRatingTag(track, policy.rating, policy.rating.readTag, rawRating,
                              useRatingTagFallback(policy.rating));
        }
    }

    if(map.contains("FMPS/Playcount") && track.rating() <= 0) {
        const TagLib::ASF::AttributeList& counts = map["FMPS/Playcount"];
        if(!counts.isEmpty()) {
            const int count = convertString(counts.front().toString()).toInt();
            if(count > 0) {
                track.setPlayCount(count);
            }
        }
    }

    if(policy.rating.readAsfSharedRating && map.contains("WM/SharedUserRating") && track.rating() <= 0) {
        const TagLib::ASF::AttributeList& ratings = map["WM/SharedUserRating"];
        if(!ratings.isEmpty()) {
            const auto rating = static_cast<int>(ratings.front().toUInt());
            track.setRawRatingTag(u"WM/SharedUserRating"_s, QString::number(rating));
            if(rating > 0 && rating <= 100) {
                track.setRating(asfSharedUserRatingToRating(rating));
            }
        }
    }
}

QByteArray readAsfCover(const TagLib::ASF::Tag* asfTags, Fooyin::Track::Cover cover)
{
    if(asfTags->isEmpty()) {
        return {};
    }

    const TagLib::ASF::AttributeList pictures = asfTags->attribute("WM/Picture");

    TagLib::ByteVector picture;

    using Picture = TagLib::ASF::Picture;
    for(const auto& attribute : pictures) {
        const Picture pic = attribute.toPicture();
        const auto type   = pic.type();

        if((cover == Fooyin::Track::Cover::Front && (type == Picture::FrontCover || type == Picture::Other))
           || (cover == Fooyin::Track::Cover::Back && type == Picture::BackCover)
           || (cover == Fooyin::Track::Cover::Artist && type == Picture::Artist)) {
            picture = pic.picture();
        }
    }

    if(!picture.isEmpty()) {
        return {picture.data(), static_cast<qsizetype>(picture.size())};
    }

    return {};
}

void writeAsfTags(TagLib::ASF::Tag* asfTags, const Fooyin::Track& track, Fooyin::AudioReader::WriteOptions options,
                  const TagPolicy& policy)
{
    if(options & Fooyin::AudioReader::Metadata) {
        asfTags->setAttribute("WM/TrackNumber", convertString(track.trackNumber()));
        asfTags->setAttribute("WM/PartOfSet", convertString(track.discNumber()));
    }

    if(options & Fooyin::AudioReader::Rating) {
        asfTags->removeItem("FMPS/Rating");
        asfTags->removeItem("RATING");

        const auto rating = textRatingWrite(track, policy.rating);
        if(!rating.tag.isEmpty()) {
            if(rating.tag == "FMPS_RATING"_L1) {
                if(!rating.value.isEmpty()) {
                    asfTags->setAttribute("FMPS/Rating", convertString(rating.value));
                }
            }
            else if(rating.tag == "RATING"_L1) {
                if(!rating.value.isEmpty()) {
                    asfTags->setAttribute("RATING", convertString(rating.value));
                }
            }
            else if(rating.tag == "WM/SHAREDUSERRATING"_L1) {
                // Handled below as a native ASF rating attribute
            }
            else {
                const TagLib::String tag = convertString(rating.tag);
                asfTags->removeItem(tag);
                if(!rating.value.isEmpty()) {
                    asfTags->setAttribute(tag, convertString(rating.value));
                }
            }
        }

        if(policy.rating.writeAsfSharedRating) {
            asfTags->removeItem("WM/SharedUserRating");
            if(track.rating() > 0.0F) {
                asfTags->setAttribute("WM/SharedUserRating", TagLib::ASF::Attribute{static_cast<unsigned int>(
                                                                 ratingToAsfSharedUserRating(track.rating()))});
            }
        }
    }

    if(options & Fooyin::AudioReader::Playcount) {
        if(track.playCount() <= 0) {
            asfTags->removeItem("FMPS/Playcount");
        }
        else {
            asfTags->setAttribute("FMPS/Playcount", convertString(QString::number(track.playCount())));
        }
    }
}

bool writeAsfCover(TagLib::ASF::Tag* asfTags, const Fooyin::TrackCovers& covers)
{
    using Picture = TagLib::ASF::Picture;

    const auto toCoverType = [](Picture::Type type) {
        switch(type) {
            case Picture::FrontCover:
                return Fooyin::Track::Cover::Front;
            case Picture::BackCover:
                return Fooyin::Track::Cover::Back;
            case Picture::Artist:
                return Fooyin::Track::Cover::Artist;
            default:
                return Fooyin::Track::Cover::Other;
        }
    };

    const auto fromCoverType = [](Fooyin::Track::Cover type) {
        switch(type) {
            case Fooyin::Track::Cover::Front:
                return Picture::FrontCover;
            case Fooyin::Track::Cover::Back:
                return Picture::BackCover;
            case Fooyin::Track::Cover::Artist:
                return Picture::Artist;
            default:
                return Picture::Other;
        }
    };

    bool modified{false};

    TagLib::ASF::AttributeList pictures = asfTags->attribute("WM/Picture");

    for(auto it = pictures.begin(); it != pictures.end(); ++it) {
        const Picture pic    = it->toPicture();
        const auto coverType = toCoverType(pic.type());
        if(covers.contains(coverType)) {
            it       = pictures.erase(it);
            modified = true;
        }
    }

    for(const auto& [type, cover] : covers) {
        if(!cover.data.isEmpty()) {
            Picture newPicture;
            newPicture.setType(fromCoverType(type));
            newPicture.setMimeType(convertString(cover.mimeType));
            newPicture.setPicture(TagLib::ByteVector(cover.data.constData(), cover.data.size()));

            pictures.append(TagLib::ASF::Attribute{newPicture});
            modified = true;
        }
    }

    asfTags->setAttribute("WM/Picture", pictures);

    return modified;
}
} // namespace

std::optional<int16_t> readOpusHeaderGainQ78(QIODevice* device)
{
    const auto page = readOpusHeadPage(device);
    if(!page.has_value()) {
        return {};
    }

    const auto gainLo = static_cast<unsigned char>(page->page[page->gainOffset]);
    const auto gainHi = static_cast<unsigned char>(page->page[page->gainOffset + 1]);
    const auto gain   = static_cast<uint16_t>(gainLo | (gainHi << 8));
    return static_cast<int16_t>(gain);
}

bool writeOpusHeaderGainQ78(QIODevice* device, int16_t gain)
{
    if(!device || !device->isOpen() || !device->isWritable() || device->isSequential()) {
        return false;
    }

    const auto page = readOpusHeadPage(device);
    if(!page.has_value()) {
        return false;
    }

    QByteArray updatedPage            = page->page;
    updatedPage[page->gainOffset]     = static_cast<char>(gain & 0xFF);
    updatedPage[page->gainOffset + 1] = static_cast<char>((static_cast<uint16_t>(gain) >> 8) & 0xFF);

    const uint32_t crc = oggPageCrc(updatedPage);
    updatedPage[22]    = static_cast<char>(crc & 0xFF);
    updatedPage[23]    = static_cast<char>((crc >> 8) & 0xFF);
    updatedPage[24]    = static_cast<char>((crc >> 16) & 0xFF);
    updatedPage[25]    = static_cast<char>((crc >> 24) & 0xFF);

    if(!device->seek(0) || device->write(updatedPage.constData(), updatedPage.size()) != updatedPage.size()) {
        return false;
    }

    if(auto* file = qobject_cast<QFile*>(device)) {
        return file->flush();
    }

    return true;
}

QStringList TagLibReader::extensions() const
{
    static const QStringList extensions{u"mp3"_s,  u"ogg"_s, u"opus"_s, u"oga"_s, u"m4a"_s,  u"wav"_s, u"wv"_s,
                                        u"flac"_s, u"wma"_s, u"asf"_s,  u"mpc"_s, u"aiff"_s, u"ape"_s, u"mp4"_s,
#if (TAGLIB_MAJOR_VERSION >= 2)
                                        u"dsf"_s,  u"dff"_s
#endif
    };
    return extensions;
}

bool TagLibReader::canReadCover() const
{
    return true;
}

bool TagLibReader::canWriteMetaData() const
{
    return true;
}

enum VbrMethod : uint8_t
{
    Unknown  = 0,
    CBR      = 1,
    ABR      = 2,
    VBR1     = 3,
    VBR2     = 4,
    VBR3     = 5,
    VBR4     = 6,
    CBR2Pass = 8,
    ABR2Pass = 9,
};

enum XingFlag : uint8_t
{
    Frames   = 1,
    Bytes    = 2,
    TOC      = 4,
    VBRScale = 8,
};

void checkXingHeader(TagLib::MPEG::File* file, Fooyin::Track& track)
{
    // Reference: http://gabriel.mp3-tech.org/mp3infotag.html

    const auto firstFrameOffset = file->firstFrameOffset();
    if(firstFrameOffset < 0) {
        return;
    }

    const TagLib::MPEG::Header firstHeader(file, firstFrameOffset, false);

    file->seek(firstFrameOffset);
    const TagLib::ByteVector header{file->readBlock(firstHeader.frameLength())};

    const QByteArray data{header.data(), header.size()};
    QDataStream stream{data};
    stream.setByteOrder(QDataStream::BigEndian);

    qsizetype offset = data.indexOf("Info");
    if(offset >= 0) {
        track.setCodecProfile(u"CBR"_s);
    }
    else {
        offset = data.indexOf("Xing");
        if(offset < 0) {
            if(data.indexOf("VBRI") >= 0) {
                track.setCodecProfile(u"VBR"_s);
            }
            return;
        }
    }

    stream.skipRawData(offset + 4);

    qint32 flags;
    stream >> flags;

    qint32 vbrScale{-1};

    if(flags & Frames) {
        stream.skipRawData(4);
    }
    if(flags & Bytes) {
        stream.skipRawData(4);
    }
    if(flags & TOC) {
        stream.skipRawData(100);
    }
    if(flags & VBRScale) {
        stream >> vbrScale;
    }

    QByteArray encoderData{9, Qt::Uninitialized};
    const auto len = stream.readRawData(encoderData.data(), 9);

    if(len == 9) {
        auto encoder = QString::fromLatin1(encoderData.constData(), encoderData.size()).simplified().remove(u"\0"_s);
        if(encoder.endsWith(u'.')) {
            encoder.chop(1);
        }
        track.setTool(encoder);
    }
    else if(!encoderData.contains("LAME") && !encoderData.contains("L3.99")) {
        if(vbrScale != -1) {
            track.setCodecProfile(u"VBR"_s);
        }
        else {
            track.setCodecProfile(u"CBR"_s);
        }
        return;
    }

    if(stream.atEnd() || !track.codecProfile().isEmpty()) {
        // Likely CBR
        return;
    }

    qint8 info;
    stream >> info;
    const auto vbrMethod = static_cast<VbrMethod>(info & 0x0F);

    stream.skipRawData(1); // Lowpass
    stream.skipRawData(4); // Peak amp
    stream.skipRawData(2); // Track RG
    stream.skipRawData(2); // Album RG
    stream.skipRawData(1); // Encoding flags
    stream.skipRawData(1); // Bitrate
    stream.skipRawData(3); // Delays
    stream.skipRawData(1); // Misc
    stream.skipRawData(1); // MP3 gain

    if(stream.atEnd()) {
        return;
    }

    qint16 preset;
    stream >> preset;
    preset &= 0x07FF;

    QString codecProfile;
    switch(vbrMethod) {
        case CBR:
        case CBR2Pass:
            codecProfile = u"CBR"_s;
            break;
        case ABR:
        case ABR2Pass:
            codecProfile = u"ABR"_s;
            break;
        case VBR1:
        case VBR2:
        case VBR3:
        case VBR4:
        case Unknown:
        default:
            // Assume VBR if we've got this far
            codecProfile = u"VBR"_s;
            break;
    }

    static const std::map<int, QString> presets{
        {8, u"8"_s},
        {320, u"320"_s},
        {410, u"V9"_s},
        {420, u"V8"_s},
        {430, u"V7"_s},
        {440, u"V6"_s},
        {450, u"V5"_s},
        {460, u"V4"_s},
        {470, u"V3"_s},
        {480, u"V2"_s},
        {490, u"V1"_s},
        {500, u"V0"_s},
        {1000, u"R3mix"_s},
        {1001, u"Standard"_s},
        {1002, u"Extreme"_s},
        {1003, u"Insane"_s},
        {1004, u"Standard Fast"_s},
        {1005, u"Extreme Fast"_s},
        {1006, u"Medium"_s},
        {1007, u"Medium Fast"_s},
    };

    if(!codecProfile.isEmpty() && presets.contains(preset)) {
        codecProfile += " "_L1 + presets.at(preset);
    }
    track.setCodecProfile(codecProfile);
}

bool TagLibReader::readTrack(const AudioSource& source, Track& track)
{
    IODeviceStream stream{source.device, track.filename()};
    if(!stream.isOpen()) {
        qCWarning(TAGLIB) << "Unable to open file readonly:" << source.filepath;
        return false;
    }

    const QMimeDatabase mimeDb;
    QString mimeType       = mimeDb.mimeTypeForFile(source.filepath).name();
    const auto style       = TagLib::AudioProperties::Average;
    const TagPolicy policy = tagPolicy();

    const auto readProperties = [&track, &policy](const TagLib::File& file) {
        readAudioProperties(file, track);
        readGeneralProperties(file.properties(), track, false, true, policy);
    };

    if(mimeType == "audio/ogg"_L1 || mimeType == "audio/x-vorbis+ogg"_L1) {
        // Workaround for opus files with ogg suffix returning incorrect type
        mimeType = mimeDb.mimeTypeForData(source.device).name();
    }
    if(mimeType == "audio/mpeg"_L1 || mimeType == "audio/mpeg3"_L1 || mimeType == "audio/x-mpeg"_L1) {
#if (TAGLIB_MAJOR_VERSION >= 2)
        TagLib::MPEG::File file(&stream, true, style, TagLib::ID3v2::FrameFactory::instance());
#else
        TagLib::MPEG::File file(&stream, TagLib::ID3v2::FrameFactory::instance(), true, style);
#endif
        if(file.isValid()) {
            readAudioProperties(file, track);

            bool readTags{false};
            if(file.hasID3v1Tag()) {
                readGeneralProperties(file.ID3v1Tag()->properties(), track, false, true, policy);
                readTags = true;
            }
            if(file.hasAPETag()) {
                const auto* apeTag = file.APETag();
                readGeneralProperties(apeTag->properties(), track, readTags, !readTags, policy);
                readApeTags(apeTag, track, policy);
                readTags = true;
            }
            if(file.hasID3v2Tag()) {
                const auto* id3Tag = file.ID3v2Tag();
                readGeneralProperties(id3Tag->properties(), track, readTags, !readTags, policy);
                readId3Tags(id3Tag, track, policy, true);
                readTags = true;
            }
            if(!readTags) {
                readGeneralProperties(file.properties(), track, false, true, policy);
            }

            checkXingHeader(&file, track);
            track.setEncoding(u"Lossy"_s);

            QStringList types;
            if(file.hasAPETag()) {
                types.emplace_back(u"apev%1"_s.arg(file.APETag()->footer()->version() / 1000));
            }
            if(file.hasID3v1Tag()) {
                types.emplace_back(u"ID3v1"_s);
            }
            if(file.hasID3v2Tag()) {
                const auto* id3Tag = file.ID3v2Tag();
                types.emplace_back(u"ID3v2.%1"_s.arg(id3Tag->header()->majorVersion()));
            }
            track.setTagTypes(types);
        }
    }
    else if(mimeType == "audio/x-aiff"_L1 || mimeType == "audio/x-aifc"_L1) {
        const TagLib::RIFF::AIFF::File file(&stream, true, style);
        if(file.isValid()) {
            readProperties(file);
            track.setEncoding(u"Lossless"_s);

            if(const auto* props = file.audioProperties()) {
                if(const int bps = props->bitsPerSample(); bps > 0) {
                    track.setBitDepth(bps);
                }
            }
            if(file.hasID3v2Tag()) {
                const auto* id3Tag = file.tag();
                readId3Tags(id3Tag, track, policy, false);
                track.setTagTypes({u"ID3v2.%1"_s.arg(id3Tag->header()->majorVersion())});
            }
        }
    }
    else if(mimeType == "audio/vnd.wave"_L1 || mimeType == "audio/wav"_L1 || mimeType == "audio/x-wav"_L1) {
        const TagLib::RIFF::WAV::File file(&stream, true, style);
        if(file.isValid()) {
            readProperties(file);
            track.setEncoding(u"Lossless"_s);

            if(const auto* props = file.audioProperties()) {
                if(const int bps = props->bitsPerSample(); bps > 0) {
                    track.setBitDepth(bps);
                }
            }
            if(file.hasID3v2Tag()) {
                const auto* id3Tag = file.ID3v2Tag();
                readId3Tags(id3Tag, track, policy, false);
                track.setTagTypes({u"ID3v2.%1"_s.arg(id3Tag->header()->majorVersion())});
            }
        }
    }
    else if(mimeType == "audio/x-musepack"_L1) {
        TagLib::MPC::File file(&stream, true, style);
        if(file.isValid()) {
            readProperties(file);
            track.setEncoding(u"Lossy"_s);

            QStringList types;

            if(file.hasID3v1Tag()) {
                types.emplace_back(u"ID3v1"_s);
            }
            if(file.hasAPETag()) {
                const auto* apeTag = file.APETag();
                readApeTags(apeTag, track, policy);
                types.emplace_back(u"APEv%1"_s.arg(apeTag->footer()->version() / 1000));
            }

            track.setTagTypes(types);
        }
    }
    else if(mimeType == "audio/x-ape"_L1) {
        TagLib::APE::File file(&stream, true, style);
        if(file.isValid()) {
            readProperties(file);
            track.setEncoding(u"Lossless"_s);

            if(const auto* props = file.audioProperties()) {
                if(const int bps = props->bitsPerSample(); bps > 0) {
                    track.setBitDepth(bps);
                }
            }

            QStringList types;

            if(file.hasID3v1Tag()) {
                types.emplace_back(u"ID3v1"_s);
            }

            if(file.hasAPETag()) {
                const auto* apeTag = file.APETag();
                readApeTags(apeTag, track, policy);
                types.emplace_back(u"APEv%1"_s.arg(apeTag->footer()->version() / 1000));
            }

            track.setTagTypes(types);
        }
    }
    else if(mimeType == "audio/x-wavpack"_L1) {
        TagLib::WavPack::File file(&stream, true, style);
        if(file.isValid()) {
            readProperties(file);

            if(const auto* props = file.audioProperties()) {
                if(const int bps = props->bitsPerSample(); bps > 0) {
                    track.setBitDepth(bps);
                }
                track.setEncoding(props->isLossless() ? u"Lossless"_s : u"Lossy"_s);
            }

            QStringList types;

            if(file.hasID3v1Tag()) {
                types.emplace_back(u"ID3v1"_s);
            }

            if(file.hasAPETag()) {
                const auto* apeTag = file.APETag();
                readApeTags(apeTag, track, policy);
                types.emplace_back(u"APEv%1"_s.arg(apeTag->footer()->version() / 1000));
            }
        }
    }
    else if(mimeType == "audio/mp4"_L1 || mimeType == "video/mp4"_L1 || mimeType == "audio/vnd.audible.aax"_L1) {
        const TagLib::MP4::File file(&stream, true, style);
        if(file.isValid()) {
            readProperties(file);

            if(const auto* props = file.audioProperties()) {
                if(const int bps = props->bitsPerSample(); bps > 0) {
                    track.setBitDepth(bps);
                }

                const auto codec = props->codec();
                switch(codec) {
                    case TagLib::MP4::Properties::AAC:
                        track.setCodec(u"AAC"_s);
                        track.setEncoding(u"Lossy"_s);
                        break;
                    case TagLib::MP4::Properties::ALAC:
                        track.setCodec(u"ALAC"_s);
                        track.setEncoding(u"Lossless"_s);
                        break;
                    case TagLib::MP4::Properties::Unknown:
                        break;
                }
            }

            if(file.hasMP4Tag()) {
                readMp4Tags(file.tag(), track, policy);
            }
        }
    }
    else if(mimeType == "audio/flac"_L1 || mimeType == "audio/x-flac"_L1) {
#if (TAGLIB_MAJOR_VERSION >= 2)
        TagLib::FLAC::File file(&stream, true, style, TagLib::ID3v2::FrameFactory::instance());
#else
        TagLib::FLAC::File file(&stream, TagLib::ID3v2::FrameFactory::instance(), true, style);
#endif
        if(file.isValid()) {
            readProperties(file);
            track.setEncoding(u"Lossless"_s);

            if(const auto* props = file.audioProperties()) {
                if(const int bps = props->bitsPerSample(); bps > 0) {
                    track.setBitDepth(bps);
                }
            }
            if(file.hasXiphComment()) {
                readXiphComment(file.xiphComment(), track, policy);
                track.setTagTypes({u"XiphComment"_s});
            }
        }
    }
    else if(mimeType == "audio/ogg"_L1 || mimeType == "audio/x-vorbis+ogg"_L1 || mimeType == "audio/vorbis"_L1
            || mimeType == "application/ogg"_L1) {
        const TagLib::Ogg::Vorbis::File file(&stream, true, style);
        if(file.isValid()) {
            readProperties(file);
            track.setEncoding(u"Lossy"_s);

            if(file.tag()) {
                readXiphComment(file.tag(), track, policy);
                track.setTagTypes({u"XiphComment"_s});
            }
        }
    }
    else if(mimeType == "audio/opus"_L1 || mimeType == "audio/x-opus+ogg"_L1) {
        const TagLib::Ogg::Opus::File file(&stream, true, style);
        if(file.isValid()) {
            readProperties(file);
            track.setEncoding(u"Lossy"_s);

            if(const auto headerGain = readOpusHeaderGainQ78(source.device);
               headerGain.has_value() && *headerGain != 0) {
                track.setOpusHeaderGainQ78(*headerGain);
            }

            if(file.tag()) {
                readXiphComment(file.tag(), track, policy);
                readOpusReplayGain(file.tag(), track);
                track.setTagTypes({u"XiphComment"_s});
            }
        }
    }
    else if(mimeType == "audio/x-ms-wma"_L1 || mimeType == "video/x-ms-asf"_L1
            || mimeType == "application/vnd.ms-asf"_L1) {
        const TagLib::ASF::File file(&stream, true, style);
        if(file.isValid()) {
            readProperties(file);

            if(const auto* props = file.audioProperties()) {
                switch(props->codec()) {
                    case TagLib::ASF::Properties::WMA1:
                        track.setCodecProfile(u"V1"_s);
                        track.setEncoding(u"Lossy"_s);
                        break;
                    case TagLib::ASF::Properties::WMA2:
                        track.setCodecProfile(u"V2"_s);
                        track.setEncoding(u"Lossy"_s);
                        break;
                    case TagLib::ASF::Properties::WMA9Pro:
                        track.setCodecProfile(u"V9"_s);
                        track.setEncoding(u"Lossy"_s);
                        break;
                    case TagLib::ASF::Properties::WMA9Lossless:
                        track.setCodecProfile(u"V9"_s);
                        track.setEncoding(u"Lossless"_s);
                        break;
                    default:
                        break;
                }

                if(const int bps = props->bitsPerSample(); bps > 0) {
                    track.setBitDepth(bps);
                }
            }
            if(file.tag()) {
                readAsfTags(file.tag(), track, policy);
            }
        }
    }
#if (TAGLIB_MAJOR_VERSION >= 2)
    else if(mimeType == "audio/x-dsf"_L1) {
        const TagLib::DSF::File file(&stream, true, style);
        if(file.isValid()) {
            readProperties(file);
            track.setEncoding(u"Lossless"_s);

            if(const auto* props = file.audioProperties()) {
                if(const int bps = props->bitsPerSample(); bps > 0) {
                    track.setBitDepth(bps);
                }
            }

            if(file.tag()) {
                readId3Tags(file.tag(), track, policy, false);
                track.setTagTypes({u"ID3v2.%1"_s.arg(file.tag()->header()->majorVersion())});
            }
        }
    }
    else if(mimeType == "audio/x-dff"_L1) {
        const TagLib::DSDIFF::File file(&stream, true, style);
        if(file.isValid()) {
            readProperties(file);
            track.setEncoding(u"Lossless"_s);

            if(const auto* props = file.audioProperties()) {
                if(const int bps = props->bitsPerSample(); bps > 0) {
                    track.setBitDepth(bps);
                }
            }
            if(file.hasID3v2Tag()) {
                readId3Tags(file.ID3v2Tag(), track, policy, false);
                track.setTagTypes({u"ID3v2.%1"_s.arg(file.ID3v2Tag()->header()->majorVersion())});
            }
        }
    }
#endif
    else {
        qCInfo(TAGLIB) << "Unsupported mime type (" << mimeType << "):" << source.filepath;
        return false;
    }

    if(track.codec().isEmpty()) {
        track.setCodec(codecForMime(mimeType));
    }

    return true;
}

QByteArray TagLibReader::readCover(const AudioSource& source, const Track& track, Track::Cover cover)
{
    IODeviceStream stream{source.device, track.filename()};
    if(!stream.isOpen()) {
        qCWarning(TAGLIB) << "Unable to open file readonly:" << track.filepath();
        return {};
    }

    const QMimeDatabase mimeDb;
    QString mimeType = mimeDb.mimeTypeForFile(source.filepath).name();
    const auto style = TagLib::AudioProperties::Average;

    if(mimeType == "audio/ogg"_L1 || mimeType == "audio/x-vorbis+ogg"_L1) {
        // Workaround for opus files with ogg suffix returning incorrect type
        mimeType = mimeDb.mimeTypeForData(source.device).name();
    }
    if(mimeType == "audio/mpeg"_L1 || mimeType == "audio/mpeg3"_L1 || mimeType == "audio/x-mpeg"_L1) {
#if (TAGLIB_MAJOR_VERSION >= 2)
        TagLib::MPEG::File file(&stream, true, style, TagLib::ID3v2::FrameFactory::instance());
#else
        TagLib::MPEG::File file(&stream, TagLib::ID3v2::FrameFactory::instance(), true, style);
#endif
        if(file.isValid() && file.hasID3v2Tag()) {
            return readId3Cover(file.ID3v2Tag(), cover);
        }
    }
    else if(mimeType == "audio/x-aiff"_L1 || mimeType == "audio/x-aifc"_L1) {
        const TagLib::RIFF::AIFF::File file(&stream, true);
        if(file.isValid() && file.hasID3v2Tag()) {
            return readId3Cover(file.tag(), cover);
        }
    }
    else if(mimeType == "audio/vnd.wave"_L1 || mimeType == "audio/wav"_L1 || mimeType == "audio/x-wav"_L1) {
        const TagLib::RIFF::WAV::File file(&stream, true);
        if(file.isValid() && file.hasID3v2Tag()) {
            return readId3Cover(file.ID3v2Tag(), cover);
        }
    }
    else if(mimeType == "audio/x-musepack"_L1) {
        TagLib::MPC::File file(&stream, true);
        if(file.isValid() && file.APETag()) {
            return readApeCover(file.APETag(), cover);
        }
    }
    else if(mimeType == "audio/x-ape"_L1) {
        TagLib::APE::File file(&stream, true);
        if(file.isValid() && file.APETag()) {
            return readApeCover(file.APETag(), cover);
        }
    }
    else if(mimeType == "audio/x-wavpack"_L1) {
        TagLib::WavPack::File file(&stream, true);
        if(file.isValid() && file.APETag()) {
            return readApeCover(file.APETag(), cover);
        }
    }
    else if(mimeType == "audio/mp4"_L1 || mimeType == "video/mp4"_L1 || mimeType == "audio/vnd.audible.aax"_L1) {
        const TagLib::MP4::File file(&stream, true);
        if(file.isValid() && file.tag()) {
            return readMp4Cover(file.tag(), cover);
        }
    }
    else if(mimeType == "audio/flac"_L1 || mimeType == "audio/x-flac"_L1) {
#if (TAGLIB_MAJOR_VERSION >= 2)
        TagLib::FLAC::File file(&stream, true, style, TagLib::ID3v2::FrameFactory::instance());
#else
        TagLib::FLAC::File file(&stream, TagLib::ID3v2::FrameFactory::instance(), true, style);
#endif
        if(file.isValid()) {
            return readFlacCover(file.pictureList(), cover);
        }
    }
    else if(mimeType == "audio/ogg"_L1 || mimeType == "audio/x-vorbis+ogg"_L1 || mimeType == "audio/vorbis"_L1
            || mimeType == "application/ogg"_L1) {
        const TagLib::Ogg::Vorbis::File file(&stream, true);
        if(file.isValid() && file.tag()) {
            return readFlacCover(file.tag()->pictureList(), cover);
        }
    }
    else if(mimeType == "audio/opus"_L1 || mimeType == "audio/x-opus+ogg"_L1) {
        const TagLib::Ogg::Opus::File file(&stream, true);
        if(file.isValid() && file.tag()) {
            return readFlacCover(file.tag()->pictureList(), cover);
        }
    }
    else if(mimeType == "audio/x-ms-wma"_L1 || mimeType == "video/x-ms-asf"_L1
            || mimeType == "application/vnd.ms-asf"_L1) {
        const TagLib::ASF::File file(&stream, true);
        if(file.isValid() && file.tag()) {
            return readAsfCover(file.tag(), cover);
        }
    }
#if (TAGLIB_MAJOR_VERSION >= 2)
    else if(mimeType == "audio/x-dsf"_L1) {
        const TagLib::DSF::File file(&stream, true);
        if(file.isValid() && file.tag()) {
            return readId3Cover(file.tag(), cover);
        }
    }
    else if(mimeType == "audio/x-dff"_L1) {
        const TagLib::DSDIFF::File file(&stream, true);
        if(file.isValid() && file.hasID3v2Tag()) {
            return readId3Cover(file.ID3v2Tag(), cover);
        }
    }
#endif
    else {
        qCInfo(TAGLIB) << "Unsupported mime type (" << mimeType << "):" << source.filepath;
        return {};
    }

    return {};
}

bool TagLibReader::writeTrack(const AudioSource& source, const Track& track, WriteOptions options)
{
    if(!(options & (Metadata | Rating | Playcount))) {
        return true;
    }

    IODeviceStream stream{source.device, track.filepath()};
    if(!stream.isOpen() || stream.readOnly()) {
        return false;
    }

    QDateTime atime;
    QDateTime mtime;
    if(options & PreserveTimestamps) {
        const QFileInfo info{source.filepath};
        atime = info.lastRead();
        mtime = info.lastModified();
    }

    const auto writeProperties = [&track, options](TagLib::File& file, bool skipExtra = false) {
        auto savedProperties = file.properties();
        writeGenericProperties(savedProperties, track, options, skipExtra);
        file.setProperties(savedProperties);
    };

    const QMimeDatabase mimeDb;
    QString mimeType       = mimeDb.mimeTypeForFile(source.filepath).name();
    const auto style       = TagLib::AudioProperties::Average;
    const TagPolicy policy = tagPolicy();
    bool success{false};
    bool failureLogged{false};

    if(mimeType == "audio/ogg"_L1 || mimeType == "audio/x-vorbis+ogg"_L1) {
        // Workaround for opus files with ogg suffix returning incorrect type
        mimeType = mimeDb.mimeTypeForData(source.device).name();
    }
    if(mimeType == "audio/mpeg"_L1 || mimeType == "audio/mpeg3"_L1 || mimeType == "audio/x-mpeg"_L1) {
#if (TAGLIB_MAJOR_VERSION >= 2)
        TagLib::MPEG::File file(&stream, true, style, TagLib::ID3v2::FrameFactory::instance());
#else
        TagLib::MPEG::File file(&stream, TagLib::ID3v2::FrameFactory::instance(), false, style);
#endif
        if(file.isValid()) {
            writeProperties(file);
            if(auto* tag = file.ID3v2Tag(true)) {
                writeID3v2Tags(tag, track, options, policy, true);
            }
            success = saveModifiedFile(file, true, u"write metadata"_s, source.filepath, mimeType, failureLogged);
        }
    }
    else if(mimeType == "audio/x-aiff"_L1 || mimeType == "audio/x-aifc"_L1) {
        TagLib::RIFF::AIFF::File file(&stream, false);
        if(file.isValid()) {
            writeProperties(file);
            if(file.hasID3v2Tag()) {
                writeID3v2Tags(file.tag(), track, options, policy, false);
            }
            else {
                logWriteFailure(u"write metadata"_s, source.filepath, mimeType, u"file has no ID3v2 tag block"_s);
                failureLogged = true;
            }

            success = failureLogged
                        ? false
                        : saveModifiedFile(file, true, u"write metadata"_s, source.filepath, mimeType, failureLogged);
        }
    }
    else if(mimeType == "audio/vnd.wave"_L1 || mimeType == "audio/wav"_L1 || mimeType == "audio/x-wav"_L1) {
        TagLib::RIFF::WAV::File file(&stream, false);
        if(file.isValid()) {
            writeProperties(file);
            if(file.hasID3v2Tag()) {
                writeID3v2Tags(file.ID3v2Tag(), track, options, policy, false);
            }
            else {
                logWriteFailure(u"write metadata"_s, source.filepath, mimeType, u"file has no ID3v2 tag block"_s);
                failureLogged = true;
            }

            success = failureLogged
                        ? false
                        : saveModifiedFile(file, true, u"write metadata"_s, source.filepath, mimeType, failureLogged);
        }
    }
    else if(mimeType == "audio/x-musepack"_L1) {
        TagLib::MPC::File file(&stream, false);
        if(file.isValid()) {
            writeProperties(file);
            if(auto* tag = file.APETag(true)) {
                writeApeTags(tag, track, options, policy);
            }
            success = saveModifiedFile(file, true, u"write metadata"_s, source.filepath, mimeType, failureLogged);
        }
    }
    else if(mimeType == "audio/x-ape"_L1) {
        TagLib::APE::File file(&stream, false);
        if(file.isValid()) {
            writeProperties(file);
            if(auto* tag = file.APETag(true)) {
                writeApeTags(tag, track, options, policy);
            }
            success = saveModifiedFile(file, true, u"write metadata"_s, source.filepath, mimeType, failureLogged);
        }
    }
    else if(mimeType == "audio/x-wavpack"_L1) {
        TagLib::WavPack::File file(&stream, false);
        if(file.isValid()) {
            writeProperties(file);
            if(auto* tag = file.APETag(true)) {
                writeApeTags(tag, track, options, policy);
            }
            success = saveModifiedFile(file, true, u"write metadata"_s, source.filepath, mimeType, failureLogged);
        }
    }
    else if(mimeType == "audio/mp4"_L1 || mimeType == "video/mp4"_L1 || mimeType == "audio/vnd.audible.aax"_L1) {
        TagLib::MP4::File file(&stream, false);
        if(file.isValid()) {
            writeProperties(file, true);
            if(file.hasMP4Tag()) {
                writeMp4Tags(file.tag(), track, options, policy);
            }
            success = saveModifiedFile(file, true, u"write metadata"_s, source.filepath, mimeType, failureLogged);
        }
    }
    else if(mimeType == "audio/flac"_L1 || mimeType == "audio/x-flac"_L1) {
#if (TAGLIB_MAJOR_VERSION >= 2)
        TagLib::FLAC::File file(&stream, true, style, TagLib::ID3v2::FrameFactory::instance());
#else
        TagLib::FLAC::File file(&stream, TagLib::ID3v2::FrameFactory::instance(), false, style);
#endif
        if(file.isValid()) {
            writeProperties(file);
            if(auto* tags = file.xiphComment(true)) {
                writeXiphComment(tags, track, options, policy);
            }
            success = saveModifiedFile(file, true, u"write metadata"_s, source.filepath, mimeType, failureLogged);
        }
    }
    else if(mimeType == "audio/ogg"_L1 || mimeType == "audio/x-vorbis+ogg"_L1 || mimeType == "audio/vorbis"_L1
            || mimeType == "application/ogg"_L1) {
        TagLib::Ogg::Vorbis::File file(&stream, false);
        if(file.isValid()) {
            writeProperties(file);
            if(file.tag()) {
                writeXiphComment(file.tag(), track, options, policy);
            }
            else {
                logWriteFailure(u"write metadata"_s, source.filepath, mimeType, u"file has no Xiph comment block"_s);
                failureLogged = true;
            }

            success = failureLogged
                        ? false
                        : saveModifiedFile(file, true, u"write metadata"_s, source.filepath, mimeType, failureLogged);
        }
    }
    else if(mimeType == "audio/opus"_L1 || mimeType == "audio/x-opus+ogg"_L1) {
        TagLib::Ogg::Opus::File file(&stream, false);
        if(file.isValid()) {
            writeProperties(file);
            if(file.tag()) {
                writeXiphComment(file.tag(), track, options, policy);
                writeOpusReplayGain(file.tag(), track, options);
            }
            else {
                logWriteFailure(u"write metadata"_s, source.filepath, mimeType, u"file has no Xiph comment block"_s);
                failureLogged = true;
            }

            if(const auto headerGain = track.opusHeaderGainQ78(); headerGain.has_value()) {
                if(!writeOpusHeaderGainQ78(source.device, *headerGain)) {
                    logWriteFailure(u"write opus header gain"_s, source.filepath, mimeType, u"failed"_s);
                    failureLogged = true;
                }
            }

            success = failureLogged
                        ? false
                        : saveModifiedFile(file, true, u"write metadata"_s, source.filepath, mimeType, failureLogged);
        }
    }
    else if(mimeType == "audio/x-ms-wma"_L1 || mimeType == "video/x-ms-asf"_L1
            || mimeType == "application/vnd.ms-asf"_L1) {
        TagLib::ASF::File file(&stream, false);
        if(file.isValid()) {
            writeProperties(file);
            if(file.tag()) {
                writeAsfTags(file.tag(), track, options, policy);
            }
            else {
                logWriteFailure(u"write metadata"_s, source.filepath, mimeType, u"file has no ASF tag block"_s);
                failureLogged = true;
            }

            success = failureLogged
                        ? false
                        : saveModifiedFile(file, true, u"write metadata"_s, source.filepath, mimeType, failureLogged);
        }
    }
#if (TAGLIB_MAJOR_VERSION >= 2)
    else if(mimeType == "audio/x-dsf"_L1) {
        TagLib::DSF::File file(&stream, false);
        if(file.isValid()) {
            writeProperties(file);
            if(file.tag()) {
                writeID3v2Tags(file.tag(), track, options, policy, false);
            }
            else {
                logWriteFailure(u"write metadata"_s, source.filepath, mimeType, u"file has no ID3v2 tag block"_s);
                failureLogged = true;
            }

            success = failureLogged
                        ? false
                        : saveModifiedFile(file, true, u"write metadata"_s, source.filepath, mimeType, failureLogged);
        }
    }
    else if(mimeType == "audio/x-dff"_L1) {
        TagLib::DSDIFF::File file(&stream, false);
        if(file.isValid()) {
            writeProperties(file);
            if(auto* tag = file.ID3v2Tag(true)) {
                writeID3v2Tags(tag, track, options, policy, false);
            }
            success = saveModifiedFile(file, true, u"write metadata"_s, source.filepath, mimeType, failureLogged);
        }
    }
#endif
    else {
        qCInfo(TAGLIB) << "Unsupported mime type (" << mimeType << "):" << source.filepath;
        return false;
    }

    if(!success && !failureLogged) {
        logWriteFailure(u"write metadata"_s, source.filepath, mimeType,
                        u"file is invalid or no writable tag handler was available"_s);
    }

    if(!success) {
        return false;
    }

    if(atime.isValid() || mtime.isValid()) {
        if(auto* fileDev = qobject_cast<QFile*>(source.device)) {
            fileDev->setFileTime(mtime, QFileDevice::FileModificationTime);
            fileDev->setFileTime(atime, QFileDevice::FileAccessTime);
        }
    }

    return success;
}

bool TagLibReader::writeCover(const AudioSource& source, const Track& track, const TrackCovers& covers,
                              WriteOptions options)
{
    IODeviceStream stream{source.device, track.filepath()};
    if(!stream.isOpen() || stream.readOnly()) {
        return false;
    }

    QDateTime atime;
    QDateTime mtime;
    if(options & PreserveTimestamps) {
        const QFileInfo info{source.filepath};
        atime = info.lastRead();
        mtime = info.lastModified();
    }

    const QMimeDatabase mimeDb;
    QString mimeType = mimeDb.mimeTypeForFile(source.filepath).name();
    const auto style = TagLib::AudioProperties::Average;
    bool success{false};
    bool failureLogged{false};

    if(mimeType == "audio/ogg"_L1 || mimeType == "audio/x-vorbis+ogg"_L1) {
        // Workaround for opus files with ogg suffix returning incorrect type
        mimeType = mimeDb.mimeTypeForData(source.device).name();
    }
    if(mimeType == "audio/mpeg"_L1 || mimeType == "audio/mpeg3"_L1 || mimeType == "audio/x-mpeg"_L1) {
#if (TAGLIB_MAJOR_VERSION >= 2)
        TagLib::MPEG::File file(&stream, true, style, TagLib::ID3v2::FrameFactory::instance());
#else
        TagLib::MPEG::File file(&stream, TagLib::ID3v2::FrameFactory::instance(), false, style);
#endif
        if(file.isValid()) {
            if(auto* tag = file.ID3v2Tag(true)) {
                success = saveModifiedFile(file, writeId3Cover(tag, covers), u"write cover artwork"_s, source.filepath,
                                           mimeType, failureLogged);
            }
        }
    }
    else if(mimeType == "audio/x-aiff"_L1 || mimeType == "audio/x-aifc"_L1) {
        TagLib::RIFF::AIFF::File file(&stream, false);
        if(file.isValid() && file.hasID3v2Tag()) {
            success = saveModifiedFile(file, writeId3Cover(file.tag(), covers), u"write cover artwork"_s,
                                       source.filepath, mimeType, failureLogged);
        }
        else if(file.isValid()) {
            logWriteFailure(u"write cover artwork"_s, source.filepath, mimeType, u"file has no ID3v2 tag block"_s);
            failureLogged = true;
        }
    }
    else if(mimeType == "audio/vnd.wave"_L1 || mimeType == "audio/wav"_L1 || mimeType == "audio/x-wav"_L1) {
        TagLib::RIFF::WAV::File file(&stream, false);
        if(file.isValid() && file.hasID3v2Tag()) {
            success = saveModifiedFile(file, writeId3Cover(file.ID3v2Tag(), covers), u"write cover artwork"_s,
                                       source.filepath, mimeType, failureLogged);
        }
        else if(file.isValid()) {
            logWriteFailure(u"write cover artwork"_s, source.filepath, mimeType, u"file has no ID3v2 tag block"_s);
            failureLogged = true;
        }
    }
    else if(mimeType == "audio/x-musepack"_L1) {
        TagLib::MPC::File file(&stream, false);
        if(file.isValid()) {
            if(auto* tag = file.APETag(true)) {
                success = saveModifiedFile(file, writeApeCover(tag, covers), u"write cover artwork"_s, source.filepath,
                                           mimeType, failureLogged);
            }
        }
    }
    else if(mimeType == "audio/x-ape"_L1) {
        TagLib::APE::File file(&stream, false);
        if(file.isValid()) {
            if(auto* tag = file.APETag(true)) {
                success = saveModifiedFile(file, writeApeCover(tag, covers), u"write cover artwork"_s, source.filepath,
                                           mimeType, failureLogged);
            }
        }
    }
    else if(mimeType == "audio/x-wavpack"_L1) {
        TagLib::WavPack::File file(&stream, false);
        if(file.isValid()) {
            if(auto* tag = file.APETag(true)) {
                success = saveModifiedFile(file, writeApeCover(tag, covers), u"write cover artwork"_s, source.filepath,
                                           mimeType, failureLogged);
            }
        }
    }
    else if(mimeType == "audio/mp4"_L1 || mimeType == "video/mp4"_L1 || mimeType == "audio/vnd.audible.aax"_L1) {
        TagLib::MP4::File file(&stream, false);
        if(file.isValid() && file.hasMP4Tag()) {
            success = saveModifiedFile(file, writeMp4Cover(file.tag(), covers), u"write cover artwork"_s,
                                       source.filepath, mimeType, failureLogged);
        }
        else if(file.isValid()) {
            logWriteFailure(u"write cover artwork"_s, source.filepath, mimeType, u"file has no MP4 tag block"_s);
            failureLogged = true;
        }
    }
    else if(mimeType == "audio/flac"_L1 || mimeType == "audio/x-flac"_L1) {
#if (TAGLIB_MAJOR_VERSION >= 2)
        TagLib::FLAC::File file(&stream, true, style, TagLib::ID3v2::FrameFactory::instance());
#else
        TagLib::FLAC::File file(&stream, TagLib::ID3v2::FrameFactory::instance(), false, style);
#endif
        if(file.isValid() && file.xiphComment(true)) {
            success = saveModifiedFile(file, writeXiphCover(&file, covers), u"write cover artwork"_s, source.filepath,
                                       mimeType, failureLogged);
        }
    }
    else if(mimeType == "audio/ogg"_L1 || mimeType == "audio/x-vorbis+ogg"_L1 || mimeType == "audio/vorbis"_L1
            || mimeType == "application/ogg"_L1) {
        TagLib::Ogg::Vorbis::File file(&stream, false);
        if(file.isValid() && file.tag()) {
            success = saveModifiedFile(file, writeXiphCover(file.tag(), covers), u"write cover artwork"_s,
                                       source.filepath, mimeType, failureLogged);
        }
        else if(file.isValid()) {
            logWriteFailure(u"write cover artwork"_s, source.filepath, mimeType, u"file has no Xiph comment block"_s);
            failureLogged = true;
        }
    }
    else if(mimeType == "audio/opus"_L1 || mimeType == "audio/x-opus+ogg"_L1) {
        TagLib::Ogg::Opus::File file(&stream, false);
        if(file.isValid() && file.tag()) {
            success = saveModifiedFile(file, writeXiphCover(file.tag(), covers), u"write cover artwork"_s,
                                       source.filepath, mimeType, failureLogged);
        }
        else if(file.isValid()) {
            logWriteFailure(u"write cover artwork"_s, source.filepath, mimeType, u"file has no Xiph comment block"_s);
            failureLogged = true;
        }
    }
    else if(mimeType == "audio/x-ms-wma"_L1 || mimeType == "video/x-ms-asf"_L1
            || mimeType == "application/vnd.ms-asf"_L1) {
        TagLib::ASF::File file(&stream, false);
        if(file.isValid() && file.tag()) {
            success = saveModifiedFile(file, writeAsfCover(file.tag(), covers), u"write cover artwork"_s,
                                       source.filepath, mimeType, failureLogged);
        }
        else if(file.isValid()) {
            logWriteFailure(u"write cover artwork"_s, source.filepath, mimeType, u"file has no ASF tag block"_s);
            failureLogged = true;
        }
    }
#if (TAGLIB_MAJOR_VERSION >= 2)
    else if(mimeType == "audio/x-dsf"_L1) {
        TagLib::DSF::File file(&stream, false);
        if(file.isValid() && file.tag()) {
            success = saveModifiedFile(file, writeId3Cover(file.tag(), covers), u"write cover artwork"_s,
                                       source.filepath, mimeType, failureLogged);
        }
        else if(file.isValid()) {
            logWriteFailure(u"write cover artwork"_s, source.filepath, mimeType, u"file has no ID3v2 tag block"_s);
            failureLogged = true;
        }
    }
    else if(mimeType == "audio/x-dff"_L1) {
        TagLib::DSDIFF::File file(&stream, false);
        if(file.isValid() && file.hasID3v2Tag()) {
            success = saveModifiedFile(file, writeId3Cover(file.ID3v2Tag(), covers), u"write cover artwork"_s,
                                       source.filepath, mimeType, failureLogged);
        }
        else if(file.isValid()) {
            logWriteFailure(u"write cover artwork"_s, source.filepath, mimeType, u"file has no ID3v2 tag block"_s);
            failureLogged = true;
        }
    }
#endif
    else {
        qCInfo(TAGLIB) << "Unsupported mime type (" << mimeType << "):" << source.filepath;
        return false;
    }

    if(!success && !failureLogged) {
        logWriteFailure(u"write cover artwork"_s, source.filepath, mimeType,
                        u"file is invalid or no writable tag handler was available"_s);
    }

    if(!success) {
        return false;
    }

    if(atime.isValid() || mtime.isValid()) {
        if(auto* fileDev = qobject_cast<QFile*>(source.device)) {
            fileDev->setFileTime(mtime, QFileDevice::FileModificationTime);
            fileDev->setFileTime(atime, QFileDevice::FileAccessTime);
        }
    }

    return success;
}
} // namespace Fooyin
