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

#include "tagreader.h"

#include "tagdefs.h"

#include <core/constants.h>
#include <core/track.h>

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
#include <taglib/tag.h>
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
constexpr std::array supportedMp4Tags{
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

QString findMp4Tag(const TagLib::String& tag)
{
    for(const auto& [key, value] : supportedMp4Tags) {
        if(tag == key) {
            return QString::fromUtf8(value);
        }
    }
    return {};
}

QString convertString(const TagLib::String& str)
{
    return QString::fromStdString(str.to8Bit(true));
}

QStringList convertStringList(const TagLib::StringList& strList)
{
    QStringList list;
    list.reserve(strList.size());

    std::ranges::transform(strList, std::back_inserter(list), &convertString);

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

TagLib::AudioProperties::ReadStyle readStyle(Fooyin::Tagging::Quality quality)
{
    switch(quality) {
        case(Fooyin::Tagging::Quality::Fast):
            return TagLib::AudioProperties::Fast;
        case(Fooyin::Tagging::Quality::Average):
            return TagLib::AudioProperties::Average;
        case(Fooyin::Tagging::Quality::Accurate):
            return TagLib::AudioProperties::Accurate;
        default:
            return TagLib::AudioProperties::Average;
    }
}

void readAudioProperties(const TagLib::File& file, Fooyin::Track& track)
{
    if(TagLib::AudioProperties* props = file.audioProperties()) {
        if(props->lengthInMilliseconds()) {
            track.setDuration(props->lengthInMilliseconds());
        }
        if(props->bitrate()) {
            track.setBitrate(props->bitrate());
        }
        if(props->sampleRate()) {
            track.setSampleRate(props->sampleRate());
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
    if(props.contains(Fooyin::Tag::Rating)) {
        // TODO
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
}

QByteArray readId3Cover(const TagLib::ID3v2::Tag* id3Tags)
{
    if(id3Tags->isEmpty()) {
        return {};
    }

    const TagLib::ID3v2::FrameList& frames = id3Tags->frameListMap()["APIC"];

    using PictureFrame = TagLib::ID3v2::AttachedPictureFrame;

    for(const auto& frame : std::as_const(frames)) {
        const auto* coverFrame        = static_cast<PictureFrame*>(frame);
        const PictureFrame::Type type = coverFrame->type();

        if(type == PictureFrame::FrontCover || type == PictureFrame::Other) {
            const auto picture = coverFrame->picture();
            return {picture.data(), picture.size()};
        }
    }
    return {};
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
}

QByteArray readApeCover(const TagLib::APE::Tag* apeTags)
{
    if(apeTags->isEmpty()) {
        return {};
    }

    const TagLib::APE::ItemListMap& items = apeTags->itemListMap();
    auto itemIt                           = items.find("COVER ART (FRONT)");

    if(itemIt != items.end()) {
        const auto& picture = itemIt->second.binaryData();
        int position        = picture.find('\0');
        if(position >= 0) {
            position += 1;
            return {picture.data() + position, picture.size() - position};
        }
    }
    return {};
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

    if(items.contains(Fooyin::Mp4::Rating)) {
        const auto rating = items[Fooyin::Mp4::Rating].toStringList();
        if(rating.size() > 0) { }
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

    if(!skipExtra) {
        static const std::set<TagLib::String> baseMp4Tags
            = {Fooyin::Mp4::Title,     Fooyin::Mp4::Artist,   Fooyin::Mp4::Album,     Fooyin::Mp4::AlbumArtist,
               Fooyin::Mp4::Genre,     Fooyin::Mp4::Composer, Fooyin::Mp4::Performer, Fooyin::Mp4::PerformerAlt,
               Fooyin::Mp4::Comment,   Fooyin::Mp4::Date,     Fooyin::Mp4::Rating,    Fooyin::Mp4::TrackNumber,
               Fooyin::Mp4::DiscNumber};

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

QByteArray readMp4Cover(const TagLib::MP4::Tag* mp4Tags)
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
}

QByteArray readFlacCover(const TagLib::List<TagLib::FLAC::Picture*>& pictures)
{
    if(pictures.isEmpty()) {
        return {};
    }

    using FlacPicture = TagLib::FLAC::Picture;

    for(const auto& picture : std::as_const(pictures)) {
        const auto type = picture->type();
        if(type == FlacPicture::FrontCover || type == FlacPicture::Other) {
            return {picture->data().data(), picture->data().size()};
        }
    }
    return {};
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
}

QByteArray readAsfCover(const TagLib::ASF::Tag* asfTags)
{
    if(asfTags->isEmpty()) {
        return {};
    }

    TagLib::ASF::AttributeList pictures = asfTags->attribute("WM/Picture");

    using Picture = TagLib::ASF::Picture;

    for(const auto& attribute : std::as_const(pictures)) {
        const Picture picture = attribute.toPicture();
        const auto imageType  = picture.type();
        if(imageType == Picture::FrontCover) {
            const auto& pictureData = picture.picture();
            return {pictureData.data(), pictureData.size()};
        }
    }
    return {};
}
} // namespace

namespace Fooyin::Tagging {
// TODO: Implement a file/mime type resolver
bool readMetaData(Track& track, Quality quality)
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
    const auto style = readStyle(quality);

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
            if(file.APETag()) {
                readApeTags(file.APETag(), track);
            }
        }
    }
    else if(mimeType == QStringLiteral("audio/x-wavpack")) {
        TagLib::WavPack::File file(&stream, true, style);
        if(file.isValid()) {
            readProperties(file);
            if(file.APETag()) {
                readApeTags(file.APETag(), track);
            }
        }
    }
    else if(mimeType == QStringLiteral("audio/mp4") || mimeType == QStringLiteral("audio/vnd.audible.aax")) {
        const TagLib::MP4::File file(&stream, true, style);
        if(file.isValid()) {
            readProperties(file, true);
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

QByteArray readCover(const Track& track)
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
            return readId3Cover(file.ID3v2Tag());
        }
    }
    else if(mimeType == QStringLiteral("audio/x-aiff") || mimeType == QStringLiteral("audio/x-aifc")) {
        const TagLib::RIFF::AIFF::File file(&stream, true);
        if(file.isValid() && file.hasID3v2Tag()) {
            return readId3Cover(file.tag());
        }
    }
    else if(mimeType == QStringLiteral("audio/vnd.wave") || mimeType == QStringLiteral("audio/wav")
            || mimeType == QStringLiteral("audio/x-wav")) {
        const TagLib::RIFF::WAV::File file(&stream, true);
        if(file.isValid() && file.hasID3v2Tag()) {
            return readId3Cover(file.ID3v2Tag());
        }
    }
    else if(mimeType == QStringLiteral("audio/x-musepack")) {
        TagLib::MPC::File file(&stream, true);
        if(file.isValid() && file.APETag()) {
            return readApeCover(file.APETag());
        }
    }
    else if(mimeType == QStringLiteral("audio/x-ape")) {
        TagLib::APE::File file(&stream, true);
        if(file.isValid() && file.APETag()) {
            return readApeCover(file.APETag());
        }
    }
    else if(mimeType == QStringLiteral("audio/x-wavpack")) {
        TagLib::WavPack::File file(&stream, true);
        if(file.isValid() && file.APETag()) {
            return readApeCover(file.APETag());
        }
    }
    else if(mimeType == QStringLiteral("audio/mp4") || mimeType == QStringLiteral("audio/vnd.audible.aax")) {
        const TagLib::MP4::File file(&stream, true);
        if(file.isValid() && file.tag()) {
            return readMp4Cover(file.tag());
        }
    }
    else if(mimeType == QStringLiteral("audio/flac")) {
#if(TAGLIB_MAJOR_VERSION >= 2)
        TagLib::FLAC::File file(&stream, true, style, TagLib::ID3v2::FrameFactory::instance());
#else
        TagLib::FLAC::File file(&stream, TagLib::ID3v2::FrameFactory::instance(), true, style);
#endif
        if(file.isValid()) {
            return readFlacCover(file.pictureList());
        }
    }
    else if(mimeType == QStringLiteral("audio/ogg") || mimeType == QStringLiteral("audio/x-vorbis+ogg")) {
        const TagLib::Ogg::Vorbis::File file(&stream, true);
        if(file.isValid() && file.tag()) {
            return readFlacCover(file.tag()->pictureList());
        }
    }
    else if(mimeType == QStringLiteral("audio/opus") || mimeType == QStringLiteral("audio/x-opus+ogg")) {
        const TagLib::Ogg::Opus::File file(&stream, true);
        if(file.isValid() && file.tag()) {
            return readFlacCover(file.tag()->pictureList());
        }
    }
    else if(mimeType == QStringLiteral("audio/x-ms-wma")) {
        const TagLib::ASF::File file(&stream, true);
        if(file.isValid() && file.tag()) {
            return readAsfCover(file.tag());
        }
    }

    return {};
}
} // namespace Fooyin::Tagging
