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

#include "tagdefs.h"

#include <core/track.h>
#include <utils/helpers.h>

#include <taglib/aifffile.h>
#include <taglib/apefile.h>
#include <taglib/apetag.h>
#include <taglib/asffile.h>
#include <taglib/asfpicture.h>
#include <taglib/asftag.h>
#include <taglib/attachedpictureframe.h>
#include <taglib/flacfile.h>
#include <taglib/id3v2framefactory.h>
#include <taglib/id3v2tag.h>
#include <taglib/mp4file.h>
#include <taglib/mp4tag.h>
#include <taglib/mpcfile.h>
#include <taglib/mpegfile.h>
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

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QMimeDatabase>
#include <QPixmap>

#include <set>

Q_LOGGING_CATEGORY(TAGLIB, "TagLib")

constexpr auto BufferSize = 1024;

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
        m_input->write(data.data(), data.size());
    }

#if TAGLIB_MAJOR_VERSION >= 2
    void insert(const TagLib::ByteVector& data, TagLib::offset_t start, size_t replace) override
#else
    void insert(const TagLib::ByteVector& data, unsigned long start, unsigned long replace) override
#endif
    {
        if(!isOpen() || readOnly()) {
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
        if(!isOpen() || readOnly()) {
            return;
        }

        const qint64 fileSize = m_input->size();
        if(start + length > static_cast<size_t>(fileSize)) {
            length = static_cast<size_t>(fileSize) - start;
        }

        QByteArray buffer(BufferSize, 0);

        auto readPosition  = static_cast<qint64>(start) + static_cast<qint64>(length);
        auto writePosition = static_cast<qint64>(start);

        qint64 bytesRead{std::numeric_limits<qint64>::max()};
        while(bytesRead != 0) {
            m_input->seek(readPosition);
            bytesRead = m_input->peek(buffer.data(), BufferSize);

            if(bytesRead <= 0) {
                clear();
                buffer.resize(bytesRead);
            }

            m_input->seek(writePosition);
            m_input->write(buffer.left(bytesRead));

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

    void seek(long offset, Position p) override
    {
        const auto seekPos = static_cast<qint64>(offset);
        switch(p) {
            case(Beginning):
                m_input->seek(seekPos);
                break;
            case(Current):
                m_input->seek(m_input->pos() + seekPos);
                break;
            case(End):
                m_input->seek(m_input->size() + seekPos);
                break;
        }
    }

    void clear() override
    {
        m_input->seek(0);
        TagLib::IOStream::clear();
    }

    [[nodiscard]] long tell() const override
    {
        return m_input->pos();
    }

    long length() override
    {
        return m_input->size();
    }

    void truncate(long length) override
    {
        const auto currPos = tell();

        seek(0, Beginning);
        const QByteArray data = m_input->peek(length);
        m_input->write(data);
        seek(currPos, Beginning);
    }

private:
    QIODevice* m_input;
    QByteArray m_fileName;
};

constexpr std::array mp4ToTag{
    std::pair(Fooyin::Mp4::Title, Fooyin::Tag::Title),
    std::pair(Fooyin::Mp4::Artist, Fooyin::Tag::Artist),
    std::pair(Fooyin::Mp4::Album, Fooyin::Tag::Album),
    std::pair(Fooyin::Mp4::AlbumArtist, Fooyin::Tag::AlbumArtist),
    std::pair(Fooyin::Mp4::Genre, Fooyin::Tag::Genre),
    std::pair(Fooyin::Mp4::Composer, Fooyin::Tag::Composer),
    std::pair(Fooyin::Mp4::Performer, Fooyin::Tag::Performer),
    std::pair(Fooyin::Mp4::Comment, Fooyin::Tag::Comment),
    std::pair(Fooyin::Mp4::Date, Fooyin::Tag::Date),
    std::pair(Fooyin::Mp4::Rating, Fooyin::Tag::Rating),
    std::pair(Fooyin::Mp4::RatingAlt, Fooyin::Tag::Rating),
    std::pair(Fooyin::Mp4::Track, Fooyin::Tag::Track),
    std::pair(Fooyin::Mp4::Disc, Fooyin::Tag::Disc),
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
    std::pair(Fooyin::Tag::Track, Fooyin::Mp4::Track),
    std::pair(Fooyin::Tag::Disc, Fooyin::Mp4::Disc),
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

QString codecForMime(const QString& mimeType)
{
    if(mimeType == u"audio/mpeg" || mimeType == u"audio/mpeg3" || mimeType == u"audio/x-mpeg") {
        return QStringLiteral("MP3");
    }
    if(mimeType == u"audio/x-aiff" || mimeType == u"audio/x-aifc") {
        return QStringLiteral("AIFF");
    }
    if(mimeType == u"audio/vnd.wave" || mimeType == u"audio/wav" || mimeType == u"audio/x-wav") {
        return QStringLiteral("PCM");
    }
    if(mimeType == u"audio/x-musepack") {
        return QStringLiteral("MPC");
    }
    if(mimeType == u"audio/x-ape") {
        return QStringLiteral("Monkey's Audio");
    }
    if(mimeType == u"audio/x-wavpack") {
        return QStringLiteral("WavPack");
    }
    if(mimeType == u"audio/flac") {
        return QStringLiteral("FLAC");
    }
    if(mimeType == u"audio/ogg" || mimeType == u"audio/x-vorbis+ogg" || mimeType == u"application/ogg") {
        return QStringLiteral("Vorbis");
    }
    if(mimeType == u"audio/opus" || mimeType == u"audio/x-opus+ogg") {
        return QStringLiteral("Opus");
    }
    if(mimeType == u"audio/x-ms-wma") {
        return QStringLiteral("WMA");
    }

    return QStringLiteral("Unknown");
}

void readAudioProperties(const TagLib::File& file, Fooyin::Track& track)
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

void readGeneralProperties(const TagLib::PropertyMap& props, Fooyin::Track& track)
{
    track.clearExtraTags();

    for(const auto& [field, value] : props) {
        if(field == Fooyin::Tag::Title) {
            track.setTitle(convertString(value.toString()));
        }
        else if(field == Fooyin::Tag::Artist || field == Fooyin::Tag::ArtistAlt) {
            track.setArtists(convertStringList(value));
        }
        else if(field == Fooyin::Tag::Album) {
            track.setAlbum(convertString(value.toString()));
        }
        else if(field == Fooyin::Tag::AlbumArtist) {
            track.setAlbumArtists(convertStringList(value));
        }
        else if(field == Fooyin::Tag::Genre) {
            track.setGenres(convertStringList(value));
        }
        else if(field == Fooyin::Tag::Composer) {
            track.setComposer(convertString(value.toString()));
        }
        else if(field == Fooyin::Tag::Performer) {
            track.setPerformer(convertString(value.toString()));
        }
        else if(field == Fooyin::Tag::Comment) {
            track.setComment(convertString(value.toString()));
        }
        else if(field == Fooyin::Tag::Date) {
            track.setDate(convertString(value.toString()));
        }
        else if(field == Fooyin::Tag::Year) {
            track.setYear(convertString(value.toString()).toInt());
        }
        else if(field == Fooyin::Tag::Track || field == Fooyin::Tag::TrackAlt) {
            track.setTrackNumber(convertString(value.toString()));
        }
        else if(field == Fooyin::Tag::TrackTotal || field == Fooyin::Tag::TrackTotalAlt) {
            track.setTrackTotal(convertString(value.toString()));
        }
        else if(field == Fooyin::Tag::Disc || field == Fooyin::Tag::DiscAlt) {
            track.setDiscNumber(convertString(value.toString()));
        }
        else if(field == Fooyin::Tag::DiscTotal || field == Fooyin::Tag::DiscTotalAlt) {
            track.setDiscTotal(convertString(value.toString()));
        }
        else if(field == Fooyin::Tag::Rating) {
            const int rating = value.front().toInt();
            if(rating > 0 && rating <= 100) {
                float adjustedRating = static_cast<float>(rating) / 10;
                if(adjustedRating > 1.0) {
                    adjustedRating /= 10;
                }
                else {
                    adjustedRating *= 2;
                }
                track.setRating(adjustedRating);
            }
        }
        else if(field == Fooyin::Tag::RatingAlt) {
            const float rating = convertString(value.toString()).toFloat();
            if(rating > 0) {
                track.setRating(rating);
            }
        }
        else if(field == Fooyin::Tag::PlayCount) {
            const int count = convertString(value.toString()).toInt();
            if(count > 0) {
                track.setPlayCount(count);
            }
        }
        else {
            const auto tagEntry = convertString(field);
            for(const auto& tagValue : value) {
                track.addExtraTag(tagEntry, convertString(tagValue));
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

void writeGenericProperties(TagLib::PropertyMap& oldProperties, const Fooyin::Track& track, bool skipExtra = false)
{
    if(!track.isValid()) {
        return;
    }

    replaceOrErase(oldProperties, Fooyin::Tag::Title, track.title());
    replaceOrErase(oldProperties, Fooyin::Tag::Artist, track.artists());
    replaceOrErase(oldProperties, Fooyin::Tag::Album, track.album());
    replaceOrErase(oldProperties, Fooyin::Tag::AlbumArtist, track.albumArtists());
    replaceOrErase(oldProperties, Fooyin::Tag::Genre, track.genres());
    replaceOrErase(oldProperties, Fooyin::Tag::Composer, track.composer());
    replaceOrErase(oldProperties, Fooyin::Tag::Performer, track.performer());
    replaceOrErase(oldProperties, Fooyin::Tag::Comment, track.comment());
    replaceOrErase(oldProperties, Fooyin::Tag::Date, track.date());

    if(oldProperties.contains(Fooyin::Tag::Track)) {
        replaceOrErase(oldProperties, Fooyin::Tag::Track, track.trackNumber());
    }
    if(oldProperties.contains(Fooyin::Tag::TrackAlt)) {
        replaceOrErase(oldProperties, Fooyin::Tag::TrackAlt, track.trackNumber());
    }
    if(oldProperties.contains(Fooyin::Tag::TrackTotal)) {
        replaceOrErase(oldProperties, Fooyin::Tag::TrackTotal, track.trackTotal());
    }
    if(oldProperties.contains(Fooyin::Tag::TrackTotalAlt)) {
        replaceOrErase(oldProperties, Fooyin::Tag::TrackTotalAlt, track.trackTotal());
    }
    if(oldProperties.contains(Fooyin::Tag::Disc)) {
        replaceOrErase(oldProperties, Fooyin::Tag::Disc, track.discNumber());
    }
    if(oldProperties.contains(Fooyin::Tag::DiscAlt)) {
        replaceOrErase(oldProperties, Fooyin::Tag::DiscAlt, track.discNumber());
    }
    if(oldProperties.contains(Fooyin::Tag::DiscTotal)) {
        replaceOrErase(oldProperties, Fooyin::Tag::DiscTotal, track.discTotal());
    }
    if(oldProperties.contains(Fooyin::Tag::DiscTotalAlt)) {
        replaceOrErase(oldProperties, Fooyin::Tag::DiscTotalAlt, track.discTotal());
    }

    if(!skipExtra) {
        const auto customTags = track.extraTags();
        for(const auto& [tag, values] : Fooyin::Utils::asRange(customTags)) {
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

float popmToRating(int popm)
{
    // Reference: https://www.mediamonkey.com/forum/viewtopic.php?f=7&t=40532&start=30#p391067
    if(popm == 0) {
        return 0.0;
    }
    if(popm == 1) {
        return 0.2;
    }
    if(popm < 23) {
        return 0.1;
    }
    if(popm < 32) {
        return 0.2;
    }
    if(popm < 64) {
        return 0.3;
    }
    if(popm < 96) {
        return 0.4;
    }
    if(popm < 128) {
        return 0.5;
    }
    if(popm < 160) {
        return 0.6;
    }
    if(popm < 196) {
        return 0.7;
    }
    if(popm < 224) {
        return 0.8;
    }
    if(popm < 255) {
        return 0.9;
    }

    return 1.0;
};

int ratingToPopm(float rating)
{
    if(rating < 0.2) {
        return 0;
    }
    if(rating < 0.4) {
        return 1;
    }
    if(rating < 0.6) {
        return 64;
    }
    if(rating < 0.8) {
        return 128;
    }
    if(rating < 1.0) {
        return 196;
    }

    return 255;
};

QString getTrackNumber(const Fooyin::Track& track)
{
    QString trackNumber = track.trackNumber();
    if(!track.trackTotal().isEmpty()) {
        trackNumber += QStringLiteral("/") + track.trackTotal();
    }
    return trackNumber;
}

QString getDiscNumber(const Fooyin::Track& track)
{
    QString discNumber = track.discNumber();
    if(!track.discTotal().isEmpty()) {
        discNumber += QStringLiteral("/") + track.discTotal();
    }
    return discNumber;
}

void readTrackTotalPair(const QString& trackNumbers, Fooyin::Track& track)
{
    const qsizetype splitIdx = trackNumbers.indexOf(u"/");
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
    const qsizetype splitIdx = discNumbers.indexOf(u"/");
    if(splitIdx >= 0) {
        track.setDiscNumber(discNumbers.first(splitIdx));
        track.setDiscTotal(discNumbers.sliced(splitIdx + 1));
    }
    else if(discNumbers.size() > 0) {
        track.setDiscNumber(discNumbers);
    }
}

void readId3Tags(const TagLib::ID3v2::Tag* id3Tags, Fooyin::Track& track)
{
    if(id3Tags->isEmpty()) {
        return;
    }

    const TagLib::ID3v2::FrameListMap& frames = id3Tags->frameListMap();

    if(id3Tags->header()->majorVersion() == 3) {
        // Handle '/' being used as a separator for artists in v2.3
        if(frames.contains("TPE1")) {
            const TagLib::ID3v2::FrameList& artistsFrame = frames["TPE1"];
            if(!artistsFrame.isEmpty()) {
                const QString artist = convertString(artistsFrame.front()->toString());
                // Ignore common artist names
                if(artist.contains(u"/") && (artist != u"AC/DC" || artist != u"AC / DC")) {
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

    if(frames.contains("FMPS_Rating") && track.rating() <= 0) {
        const TagLib::ID3v2::FrameList& ratingFrame = frames["FMPS_Rating"];
        if(!ratingFrame.isEmpty()) {
            const float rating = convertString(ratingFrame.front()->toString()).toFloat();
            if(rating > 0 && rating <= 1.0) {
                track.setRating(rating);
            }
        }
    }

    if(frames.contains("FMPS_Playcount") && track.playCount() <= 0) {
        const TagLib::ID3v2::FrameList& countFrame = frames["FMPS_Playcount"];
        if(!countFrame.isEmpty()) {
            const int count = convertString(countFrame.front()->toString()).toInt();
            if(count > 0) {
                track.setPlayCount(count);
            }
        }
    }

    if(frames.contains("POPM")) {
        // Use only first rating
        if(auto* ratingFrame = dynamic_cast<TagLib::ID3v2::PopularimeterFrame*>(frames["POPM"].front())) {
            if(track.playCount() <= 0 && ratingFrame->counter() > 0) {
                track.setPlayCount(static_cast<int>(ratingFrame->counter()));
            }
            if(track.rating() <= 0 && ratingFrame->rating() > 0) {
                track.setRating(popmToRating(ratingFrame->rating()));
            }
        }
    }
}

QByteArray readId3Cover(const TagLib::ID3v2::Tag* id3Tags, Fooyin::Track::Cover cover)
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

        if((cover == Fooyin::Track::Cover::Front && (type == PictureFrame::FrontCover || type == PictureFrame::Other))
           || (cover == Fooyin::Track::Cover::Back && type == PictureFrame::BackCover)
           || (cover == Fooyin::Track::Cover::Artist && type == PictureFrame::Artist)) {
            picture = coverFrame->picture();
        }
    }

    if(picture.isEmpty()) {
        return {};
    }

    return {picture.data(), static_cast<qsizetype>(picture.size())};
}

void writeID3v2Tags(TagLib::ID3v2::Tag* id3Tags, const Fooyin::Track& track, Fooyin::AudioReader::WriteOptions options)
{
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

    if(options & Fooyin::AudioReader::Rating) {
        id3Tags->removeFrames("FMPS_Rating");

        const auto rating = QString::number(track.rating());
        auto ratingFrame
            = std::make_unique<TagLib::ID3v2::TextIdentificationFrame>("FMPS_Rating", TagLib::String::UTF8);
        ratingFrame->setText(convertString(rating));
        id3Tags->addFrame(ratingFrame.release());

        TagLib::ID3v2::PopularimeterFrame* frame{nullptr};
        const TagLib::ID3v2::FrameListMap& map = id3Tags->frameListMap();
        if(map.contains("POPM")) {
            frame = dynamic_cast<TagLib::ID3v2::PopularimeterFrame*>(map["POPM"].front());
        }

        if(!frame) {
            frame = new TagLib::ID3v2::PopularimeterFrame();
            id3Tags->addFrame(frame);
        }

        frame->setRating(ratingToPopm(track.rating()));
    }

    if(options & Fooyin::AudioReader::Playcount) {
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

void readApeTags(const TagLib::APE::Tag* apeTags, Fooyin::Track& track)
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

    if(items.contains("FMPS_RATING") && track.rating() <= 0) {
        const float rating = convertString(items["FMPS_RATING"].toString()).toFloat();
        if(rating > 0 && rating <= 1.0) {
            track.setRating(rating);
        }
    }

    if(items.contains("FMPS_PLAYCOUNT") && track.rating() <= 0) {
        const int count = convertString(items["FMPS_PLAYCOUNT"].toString()).toInt();
        if(count > 0) {
            track.setPlayCount(count);
        }
    }
}

QByteArray readApeCover(const TagLib::APE::Tag* apeTags, Fooyin::Track::Cover cover)
{
    if(apeTags->isEmpty()) {
        return {};
    }

    const TagLib::APE::ItemListMap& items = apeTags->itemListMap();

    const TagLib::String coverType = cover == Fooyin::Track::Cover::Front ? "COVER ART (FRONT)"
                                   : cover == Fooyin::Track::Cover::Back  ? "COVER ART (BACK)"
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

void writeApeTags(TagLib::APE::Tag* apeTags, const Fooyin::Track& track, Fooyin::AudioReader::WriteOptions options)
{
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

    if(options & Fooyin::AudioReader::Rating) {
        if(track.rating() <= 0) {
            apeTags->removeItem("FMPS_RATING");
        }
        else {
            apeTags->setItem("FMPS_RATING", {"FMPS_RATING", convertString(QString::number(track.rating()))});
        }
    }

    if(options & Fooyin::AudioReader::Playcount) {
        if(track.playCount() <= 0) {
            apeTags->removeItem("FMPS_PLAYCOUNT");
        }
        else {
            apeTags->setItem("FMPS_PLAYCOUNT", {"FMPS_PLAYCOUNT", convertString(QString::number(track.rating()))});
        }
    }
}

QString stripMp4FreeFormName(const TagLib::String& name)
{
    QString freeFormName = convertString(name);

    if(freeFormName.startsWith(u"----")) {
        qsizetype nameStart = freeFormName.lastIndexOf(u":");
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

void readMp4Tags(const TagLib::MP4::Tag* mp4Tags, Fooyin::Track& track, bool skipExtra = false)
{
    if(mp4Tags->isEmpty()) {
        return;
    }

    const auto& items = mp4Tags->itemMap();

    if(items.contains(Fooyin::Mp4::PerformerAlt)) {
        const auto performer = items[Fooyin::Mp4::PerformerAlt].toStringList();
        if(performer.size() > 0) {
            track.setPerformer(convertString(performer.toString()));
        }
    }

    if(items.contains(Fooyin::Mp4::Track)) {
        const TagLib::MP4::Item::IntPair& trackNumbers = items[Fooyin::Mp4::Track].toIntPair();
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
        track.setRating(convertRating(rating));
    }

    if(items.contains(Fooyin::Mp4::RatingAlt) && track.rating() <= 0) {
        const float rating = convertString(items[Fooyin::Mp4::RatingAlt].toStringList().toString("\n")).toFloat();
        if(rating > 0) {
            track.setRating(rating);
        }
    }

    if(items.contains(Fooyin::Mp4::PlayCount) && track.playCount() <= 0) {
        const int count = items[Fooyin::Mp4::PlayCount].toInt();
        if(count > 0) {
            track.setPlayCount(count);
        }
    }

    if(!skipExtra) {
        using namespace Fooyin::Mp4;
        static const std::set<TagLib::String> baseMp4Tags
            = {Title,      Artist,        Album,  AlbumArtist, Genre,      Composer,    Performer, PerformerAlt,
               Comment,    Date,          Rating, RatingAlt,   RatingAlt2, PlayCount,   Track,     TrackAlt,
               TrackTotal, TrackTotalAlt, Disc,   DiscAlt,     DiscTotal,  DiscTotalAlt};

        track.clearExtraTags();

        for(const auto& [key, item] : items) {
            if(!baseMp4Tags.contains(key)) {
                QString tagName = findMp4Tag(key);
                if(tagName.isEmpty()) {
                    tagName = stripMp4FreeFormName(key);
                }
                const auto values = convertStringList(item.toStringList());
                for(const auto& value : values) {
                    track.addExtraTag(tagName, value);
                }
            }
        }
    }
}

QByteArray readMp4Cover(const TagLib::MP4::Tag* mp4Tags, Fooyin::Track::Cover /*cover*/)
{
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

    if(name.startsWith(u"----") || (name.length() == 4 && name[0] == u'\251')) {
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
        if(static_cast<int>(key.length()) >= len && key.substr(key.length() - len, len) == tagKey) {
            freeFormName = key;
            break;
        }
    }

    return freeFormName;
}

void writeMp4Tags(TagLib::MP4::Tag* mp4Tags, const Fooyin::Track& track, Fooyin::AudioReader::WriteOptions options)
{
    const QString trackNumber = track.trackNumber();
    const QString trackTotal  = track.trackTotal();

    mp4Tags->removeItem(Fooyin::Mp4::Track);
    mp4Tags->removeItem(Fooyin::Mp4::TrackAlt);
    mp4Tags->removeItem(Fooyin::Mp4::TrackTotal);
    mp4Tags->removeItem(Fooyin::Mp4::TrackTotalAlt);

    if(!trackNumber.isEmpty() || !trackTotal.isEmpty()) {
        bool numOk{false};
        const int num = trackNumber.toInt(&numOk);
        bool totalOk{false};
        const int total = trackTotal.toInt(&totalOk);

        if(numOk && totalOk) {
            mp4Tags->setItem(Fooyin::Mp4::Track, {num, total});
        }
        else {
            if(!trackNumber.isEmpty()) {
                mp4Tags->setItem(Fooyin::Mp4::TrackAlt, {convertString(trackNumber)});
            }
            if(!trackTotal.isEmpty()) {
                mp4Tags->setItem(Fooyin::Mp4::TrackTotalAlt, {convertString(trackTotal)});
            }
        }
    }

    const QString discNumber = track.discNumber();
    const QString discTotal  = track.discTotal();

    mp4Tags->removeItem(Fooyin::Mp4::Disc);
    mp4Tags->removeItem(Fooyin::Mp4::DiscAlt);
    mp4Tags->removeItem(Fooyin::Mp4::DiscTotal);
    mp4Tags->removeItem(Fooyin::Mp4::DiscTotalAlt);

    if(!discNumber.isEmpty() || !discTotal.isEmpty()) {
        bool numOk{false};
        const int num = discNumber.toInt(&numOk);
        bool totalOk{false};
        const int total = discTotal.toInt(&totalOk);

        if(numOk && totalOk) {
            mp4Tags->setItem(Fooyin::Mp4::Disc, {num, total});
        }
        else {
            if(!discNumber.isEmpty()) {
                mp4Tags->setItem(Fooyin::Mp4::DiscAlt, {convertString(discNumber)});
            }
            if(!discTotal.isEmpty()) {
                mp4Tags->setItem(Fooyin::Mp4::DiscTotalAlt, {convertString(discTotal)});
            }
        }
    }

    mp4Tags->setItem(Fooyin::Mp4::PerformerAlt, {convertString(track.performer())});

    if(options & Fooyin::AudioReader::Rating) {
        mp4Tags->setItem(Fooyin::Mp4::RatingAlt, {convertString(QString::number(track.rating()))});
    }

    if(options & Fooyin::AudioReader::Playcount) {
        if(track.playCount() <= 0) {
            mp4Tags->removeItem(Fooyin::Mp4::PlayCount);
        }
        else {
            mp4Tags->setItem(Fooyin::Mp4::PlayCount, {convertString(QString::number(track.playCount()))});
        }
    }

    using namespace Fooyin::Tag;
    static const std::set<QString> baseMp4Tags
        = {QString::fromLatin1(Title),         QString::fromLatin1(Artist),      QString::fromLatin1(Album),
           QString::fromLatin1(AlbumArtist),   QString::fromLatin1(Genre),       QString::fromLatin1(Composer),
           QString::fromLatin1(Performer),     QString::fromLatin1(Comment),     QString::fromLatin1(Date),
           QString::fromLatin1(Rating),        QString::fromLatin1(RatingAlt),   QString::fromLatin1(PlayCount),
           QString::fromLatin1(Track),         QString::fromLatin1(TrackAlt),    QString::fromLatin1(TrackTotal),
           QString::fromLatin1(TrackTotalAlt), QString::fromLatin1(Disc),        QString::fromLatin1(DiscAlt),
           QString::fromLatin1(DiscTotal),     QString::fromLatin1(DiscTotalAlt)};

    const auto customTags = track.extraTags();
    for(const auto& [tag, values] : Fooyin::Utils::asRange(customTags)) {
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

void readXiphComment(const TagLib::Ogg::XiphComment* xiphTags, Fooyin::Track& track)
{
    if(xiphTags->isEmpty()) {
        return;
    }

    const TagLib::Ogg::FieldListMap& fields = xiphTags->fieldListMap();

    if(fields.contains(Fooyin::Tag::Track)) {
        const TagLib::StringList& trackNumber = fields[Fooyin::Tag::Track];
        if(!trackNumber.isEmpty() && !trackNumber.front().isEmpty()) {
            track.setTrackNumber(convertString(trackNumber.front()));
        }
    }

    if(fields.contains(Fooyin::Tag::TrackTotal)) {
        const TagLib::StringList& trackTotal = fields[Fooyin::Tag::TrackTotal];
        if(!trackTotal.isEmpty() && !trackTotal.front().isEmpty()) {
            track.setTrackTotal(convertString(trackTotal.front()));
        }
    }

    if(fields.contains(Fooyin::Tag::Disc)) {
        const TagLib::StringList& discNumber = fields[Fooyin::Tag::Disc];
        if(!discNumber.isEmpty() && !discNumber.front().isEmpty()) {
            track.setDiscNumber(convertString(discNumber.front()));
        }
    }

    if(fields.contains(Fooyin::Tag::DiscTotal)) {
        const TagLib::StringList& discTotal = fields[Fooyin::Tag::DiscTotal];
        if(!discTotal.isEmpty() && !discTotal.front().isEmpty()) {
            track.setDiscTotal(convertString(discTotal.front()));
        }
    }

    if(fields.contains("FMPS_RATING") && track.rating() <= 0) {
        const TagLib::StringList& ratings = fields["FMPS_RATING"];
        if(!ratings.isEmpty()) {
            const float rating = convertString(ratings.front()).toFloat();
            if(rating > 0 && rating <= 1.0) {
                track.setRating(rating);
            }
        }
    }

    if(fields.contains("FMPS_PLAYCOUNT") && track.playCount() <= 0) {
        const TagLib::StringList& countList = fields["FMPS_PLAYCOUNT"];
        if(!countList.isEmpty()) {
            const int count = convertString(countList.front()).toInt();
            if(count > 0) {
                track.setPlayCount(count);
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
                      Fooyin::AudioReader::WriteOptions options)
{
    if(track.trackNumber().isEmpty()) {
        xiphTags->removeFields(Fooyin::Tag::Track);
    }
    else {
        xiphTags->addField(Fooyin::Tag::Track, convertString(track.trackNumber()), true);
    }

    if(track.trackTotal().isEmpty()) {
        xiphTags->removeFields(Fooyin::Tag::TrackTotal);
    }
    else {
        xiphTags->addField(Fooyin::Tag::TrackTotal, convertString(track.trackTotal()), true);
    }

    if(track.discNumber().isEmpty()) {
        xiphTags->removeFields(Fooyin::Tag::Disc);
    }
    else {
        xiphTags->addField(Fooyin::Tag::Disc, convertString(track.discNumber()), true);
    }

    if(track.discTotal().isEmpty()) {
        xiphTags->removeFields(Fooyin::Tag::DiscTotal);
    }
    else {
        xiphTags->addField(Fooyin::Tag::DiscTotal, convertString(track.discTotal()), true);
    }

    if(options & Fooyin::AudioReader::Rating) {
        if(track.rating() <= 0) {
            xiphTags->removeFields("FMPS_RATING");
        }
        else {
            xiphTags->addField("FMPS_RATING", convertString(QString::number(track.rating())), true);
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

void readAsfTags(const TagLib::ASF::Tag* asfTags, Fooyin::Track& track)
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

    if(map.contains("FMPS/Rating") && track.rating() <= 0) {
        const TagLib::ASF::AttributeList& ratings = map["FMPS/Rating"];
        if(!ratings.isEmpty()) {
            const float rate = convertString(ratings.front().toString()).toFloat();
            if(rate > 0 && rate <= 1.0) {
                track.setRating(rate);
            }
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

    auto convertRating = [](int rating) -> float {
        if(rating == 0) {
            return 0.0;
        }
        if(rating < 25) {
            return 0.2;
        }
        if(rating < 50) {
            return 0.4;
        }
        if(rating < 75) {
            return 0.6;
        }
        if(rating < 99) {
            return 0.8;
        }

        return 1.0;
    };

    if(map.contains("WM/SharedUserRating") && track.rating() <= 0) {
        const TagLib::ASF::AttributeList& ratings = map["WM/SharedUserRating"];
        if(!ratings.isEmpty()) {
            const auto rating = static_cast<int>(ratings.front().toUInt());
            if(rating > 0 && rating <= 100) {
                track.setRating(convertRating(rating));
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

void writeAsfTags(TagLib::ASF::Tag* asfTags, const Fooyin::Track& track, Fooyin::AudioReader::WriteOptions options)
{
    asfTags->setAttribute("WM/TrackNumber", convertString(track.trackNumber()));
    asfTags->setAttribute("WM/PartOfSet", convertString(track.discNumber()));

    if(options & Fooyin::AudioReader::Rating) {
        asfTags->addAttribute("FMPS/Rating", convertString(QString::number(track.rating())));
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
} // namespace

namespace Fooyin {
QStringList TagLibReader::extensions() const
{
    static const QStringList extensions{QStringLiteral("mp3"),  QStringLiteral("ogg"),  QStringLiteral("opus"),
                                        QStringLiteral("oga"),  QStringLiteral("m4a"),  QStringLiteral("wav"),
                                        QStringLiteral("wv"),   QStringLiteral("flac"), QStringLiteral("wma"),
                                        QStringLiteral("mpc"),  QStringLiteral("aiff"), QStringLiteral("ape"),
                                        QStringLiteral("webm"), QStringLiteral("mp4")};
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

bool TagLibReader::readTrack(const AudioSource& source, Track& track)
{
    IODeviceStream stream{source.device, track.filename()};
    if(!stream.isOpen()) {
        qCWarning(TAGLIB) << "Unable to open file readonly:" << source.filepath;
        return false;
    }

    const QMimeDatabase mimeDb;
    QString mimeType = mimeDb.mimeTypeForFile(source.filepath).name();
    const auto style = TagLib::AudioProperties::Average;

    const auto readProperties = [&track](const TagLib::File& file) {
        readAudioProperties(file, track);
        readGeneralProperties(file.properties(), track);
    };

    if(mimeType == u"audio/ogg" || mimeType == u"audio/x-vorbis+ogg") {
        // Workaround for opus files with ogg suffix returning incorrect type
        mimeType = mimeDb.mimeTypeForData(source.device).name();
    }
    if(mimeType == u"audio/mpeg" || mimeType == u"audio/mpeg3" || mimeType == u"audio/x-mpeg") {
#if(TAGLIB_MAJOR_VERSION >= 2)
        TagLib::MPEG::File file(&stream, true, style, TagLib::ID3v2::FrameFactory::instance());
#else
        TagLib::MPEG::File file(&stream, TagLib::ID3v2::FrameFactory::instance(), true, style);
#endif
        if(file.isValid()) {
            readProperties(file);
            if(file.hasID3v2Tag()) {
                readId3Tags(file.ID3v2Tag(), track);
            }
        }
    }
    else if(mimeType == u"audio/x-aiff" || mimeType == u"audio/x-aifc") {
        const TagLib::RIFF::AIFF::File file(&stream, true, style);
        if(file.isValid()) {
            readProperties(file);
            if(const int bps = file.audioProperties()->bitsPerSample(); bps > 0) {
                track.setBitDepth(bps);
            }
            if(file.hasID3v2Tag()) {
                readId3Tags(file.tag(), track);
            }
        }
    }
    else if(mimeType == u"audio/vnd.wave" || mimeType == u"audio/wav" || mimeType == u"audio/x-wav") {
        const TagLib::RIFF::WAV::File file(&stream, true, style);
        if(file.isValid()) {
            readProperties(file);
            if(const int bps = file.audioProperties()->bitsPerSample(); bps > 0) {
                track.setBitDepth(bps);
            }
            if(file.hasID3v2Tag()) {
                readId3Tags(file.ID3v2Tag(), track);
            }
        }
    }
    else if(mimeType == u"audio/x-musepack") {
        TagLib::MPC::File file(&stream, true, style);
        if(file.isValid()) {
            readProperties(file);
            if(file.APETag()) {
                readApeTags(file.APETag(), track);
            }
        }
    }
    else if(mimeType == u"audio/x-ape") {
        TagLib::APE::File file(&stream, true, style);
        if(file.isValid()) {
            readProperties(file);
            if(const int bps = file.audioProperties()->bitsPerSample(); bps > 0) {
                track.setBitDepth(bps);
            }
            if(file.APETag()) {
                readApeTags(file.APETag(), track);
            }
        }
    }
    else if(mimeType == u"audio/x-wavpack") {
        TagLib::WavPack::File file(&stream, true, style);
        if(file.isValid()) {
            readProperties(file);
            if(const int bps = file.audioProperties()->bitsPerSample(); bps > 0) {
                track.setBitDepth(bps);
            }
            if(file.APETag()) {
                readApeTags(file.APETag(), track);
            }
        }
    }
    else if(mimeType == u"audio/mp4" || mimeType == u"audio/vnd.audible.aax") {
        const TagLib::MP4::File file(&stream, true, style);
        if(file.isValid()) {
            readProperties(file);
            const auto codec = file.audioProperties()->codec();
            switch(codec) {
                case(TagLib::MP4::Properties::AAC):
                    track.setCodec(QStringLiteral("AAC"));
                    break;
                case(TagLib::MP4::Properties::ALAC):
                    track.setCodec(QStringLiteral("ALAC"));
                    break;
                case(TagLib::MP4::Properties::Unknown):
                    break;
            }

            if(const int bps = file.audioProperties()->bitsPerSample(); bps > 0) {
                track.setBitDepth(bps);
            }
            if(file.hasMP4Tag()) {
                readMp4Tags(file.tag(), track);
            }
        }
    }
    else if(mimeType == u"audio/flac") {
#if(TAGLIB_MAJOR_VERSION >= 2)
        TagLib::FLAC::File file(&stream, true, style, TagLib::ID3v2::FrameFactory::instance());
#else
        TagLib::FLAC::File file(&stream, TagLib::ID3v2::FrameFactory::instance(), true, style);
#endif
        if(file.isValid()) {
            readProperties(file);
            if(const int bps = file.audioProperties()->bitsPerSample(); bps > 0) {
                track.setBitDepth(bps);
            }
            if(file.hasXiphComment()) {
                readXiphComment(file.xiphComment(), track);
            }
        }
    }
    else if(mimeType == u"audio/ogg" || mimeType == u"audio/x-vorbis+ogg" || mimeType == u"application/ogg") {
        const TagLib::Ogg::Vorbis::File file(&stream, true, style);
        if(file.isValid()) {
            readProperties(file);
            if(file.tag()) {
                readXiphComment(file.tag(), track);
            }
        }
    }
    else if(mimeType == u"audio/opus" || mimeType == u"audio/x-opus+ogg") {
        const TagLib::Ogg::Opus::File file(&stream, true, style);
        if(file.isValid()) {
            readProperties(file);
            if(file.tag()) {
                readXiphComment(file.tag(), track);
            }
        }
    }
    else if(mimeType == u"audio/x-ms-wma") {
        const TagLib::ASF::File file(&stream, true, style);
        if(file.isValid()) {
            readProperties(file);
            if(const int bps = file.audioProperties()->bitsPerSample(); bps > 0) {
                track.setBitDepth(bps);
            }
            if(file.tag()) {
                readAsfTags(file.tag(), track);
            }
        }
    }
    else {
        qCInfo(TAGLIB) << "Unsupported mime type:" << mimeType;
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

    if(mimeType == u"audio/ogg" || mimeType == u"audio/x-vorbis+ogg") {
        // Workaround for opus files with ogg suffix returning incorrect type
        mimeType = mimeDb.mimeTypeForData(source.device).name();
    }
    if(mimeType == u"audio/mpeg" || mimeType == u"audio/mpeg3" || mimeType == u"audio/x-mpeg") {
#if(TAGLIB_MAJOR_VERSION >= 2)
        TagLib::MPEG::File file(&stream, true, style, TagLib::ID3v2::FrameFactory::instance());
#else
        TagLib::MPEG::File file(&stream, TagLib::ID3v2::FrameFactory::instance(), true, style);
#endif
        if(file.isValid() && file.hasID3v2Tag()) {
            return readId3Cover(file.ID3v2Tag(), cover);
        }
    }
    else if(mimeType == u"audio/x-aiff" || mimeType == u"audio/x-aifc") {
        const TagLib::RIFF::AIFF::File file(&stream, true);
        if(file.isValid() && file.hasID3v2Tag()) {
            return readId3Cover(file.tag(), cover);
        }
    }
    else if(mimeType == u"audio/vnd.wave" || mimeType == u"audio/wav" || mimeType == u"audio/x-wav") {
        const TagLib::RIFF::WAV::File file(&stream, true);
        if(file.isValid() && file.hasID3v2Tag()) {
            return readId3Cover(file.ID3v2Tag(), cover);
        }
    }
    else if(mimeType == u"audio/x-musepack") {
        TagLib::MPC::File file(&stream, true);
        if(file.isValid() && file.APETag()) {
            return readApeCover(file.APETag(), cover);
        }
    }
    else if(mimeType == u"audio/x-ape") {
        TagLib::APE::File file(&stream, true);
        if(file.isValid() && file.APETag()) {
            return readApeCover(file.APETag(), cover);
        }
    }
    else if(mimeType == u"audio/x-wavpack") {
        TagLib::WavPack::File file(&stream, true);
        if(file.isValid() && file.APETag()) {
            return readApeCover(file.APETag(), cover);
        }
    }
    else if(mimeType == u"audio/mp4" || mimeType == u"audio/vnd.audible.aax") {
        const TagLib::MP4::File file(&stream, true);
        if(file.isValid() && file.tag()) {
            return readMp4Cover(file.tag(), cover);
        }
    }
    else if(mimeType == u"audio/flac") {
#if(TAGLIB_MAJOR_VERSION >= 2)
        TagLib::FLAC::File file(&stream, true, style, TagLib::ID3v2::FrameFactory::instance());
#else
        TagLib::FLAC::File file(&stream, TagLib::ID3v2::FrameFactory::instance(), true, style);
#endif
        if(file.isValid()) {
            return readFlacCover(file.pictureList(), cover);
        }
    }
    else if(mimeType == u"audio/ogg" || mimeType == u"audio/x-vorbis+ogg" || mimeType == u"application/ogg") {
        const TagLib::Ogg::Vorbis::File file(&stream, true);
        if(file.isValid() && file.tag()) {
            return readFlacCover(file.tag()->pictureList(), cover);
        }
    }
    else if(mimeType == u"audio/opus" || mimeType == u"audio/x-opus+ogg") {
        const TagLib::Ogg::Opus::File file(&stream, true);
        if(file.isValid() && file.tag()) {
            return readFlacCover(file.tag()->pictureList(), cover);
        }
    }
    else if(mimeType == u"audio/x-ms-wma") {
        const TagLib::ASF::File file(&stream, true);
        if(file.isValid() && file.tag()) {
            return readAsfCover(file.tag(), cover);
        }
    }
    else {
        qCInfo(TAGLIB) << "Unsupported mime type:" << mimeType;
        return {};
    }

    return {};
}

bool TagLibReader::writeTrack(const AudioSource& source, const Track& track, AudioReader::WriteOptions options)
{
    IODeviceStream stream{source.device, track.filepath()};
    if(!stream.isOpen() || stream.readOnly()) {
        return false;
    }

    const auto writeProperties = [&track](TagLib::File& file, bool skipExtra = false) {
        auto savedProperties = file.properties();
        writeGenericProperties(savedProperties, track, skipExtra);
        file.setProperties(savedProperties);
    };

    const QMimeDatabase mimeDb;
    QString mimeType = mimeDb.mimeTypeForFile(source.filepath).name();
    const auto style = TagLib::AudioProperties::Average;

    if(mimeType == u"audio/ogg" || mimeType == u"audio/x-vorbis+ogg") {
        // Workaround for opus files with ogg suffix returning incorrect type
        mimeType = mimeDb.mimeTypeForData(source.device).name();
    }
    if(mimeType == u"audio/mpeg" || mimeType == u"audio/mpeg3" || mimeType == u"audio/x-mpeg") {
#if(TAGLIB_MAJOR_VERSION >= 2)
        TagLib::MPEG::File file(&stream, true, style, TagLib::ID3v2::FrameFactory::instance());
#else
        TagLib::MPEG::File file(&stream, TagLib::ID3v2::FrameFactory::instance(), false, style);
#endif
        if(file.isValid()) {
            writeProperties(file);
            if(file.hasID3v2Tag()) {
                writeID3v2Tags(file.ID3v2Tag(), track, options);
            }
            file.save();
        }
    }
    else if(mimeType == u"audio/x-aiff" || mimeType == u"audio/x-aifc") {
        TagLib::RIFF::AIFF::File file(&stream, false);
        if(file.isValid()) {
            writeProperties(file);
            if(file.hasID3v2Tag()) {
                writeID3v2Tags(file.tag(), track, options);
            }
            file.save();
        }
    }
    else if(mimeType == u"audio/vnd.wave" || mimeType == u"audio/wav" || mimeType == u"audio/x-wav") {
        TagLib::RIFF::WAV::File file(&stream, false);
        if(file.isValid()) {
            writeProperties(file);
            if(file.hasID3v2Tag()) {
                writeID3v2Tags(file.ID3v2Tag(), track, options);
            }
            file.save();
        }
    }
    else if(mimeType == u"audio/x-musepack") {
        TagLib::MPC::File file(&stream, false);
        if(file.isValid()) {
            writeProperties(file);
            if(file.hasAPETag()) {
                writeApeTags(file.APETag(), track, options);
            }
            file.save();
        }
    }
    else if(mimeType == u"audio/x-ape") {
        TagLib::APE::File file(&stream, false);
        if(file.isValid()) {
            writeProperties(file);
            if(file.hasAPETag()) {
                writeApeTags(file.APETag(), track, options);
            }
            file.save();
        }
    }
    else if(mimeType == u"audio/x-wavpack") {
        TagLib::WavPack::File file(&stream, false);
        if(file.isValid()) {
            writeProperties(file);
            if(file.hasAPETag()) {
                writeApeTags(file.APETag(), track, options);
            }
            file.save();
        }
    }
    else if(mimeType == u"audio/mp4") {
        TagLib::MP4::File file(&stream, false);
        if(file.isValid()) {
            writeProperties(file, true);
            if(file.hasMP4Tag()) {
                writeMp4Tags(file.tag(), track, options);
            }
            file.save();
        }
    }
    else if(mimeType == u"audio/flac") {
#if(TAGLIB_MAJOR_VERSION >= 2)
        TagLib::FLAC::File file(&stream, true, style, TagLib::ID3v2::FrameFactory::instance());
#else
        TagLib::FLAC::File file(&stream, TagLib::ID3v2::FrameFactory::instance(), false, style);
#endif
        if(file.isValid()) {
            writeProperties(file);
            if(file.hasXiphComment()) {
                writeXiphComment(file.xiphComment(), track, options);
            }
            file.save();
        }
    }
    else if(mimeType == u"audio/ogg" || mimeType == u"audio/x-vorbis+ogg" || mimeType == u"application/ogg") {
        TagLib::Ogg::Vorbis::File file(&stream, false);
        if(file.isValid()) {
            writeProperties(file);
            if(file.tag()) {
                writeXiphComment(file.tag(), track, options);
            }
            file.save();
        }
    }
    else if(mimeType == u"audio/opus" || mimeType == u"audio/x-opus+ogg") {
        TagLib::Ogg::Opus::File file(&stream, false);
        if(file.isValid()) {
            writeProperties(file);
            writeXiphComment(file.tag(), track, options);
            file.save();
        }
    }
    else if(mimeType == u"audio/x-ms-wma") {
        TagLib::ASF::File file(&stream, false);
        if(file.isValid()) {
            writeProperties(file);
            if(file.tag()) {
                writeAsfTags(file.tag(), track, options);
            }
            file.save();
        }
    }
    else {
        qCInfo(TAGLIB) << "Unsupported mime type:" << mimeType;
        return false;
    }

    return true;
}
} // namespace Fooyin
