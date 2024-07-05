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

#include <core/constants.h>
#include <core/track.h>
#include <utils/helpers.h>

#include <taglib/aifffile.h>
#include <taglib/apefile.h>
#include <taglib/apetag.h>
#include <taglib/asffile.h>
#include <taglib/asfpicture.h>
#include <taglib/asftag.h>
#include <taglib/attachedpictureframe.h>
#include <taglib/fileref.h>
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
#include <QMimeDatabase>
#include <QPixmap>

#include <set>

namespace {
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
    std::pair(Fooyin::Mp4::TrackNumber, Fooyin::Tag::TrackNumber),
    std::pair(Fooyin::Mp4::DiscNumber, Fooyin::Tag::DiscNumber),
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
    std::pair(Fooyin::Tag::DiscNumber, Fooyin::Mp4::DiscNumber),
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
        if(tag == QString::fromLatin1(key)) {
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

Fooyin::Track::Type typeForMime(const QString& mimeType)
{
    if(mimeType == QStringLiteral("audio/mpeg") || mimeType == QStringLiteral("audio/mpeg3")
       || mimeType == QStringLiteral("audio/x-mpeg")) {
        return Fooyin::Track::Type::MPEG;
    }
    if(mimeType == QStringLiteral("audio/x-aiff") || mimeType == QStringLiteral("audio/x-aifc")) {
        return Fooyin::Track::Type::AIFF;
    }
    if(mimeType == QStringLiteral("audio/vnd.wave") || mimeType == QStringLiteral("audio/wav")
       || mimeType == QStringLiteral("audio/x-wav")) {
        return Fooyin::Track::Type::WAV;
    }
    if(mimeType == QStringLiteral("audio/x-musepack")) {
        return Fooyin::Track::Type::MPC;
    }
    if(mimeType == QStringLiteral("audio/x-ape")) {
        return Fooyin::Track::Type::APE;
    }
    if(mimeType == QStringLiteral("audio/x-wavpack")) {
        return Fooyin::Track::Type::WavPack;
    }
    if(mimeType == QStringLiteral("audio/mp4") || mimeType == QStringLiteral("audio/vnd.audible.aax")) {
        return Fooyin::Track::Type::MP4;
    }
    if(mimeType == QStringLiteral("audio/flac")) {
        return Fooyin::Track::Type::FLAC;
    }
    if(mimeType == QStringLiteral("audio/ogg") || mimeType == QStringLiteral("audio/x-vorbis+ogg")) {
        return Fooyin::Track::Type::OggVorbis;
    }
    if(mimeType == QStringLiteral("audio/opus") || mimeType == QStringLiteral("audio/x-opus+ogg")) {
        return Fooyin::Track::Type::OggOpus;
    }
    if(mimeType == QStringLiteral("audio/x-ms-wma")) {
        return Fooyin::Track::Type::ASF;
    }

    return Fooyin::Track::Type::Unknown;
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

void readGeneralProperties(const TagLib::PropertyMap& props, Fooyin::Track& track, bool skipExtra = false)
{
    if(props.isEmpty()) {
        return;
    }

    if(props.contains(Fooyin::Tag::Title)) {
        track.setTitle(convertString(props[Fooyin::Tag::Title].toString()));
    }
    if(props.contains(Fooyin::Tag::Artist)) {
        track.setArtists(convertStringList(props[Fooyin::Tag::Artist]));
    }
    if(props.contains(Fooyin::Tag::Album)) {
        track.setAlbum(convertString(props[Fooyin::Tag::Album].toString()));
    }
    if(props.contains(Fooyin::Tag::AlbumArtist)) {
        track.setAlbumArtists(convertStringList(props[Fooyin::Tag::AlbumArtist]));
    }
    if(props.contains(Fooyin::Tag::Genre)) {
        track.setGenres(convertStringList(props[Fooyin::Tag::Genre]));
    }
    if(props.contains(Fooyin::Tag::Composer)) {
        track.setComposer(convertString(props[Fooyin::Tag::Composer].toString()));
    }
    if(props.contains(Fooyin::Tag::Performer)) {
        track.setPerformer(convertString(props[Fooyin::Tag::Performer].toString()));
    }
    if(props.contains(Fooyin::Tag::Comment)) {
        track.setComment(convertString(props[Fooyin::Tag::Comment].toString()));
    }
    if(props.contains(Fooyin::Tag::Date)) {
        track.setDate(convertString(props[Fooyin::Tag::Date].toString()));
    }
    if(props.contains(Fooyin::Tag::Rating) && track.rating() <= 0) {
        const int rating = props[Fooyin::Tag::Rating].front().toInt();
        if(rating > 0 && rating <= 100) {
            float adjustedRating = static_cast<float>(rating) / 10;
            if(adjustedRating > 1.0) {
                // Assume a scale of 0-100
                adjustedRating /= 10;
            }
            else {
                // Assume 1=1 star
                adjustedRating *= 2;
            }
            track.setRating(adjustedRating);
        }
    }

    if(!skipExtra) {
        static const std::set<TagLib::String> baseTags
            = {Fooyin::Tag::Title,       Fooyin::Tag::Artist,     Fooyin::Tag::Album,      Fooyin::Tag::AlbumArtist,
               Fooyin::Tag::TrackNumber, Fooyin::Tag::TrackTotal, Fooyin::Tag::DiscNumber, Fooyin::Tag::DiscTotal,
               Fooyin::Tag::Genre,       Fooyin::Tag::Composer,   Fooyin::Tag::Performer,  Fooyin::Tag::Comment,
               Fooyin::Tag::Date,        Fooyin::Tag::Rating};

        track.clearExtraTags();

        for(const auto& [tag, values] : props) {
            if(!baseTags.contains(tag)) {
                const auto tagEntry = QString::fromStdString(tag.to8Bit(true));
                for(const auto& value : values) {
                    track.addExtraTag(tagEntry, QString::fromStdString(value.to8Bit(true)));
                }
            }
        }
    }
}

void writeGenericProperties(TagLib::PropertyMap& oldProperties, const Fooyin::Track& track, bool skipExtra = false)
{
    if(!track.isValid()) {
        return;
    }

    if(!track.title().isEmpty()) {
        oldProperties.replace(Fooyin::Tag::Title, convertString(track.title()));
    }

    if(!track.artists().empty()) {
        oldProperties.replace(Fooyin::Tag::Artist, convertStringList(track.artists()));
    }

    if(!track.album().isEmpty()) {
        oldProperties.replace(Fooyin::Tag::Album, convertString(track.album()));
    }

    if(!track.albumArtist().isEmpty()) {
        oldProperties.replace(Fooyin::Tag::AlbumArtist, convertString(track.albumArtist()));
    }

    if(track.trackNumber() >= 0) {
        const auto trackNums
            = TStringToQString(oldProperties[Fooyin::Tag::TrackNumber].toString()).split(QStringLiteral("/"));
        QString trackNumber = QString::number(track.trackNumber());
        if(trackNums.size() > 1) {
            trackNumber += QStringLiteral("/") + QString::number(track.trackTotal());
        }
        oldProperties.replace(Fooyin::Tag::TrackNumber, convertString(trackNumber));
    }

    if(track.discNumber() >= 0) {
        const auto discNums
            = TStringToQString(oldProperties[Fooyin::Tag::DiscNumber].toString()).split(QStringLiteral("/"));
        QString discNumber = QString::number(track.discNumber());
        if(discNums.size() > 1) {
            discNumber += QStringLiteral("/") + QString::number(track.discTotal());
        }
        oldProperties.replace(Fooyin::Tag::DiscNumber, convertString(discNumber));
    }

    if(!track.genres().empty()) {
        oldProperties.replace(Fooyin::Tag::Genre, convertStringList(track.genres()));
    }

    if(!track.composer().isEmpty()) {
        oldProperties.replace(Fooyin::Tag::Composer, convertString(track.composer()));
    }

    if(!track.performer().isEmpty()) {
        oldProperties.replace(Fooyin::Tag::Performer, convertString(track.performer()));
    }

    if(!track.comment().isEmpty()) {
        oldProperties.replace(Fooyin::Tag::Comment, convertString(track.comment()));
    }

    if(!track.date().isEmpty()) {
        oldProperties.replace(Fooyin::Tag::Date, convertString(track.date()));
    }

    if(!skipExtra) {
        static const std::set<TagLib::String> baseTags
            = {Fooyin::Tag::Title,       Fooyin::Tag::Artist,     Fooyin::Tag::Album,      Fooyin::Tag::AlbumArtist,
               Fooyin::Tag::TrackNumber, Fooyin::Tag::TrackTotal, Fooyin::Tag::DiscNumber, Fooyin::Tag::DiscTotal,
               Fooyin::Tag::Genre,       Fooyin::Tag::Composer,   Fooyin::Tag::Performer,  Fooyin::Tag::Comment,
               Fooyin::Tag::Date,        Fooyin::Tag::Rating};

        const auto customTags = track.extraTags();
        for(const auto& [tag, values] : Fooyin::Utils::asRange(customTags)) {
            const TagLib::String name = convertString(tag);
            if(!baseTags.contains(name)) {
                oldProperties.replace(name, convertStringList(values));
            }
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
    QString trackNumber;
    if(track.trackNumber() > 0) {
        trackNumber += QString::number(track.trackNumber());
    }
    if(track.trackTotal() > 0) {
        trackNumber += QStringLiteral("/") + QString::number(track.trackTotal());
    }
    return trackNumber;
}

QString getDiscNumber(const Fooyin::Track& track)
{
    QString discNumber;
    if(track.discNumber() > 0) {
        discNumber += QString::number(track.discNumber());
    }
    if(track.discTotal() > 0) {
        discNumber += QStringLiteral("/") + QString::number(track.discTotal());
    }
    return discNumber;
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

void readId3Tags(const TagLib::ID3v2::Tag* id3Tags, Fooyin::Track& track)
{
    if(id3Tags->isEmpty()) {
        return;
    }

    const TagLib::ID3v2::FrameListMap& frames = id3Tags->frameListMap();

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

void writeID3v2Tags(TagLib::ID3v2::Tag* id3Tags, const Fooyin::Track& track)
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

    id3Tags->removeFrames("FMPS_Rating");

    const auto rating = QString::number(track.rating());
    auto ratingFrame  = std::make_unique<TagLib::ID3v2::TextIdentificationFrame>("FMPS_Rating", TagLib::String::UTF8);
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

void writeApeTags(TagLib::APE::Tag* apeTags, const Fooyin::Track& track)
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

    if(track.rating() > 0) {
        apeTags->setItem("FMPS_Rating",
                         TagLib::APE::Item{"FMPS_Rating", convertString(QString::number(track.rating()))});
    }
    else {
        apeTags->removeItem("FMPS_Rating");
    }
}

QString stripMp4FreeFormName(const TagLib::String& name)
{
    QString freeFormName = convertString(name);

    if(freeFormName.startsWith(QStringLiteral("----"))) {
        qsizetype nameStart = freeFormName.lastIndexOf(QStringLiteral(":"));
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

    if(items.contains(Fooyin::Mp4::TrackNumber)) {
        const TagLib::MP4::Item::IntPair& trackNumbers = items[Fooyin::Mp4::TrackNumber].toIntPair();
        if(trackNumbers.first > 0) {
            track.setTrackNumber(trackNumbers.first);
        }
        if(trackNumbers.second > 0) {
            track.setTrackTotal(trackNumbers.second);
        }
    }

    if(items.contains(Fooyin::Mp4::DiscNumber)) {
        const TagLib::MP4::Item::IntPair& discNumbers = items[Fooyin::Mp4::DiscNumber].toIntPair();
        if(discNumbers.first > 0) {
            track.setDiscNumber(discNumbers.first);
        }
        if(discNumbers.second > 0) {
            track.setDiscTotal(discNumbers.second);
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

    if(!skipExtra) {
        static const std::set<TagLib::String> baseMp4Tags
            = {Fooyin::Mp4::Title,       Fooyin::Mp4::Artist,    Fooyin::Mp4::Album,     Fooyin::Mp4::AlbumArtist,
               Fooyin::Mp4::Genre,       Fooyin::Mp4::Composer,  Fooyin::Mp4::Performer, Fooyin::Mp4::PerformerAlt,
               Fooyin::Mp4::Comment,     Fooyin::Mp4::Date,      Fooyin::Mp4::Rating,    Fooyin::Mp4::RatingAlt,
               Fooyin::Mp4::TrackNumber, Fooyin::Mp4::DiscNumber};

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

    if(name.startsWith(QStringLiteral("----")) || (name.length() == 4 && name[0] == QStringLiteral("\251"))) {
        return {};
    }

    TagLib::String tagKey = convertString(name);

    if(name[0] == QStringLiteral(":")) {
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

void writeMp4Tags(TagLib::MP4::Tag* mp4Tags, const Fooyin::Track& track)
{
    const int trackNumber = track.trackNumber();
    const int trackTotal  = track.trackTotal();

    if(trackNumber < 1 && trackTotal < 1) {
        mp4Tags->removeItem("trkn");
    }
    else {
        mp4Tags->setItem("trkn", {trackNumber, trackTotal});
    }

    const int discNumber = track.discNumber();
    const int discTotal  = track.discTotal();

    if(discNumber < 1 && discTotal < 1) {
        mp4Tags->removeItem("disk");
    }
    else {
        mp4Tags->setItem("disk", {discNumber, discTotal});
    }

    mp4Tags->setItem(Fooyin::Mp4::PerformerAlt, TagLib::StringList{convertString(track.performer())});
    mp4Tags->setItem(Fooyin::Mp4::RatingAlt, TagLib::StringList(convertString(QString::number(track.rating()))));

    static const std::set<QString> baseMp4Tags
        = {QString::fromLatin1(Fooyin::Tag::Title),       QString::fromLatin1(Fooyin::Tag::Artist),
           QString::fromLatin1(Fooyin::Tag::Album),       QString::fromLatin1(Fooyin::Tag::AlbumArtist),
           QString::fromLatin1(Fooyin::Tag::Genre),       QString::fromLatin1(Fooyin::Tag::Composer),
           QString::fromLatin1(Fooyin::Tag::Performer),   QString::fromLatin1(Fooyin::Tag::Comment),
           QString::fromLatin1(Fooyin::Tag::Date),        QString::fromLatin1(Fooyin::Tag::Rating),
           QString::fromLatin1(Fooyin::Tag::TrackNumber), QString::fromLatin1(Fooyin::Tag::DiscNumber)};

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

    if(fields.contains(Fooyin::Tag::TrackNumber)) {
        const TagLib::StringList& trackNumber = fields[Fooyin::Tag::TrackNumber];
        if(!trackNumber.isEmpty() && !trackNumber.front().isEmpty()) {
            track.setTrackNumber(trackNumber.front().toInt());
        }
    }

    if(fields.contains(Fooyin::Tag::TrackTotal)) {
        const TagLib::StringList& trackTotal = fields[Fooyin::Tag::TrackTotal];
        if(!trackTotal.isEmpty() && !trackTotal.front().isEmpty()) {
            track.setTrackTotal(trackTotal.front().toInt());
        }
    }

    if(fields.contains(Fooyin::Tag::DiscNumber)) {
        const TagLib::StringList& discNumber = fields[Fooyin::Tag::DiscNumber];
        if(!discNumber.isEmpty() && !discNumber.front().isEmpty()) {
            track.setDiscNumber(discNumber.front().toInt());
        }
    }

    if(fields.contains(Fooyin::Tag::DiscTotal)) {
        const TagLib::StringList& discTotal = fields[Fooyin::Tag::DiscTotal];
        if(!discTotal.isEmpty() && !discTotal.front().isEmpty()) {
            track.setDiscTotal(discTotal.front().toInt());
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

void writeXiphComment(TagLib::Ogg::XiphComment* xiphTags, const Fooyin::Track& track)
{
    if(track.trackNumber() < 0) {
        xiphTags->removeFields(Fooyin::Tag::TrackNumber);
    }
    else {
        xiphTags->addField(Fooyin::Tag::TrackNumber, TagLib::String::number(track.trackNumber()), true);
    }

    if(track.trackTotal() < 0) {
        xiphTags->removeFields(Fooyin::Tag::TrackTotal);
    }
    else {
        xiphTags->addField(Fooyin::Tag::TrackTotal, TagLib::String::number(track.trackTotal()), true);
    }

    if(track.discNumber() < 0) {
        xiphTags->removeFields(Fooyin::Tag::DiscNumber);
    }
    else {
        xiphTags->addField(Fooyin::Tag::DiscNumber, TagLib::String::number(track.discNumber()), true);
    }

    if(track.discTotal() < 0) {
        xiphTags->removeFields(Fooyin::Tag::DiscTotal);
    }
    else {
        xiphTags->addField(Fooyin::Tag::DiscTotal, TagLib::String::number(track.discTotal()), true);
    }

    if(track.rating() <= 0) {
        xiphTags->removeFields("FMPS_RATING");
    }
    else {
        xiphTags->addField("FMPS_RATING", convertString(QString::number(track.rating())), true);
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
                track.setTrackNumber(num.toString().toInt());
            }
        }
    }

    if(map.contains("WM/PartOfSet")) {
        const TagLib::ASF::AttributeList& discNumber = map["WM/PartOfSet"];
        if(!discNumber.isEmpty()) {
            const TagLib::ASF::Attribute& num = discNumber.front();
            if(num.type() == TagLib::ASF::Attribute::UnicodeType) {
                track.setDiscNumber(num.toString().toInt());
            }
        }
    }

    if(map.contains("FMPS/Rating") && track.rating() <= 0) {
        const TagLib::ASF::AttributeList& rating = map["FMPS/Rating"];
        if(!rating.isEmpty()) {
            const float rate = convertString(map["FMPS/Rating"].front().toString()).toFloat();
            if(rate > 0 && rate <= 1.0) {
                track.setRating(rate);
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

void writeAsfTags(TagLib::ASF::Tag* asfTags, const Fooyin::Track& track)
{
    asfTags->setAttribute("WM/TrackNumber", TagLib::String::number(track.trackNumber()));
    asfTags->setAttribute("WM/PartOfSet", TagLib::String::number(track.discNumber()));
    asfTags->addAttribute("FMPS/Rating", convertString(QString::number(track.rating())));
}
} // namespace

namespace Fooyin {
QStringList TagLibParser::supportedExtensions() const
{
    static const QStringList extensions{QStringLiteral("mp3"),  QStringLiteral("ogg"),  QStringLiteral("opus"),
                                        QStringLiteral("oga"),  QStringLiteral("m4a"),  QStringLiteral("wav"),
                                        QStringLiteral("wv"),   QStringLiteral("flac"), QStringLiteral("wma"),
                                        QStringLiteral("mpc"),  QStringLiteral("aiff"), QStringLiteral("ape"),
                                        QStringLiteral("webm"), QStringLiteral("mp4")};
    return extensions;
}

bool TagLibParser::readMetaData(Track& track) const
{
    const auto filepath = track.filepath();
    const QFileInfo fileInfo{filepath};

    if(fileInfo.size() <= 0) {
        return false;
    }

    track.setFileSize(fileInfo.size());

    track.setAddedTime(QDateTime::currentMSecsSinceEpoch());
    const QDateTime modifiedTime = fileInfo.lastModified();
    track.setModifiedTime(modifiedTime.isValid() ? modifiedTime.toMSecsSinceEpoch() : 0);

    TagLib::FileStream stream(filepath.toUtf8().constData(), true);
    if(!stream.isOpen()) {
        qWarning() << "Unable to open file readonly: " << filepath;
        return false;
    }

    const QMimeDatabase mimeDb;
    QString mimeType = mimeDb.mimeTypeForFile(filepath).name();
    const auto style = TagLib::AudioProperties::Average;

    const auto readProperties = [&track](const TagLib::File& file, bool skipExtra = false) {
        readAudioProperties(file, track);
        readGeneralProperties(file.properties(), track, skipExtra);
    };

    if(mimeType == QStringLiteral("audio/ogg") || mimeType == QStringLiteral("audio/x-vorbis+ogg")) {
        // Workaround for opus files with ogg suffix returning incorrect type
        mimeType = mimeDb.mimeTypeForFile(filepath, QMimeDatabase::MatchContent).name();
    }
    if(mimeType == QStringLiteral("audio/mpeg") || mimeType == QStringLiteral("audio/mpeg3")
       || mimeType == QStringLiteral("audio/x-mpeg")) {
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
    else if(mimeType == QStringLiteral("audio/x-aiff") || mimeType == QStringLiteral("audio/x-aifc")) {
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
    else if(mimeType == QStringLiteral("audio/vnd.wave") || mimeType == QStringLiteral("audio/wav")
            || mimeType == QStringLiteral("audio/x-wav")) {
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
    else if(mimeType == QStringLiteral("audio/x-musepack")) {
        TagLib::MPC::File file(&stream, true, style);
        if(file.isValid()) {
            readProperties(file);
            if(file.APETag()) {
                readApeTags(file.APETag(), track);
            }
        }
    }
    else if(mimeType == QStringLiteral("audio/x-ape")) {
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
    else if(mimeType == QStringLiteral("audio/x-wavpack")) {
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
    else if(mimeType == QStringLiteral("audio/mp4") || mimeType == QStringLiteral("audio/vnd.audible.aax")) {
        const TagLib::MP4::File file(&stream, true, style);
        if(file.isValid()) {
            readProperties(file, true);
            if(const int bps = file.audioProperties()->bitsPerSample(); bps > 0) {
                track.setBitDepth(bps);
            }
            if(file.hasMP4Tag()) {
                readMp4Tags(file.tag(), track);
            }
        }
    }
    else if(mimeType == QStringLiteral("audio/flac")) {
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
    else if(mimeType == QStringLiteral("audio/ogg") || mimeType == QStringLiteral("audio/x-vorbis+ogg")) {
        const TagLib::Ogg::Vorbis::File file(&stream, true, style);
        if(file.isValid()) {
            readProperties(file);
            if(file.tag()) {
                readXiphComment(file.tag(), track);
            }
        }
    }
    else if(mimeType == QStringLiteral("audio/opus") || mimeType == QStringLiteral("audio/x-opus+ogg")) {
        const TagLib::Ogg::Opus::File file(&stream, true, style);
        if(file.isValid()) {
            readProperties(file);
            if(file.tag()) {
                readXiphComment(file.tag(), track);
            }
        }
    }
    else if(mimeType == QStringLiteral("audio/x-ms-wma")) {
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
        qDebug() << "Unsupported mime type: " << mimeType;
    }

    track.setType(typeForMime(mimeType));
    track.generateHash();

    return true;
}

QByteArray TagLibParser::readCover(const Track& track, Track::Cover cover) const
{
    const auto filepath = track.filepath();
    const QFileInfo fileInfo{filepath};

    if(fileInfo.size() <= 0) {
        return {};
    }

    TagLib::FileStream stream(filepath.toUtf8().constData(), true);
    if(!stream.isOpen()) {
        qWarning() << "Unable to open file readonly: " << filepath;
        return {};
    }

    const QMimeDatabase mimeDb;
    QString mimeType = mimeDb.mimeTypeForFile(filepath).name();
    const auto style = TagLib::AudioProperties::Average;

    if(mimeType == QStringLiteral("audio/ogg") || mimeType == QStringLiteral("audio/x-vorbis+ogg")) {
        // Workaround for opus files with ogg suffix returning incorrect type
        mimeType = mimeDb.mimeTypeForFile(filepath, QMimeDatabase::MatchContent).name();
    }
    if(mimeType == QStringLiteral("audio/mpeg") || mimeType == QStringLiteral("audio/mpeg3")
       || mimeType == QStringLiteral("audio/x-mpeg")) {
#if(TAGLIB_MAJOR_VERSION >= 2)
        TagLib::MPEG::File file(&stream, true, style, TagLib::ID3v2::FrameFactory::instance());
#else
        TagLib::MPEG::File file(&stream, TagLib::ID3v2::FrameFactory::instance(), true, style);
#endif
        if(file.isValid() && file.hasID3v2Tag()) {
            return readId3Cover(file.ID3v2Tag(), cover);
        }
    }
    else if(mimeType == QStringLiteral("audio/x-aiff") || mimeType == QStringLiteral("audio/x-aifc")) {
        const TagLib::RIFF::AIFF::File file(&stream, true);
        if(file.isValid() && file.hasID3v2Tag()) {
            return readId3Cover(file.tag(), cover);
        }
    }
    else if(mimeType == QStringLiteral("audio/vnd.wave") || mimeType == QStringLiteral("audio/wav")
            || mimeType == QStringLiteral("audio/x-wav")) {
        const TagLib::RIFF::WAV::File file(&stream, true);
        if(file.isValid() && file.hasID3v2Tag()) {
            return readId3Cover(file.ID3v2Tag(), cover);
        }
    }
    else if(mimeType == QStringLiteral("audio/x-musepack")) {
        TagLib::MPC::File file(&stream, true);
        if(file.isValid() && file.APETag()) {
            return readApeCover(file.APETag(), cover);
        }
    }
    else if(mimeType == QStringLiteral("audio/x-ape")) {
        TagLib::APE::File file(&stream, true);
        if(file.isValid() && file.APETag()) {
            return readApeCover(file.APETag(), cover);
        }
    }
    else if(mimeType == QStringLiteral("audio/x-wavpack")) {
        TagLib::WavPack::File file(&stream, true);
        if(file.isValid() && file.APETag()) {
            return readApeCover(file.APETag(), cover);
        }
    }
    else if(mimeType == QStringLiteral("audio/mp4") || mimeType == QStringLiteral("audio/vnd.audible.aax")) {
        const TagLib::MP4::File file(&stream, true);
        if(file.isValid() && file.tag()) {
            return readMp4Cover(file.tag(), cover);
        }
    }
    else if(mimeType == QStringLiteral("audio/flac")) {
#if(TAGLIB_MAJOR_VERSION >= 2)
        TagLib::FLAC::File file(&stream, true, style, TagLib::ID3v2::FrameFactory::instance());
#else
        TagLib::FLAC::File file(&stream, TagLib::ID3v2::FrameFactory::instance(), true, style);
#endif
        if(file.isValid()) {
            return readFlacCover(file.pictureList(), cover);
        }
    }
    else if(mimeType == QStringLiteral("audio/ogg") || mimeType == QStringLiteral("audio/x-vorbis+ogg")) {
        const TagLib::Ogg::Vorbis::File file(&stream, true);
        if(file.isValid() && file.tag()) {
            return readFlacCover(file.tag()->pictureList(), cover);
        }
    }
    else if(mimeType == QStringLiteral("audio/opus") || mimeType == QStringLiteral("audio/x-opus+ogg")) {
        const TagLib::Ogg::Opus::File file(&stream, true);
        if(file.isValid() && file.tag()) {
            return readFlacCover(file.tag()->pictureList(), cover);
        }
    }
    else if(mimeType == QStringLiteral("audio/x-ms-wma")) {
        const TagLib::ASF::File file(&stream, true);
        if(file.isValid() && file.tag()) {
            return readAsfCover(file.tag(), cover);
        }
    }

    return {};
}

bool TagLibParser::writeMetaData(const Track& track) const
{
    const QString filepath = track.filepath();

    TagLib::FileStream stream(filepath.toUtf8().constData(), false);

    if(!stream.isOpen() || stream.readOnly()) {
        return false;
    }

    const auto writeProperties = [&track](TagLib::File& file, bool skipExtra = false) {
        auto savedProperties = file.properties();
        writeGenericProperties(savedProperties, track, skipExtra);
        file.setProperties(savedProperties);
    };

    const QMimeDatabase mimeDb;
    QString mimeType = mimeDb.mimeTypeForFile(filepath).name();
    const auto style = TagLib::AudioProperties::Average;

    if(mimeType == QStringLiteral("audio/ogg") || mimeType == QStringLiteral("audio/x-vorbis+ogg")) {
        // Workaround for opus files with ogg suffix returning incorrect type
        mimeType = mimeDb.mimeTypeForFile(filepath, QMimeDatabase::MatchContent).name();
    }
    if(mimeType == QStringLiteral("audio/mpeg") || mimeType == QStringLiteral("audio/mpeg3")
       || mimeType == QStringLiteral("audio/x-mpeg")) {
#if(TAGLIB_MAJOR_VERSION >= 2)
        TagLib::MPEG::File file(&stream, true, style, TagLib::ID3v2::FrameFactory::instance());
#else
        TagLib::MPEG::File file(&stream, TagLib::ID3v2::FrameFactory::instance(), false, style);
#endif
        if(file.isValid()) {
            writeProperties(file);
            if(file.hasID3v2Tag()) {
                writeID3v2Tags(file.ID3v2Tag(), track);
            }
            file.save();
        }
    }
    else if(mimeType == QStringLiteral("audio/x-aiff") || mimeType == QStringLiteral("audio/x-aifc")) {
        TagLib::RIFF::AIFF::File file(&stream, false);
        if(file.isValid()) {
            writeProperties(file);
            if(file.hasID3v2Tag()) {
                writeID3v2Tags(file.tag(), track);
            }
            file.save();
        }
    }
    else if(mimeType == QStringLiteral("audio/vnd.wave") || mimeType == QStringLiteral("audio/wav")
            || mimeType == QStringLiteral("audio/x-wav")) {
        TagLib::RIFF::WAV::File file(&stream, false);
        if(file.isValid()) {
            writeProperties(file);
            if(file.hasID3v2Tag()) {
                writeID3v2Tags(file.ID3v2Tag(), track);
            }
            file.save();
        }
    }
    else if(mimeType == QStringLiteral("audio/x-musepack")) {
        TagLib::MPC::File file(&stream, false);
        if(file.isValid()) {
            writeProperties(file);
            if(file.hasAPETag()) {
                writeApeTags(file.APETag(), track);
            }
            file.save();
        }
    }
    else if(mimeType == QStringLiteral("audio/x-ape")) {
        TagLib::APE::File file(&stream, false);
        if(file.isValid()) {
            writeProperties(file);
            if(file.hasAPETag()) {
                writeApeTags(file.APETag(), track);
            }
            file.save();
        }
    }
    else if(mimeType == QStringLiteral("audio/x-wavpack")) {
        TagLib::WavPack::File file(&stream, false);
        if(file.isValid()) {
            writeProperties(file);
            if(file.hasAPETag()) {
                writeApeTags(file.APETag(), track);
            }
            file.save();
        }
    }
    else if(mimeType == QStringLiteral("audio/mp4")) {
        TagLib::MP4::File file(&stream, false);
        if(file.isValid()) {
            writeProperties(file, true);
            if(file.hasMP4Tag()) {
                writeMp4Tags(file.tag(), track);
            }
            file.save();
        }
    }
    else if(mimeType == QStringLiteral("audio/flac")) {
#if(TAGLIB_MAJOR_VERSION >= 2)
        TagLib::FLAC::File file(&stream, true, style, TagLib::ID3v2::FrameFactory::instance());
#else
        TagLib::FLAC::File file(&stream, TagLib::ID3v2::FrameFactory::instance(), false, style);
#endif
        if(file.isValid()) {
            writeProperties(file);
            if(file.hasXiphComment()) {
                writeXiphComment(file.xiphComment(), track);
            }
            file.save();
        }
    }
    else if(mimeType == QStringLiteral("audio/ogg") || mimeType == QStringLiteral("audio/x-vorbis+ogg")) {
        TagLib::Ogg::Vorbis::File file(&stream, false);
        if(file.isValid()) {
            writeProperties(file);
            if(file.tag()) {
                writeXiphComment(file.tag(), track);
            }
            file.save();
        }
    }
    else if(mimeType == QStringLiteral("audio/opus") || mimeType == QStringLiteral("audio/x-opus+ogg")) {
        TagLib::Ogg::Opus::File file(&stream, false);
        if(file.isValid()) {
            writeProperties(file);
            writeXiphComment(file.tag(), track);
            file.save();
        }
    }
    else if(mimeType == QStringLiteral("audio/x-ms-wma")) {
        TagLib::ASF::File file(&stream, false);
        if(file.isValid()) {
            writeProperties(file);
            if(file.tag()) {
                writeAsfTags(file.tag(), track);
            }
            file.save();
        }
    }

    return true;
}
} // namespace Fooyin
