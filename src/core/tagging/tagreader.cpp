/*
 * Fooyin
 * Copyright 2022-2023, Luke Taylor <LukeT1@proton.me>
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

#include <core/tagging/tagreader.h>

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

using namespace Qt::Literals::StringLiterals;

namespace {
using namespace Fy::Core::Tagging;

constexpr std::array supportedMp4Tags{
    std::pair(Mp4::Title, Tag::Title),
    std::pair(Mp4::Artist, Tag::Artist),
    std::pair(Mp4::Album, Tag::Album),
    std::pair(Mp4::AlbumArtist, Tag::AlbumArtist),
    std::pair(Mp4::Genre, Tag::Genre),
    std::pair(Mp4::Composer, Tag::Composer),
    std::pair(Mp4::Performer, Tag::Performer),
    std::pair(Mp4::Comment, Tag::Comment),
    std::pair(Mp4::Lyrics, Tag::Lyrics),
    std::pair(Mp4::Date, Tag::Date),
    std::pair(Mp4::Rating, Tag::Rating),
    std::pair(Mp4::TrackNumber, Tag::TrackNumber),
    std::pair(Mp4::DiscNumber, Tag::DiscNumber),
    std::pair("cpil", "COMPILATION"),
    std::pair("tmpo", "BPM"),
    std::pair("cprt", "COPYRIGHT"),
    std::pair("\251too", "ENCODEDBY"),
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
    std::pair("----:com.apple.iTunes:originaldate", "ORIGINALDATE"),
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
            return value;
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

Fy::Core::Track::Type typeForMime(const QString& mimeType)
{
    if(mimeType == "audio/mpeg"_L1 || mimeType == "audio/mpeg3"_L1 || mimeType == "audio/x-mpeg"_L1) {
        return Fy::Core::Track::Type::MPEG;
    }
    if(mimeType == "audio/x-aiff"_L1 || mimeType == "audio/x-aifc"_L1) {
        return Fy::Core::Track::Type::AIFF;
    }
    if(mimeType == "audio/vnd.wave"_L1 || mimeType == "audio/wav"_L1 || mimeType == "audio/x-wav"_L1) {
        return Fy::Core::Track::Type::WAV;
    }
    if(mimeType == "audio/x-musepack"_L1) {
        return Fy::Core::Track::Type::MPC;
    }
    if(mimeType == "audio/x-ape"_L1) {
        return Fy::Core::Track::Type::APE;
    }
    if(mimeType == "audio/x-wavpack"_L1) {
        return Fy::Core::Track::Type::WavPack;
    }
    if(mimeType == "audio/mp4"_L1 || mimeType == "audio/vnd.audible.aax"_L1) {
        return Fy::Core::Track::Type::MP4;
    }
    if(mimeType == "audio/flac"_L1) {
        return Fy::Core::Track::Type::FLAC;
    }
    if(mimeType == "audio/ogg"_L1 || mimeType == "audio/x-vorbis+ogg"_L1) {
        return Fy::Core::Track::Type::OggVorbis;
    }
    if(mimeType == "audio/opus"_L1 || mimeType == "audio/x-opus+ogg"_L1) {
        return Fy::Core::Track::Type::OggOpus;
    }
    if(mimeType == "audio/x-ms-wma"_L1) {
        return Fy::Core::Track::Type::ASF;
    }

    return Fy::Core::Track::Type::Unknown;
}

TagLib::AudioProperties::ReadStyle readStyle(Fy::Core::Tagging::TagReader::Quality quality)
{
    switch(quality) {
        case(Fy::Core::Tagging::TagReader::Quality::Fast):
            return TagLib::AudioProperties::Fast;
        case(Fy::Core::Tagging::TagReader::Quality::Average):
            return TagLib::AudioProperties::Average;
        case(Fy::Core::Tagging::TagReader::Quality::Accurate):
            return TagLib::AudioProperties::Accurate;
        default:
            return TagLib::AudioProperties::Average;
    }
}

void readAudioProperties(const TagLib::File& file, Fy::Core::Track& track)
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

void readGeneralProperties(const TagLib::PropertyMap& props, Fy::Core::Track& track, bool skipExtra = false)
{
    if(props.isEmpty()) {
        return;
    }

    if(props.contains(Tag::Title)) {
        track.setTitle(convertString(props[Tag::Title].toString()));
    }
    if(props.contains(Tag::Artist)) {
        track.setArtists(convertStringList(props[Tag::Artist]));
    }
    if(props.contains(Tag::Album)) {
        track.setAlbum(convertString(props[Tag::Album].toString()));
    }
    if(props.contains(Tag::AlbumArtist)) {
        // TODO: Support multiple album artists
        track.setAlbumArtist(convertString(props[Tag::AlbumArtist].toString()));
    }
    if(props.contains(Tag::Genre)) {
        track.setGenres(convertStringList(props[Tag::Genre]));
    }
    if(props.contains(Tag::Composer)) {
        track.setComposer(convertString(props[Tag::Composer].toString()));
    }
    if(props.contains(Tag::Performer)) {
        track.setPerformer(convertString(props[Tag::Performer].toString()));
    }
    if(props.contains(Tag::Comment)) {
        track.setComment(convertString(props[Tag::Comment].toString()));
    }
    if(props.contains(Tag::Lyrics)) {
        track.setLyrics(convertString(props[Tag::Lyrics].toString()));
    }
    if(props.contains(Tag::Date)) {
        track.setDate(convertString(props[Tag::Date].toString()));
    }
    if(props.contains(Tag::Rating)) {
        // TODO
    }

    if(!skipExtra) {
        static const std::set<TagLib::String> baseTags
            = {Tag::Title,      Tag::Artist,     Tag::Album,     Tag::AlbumArtist, Tag::TrackNumber,
               Tag::TrackTotal, Tag::DiscNumber, Tag::DiscTotal, Tag::Genre,       Tag::Composer,
               Tag::Performer,  Tag::Comment,    Tag::Lyrics,    Tag::Date,        Tag::Rating};

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

void readTrackTotalPair(const QString& trackNumbers, Fy::Core::Track& track)
{
    const qsizetype splitIdx = trackNumbers.indexOf("/"_L1);
    if(splitIdx >= 0) {
        track.setTrackNumber(trackNumbers.first(splitIdx).toInt());
        track.setTrackTotal(trackNumbers.sliced(splitIdx + 1).toInt());
    }
    else if(trackNumbers.size() > 0) {
        track.setTrackNumber(trackNumbers.toInt());
    }
}
void readDiscTotalPair(const QString& discNumbers, Fy::Core::Track& track)
{
    const qsizetype splitIdx = discNumbers.indexOf("/"_L1);
    if(splitIdx >= 0) {
        track.setDiscNumber(discNumbers.first(splitIdx).toInt());
        track.setDiscTotal(discNumbers.sliced(splitIdx + 1).toInt());
    }
    else if(discNumbers.size() > 0) {
        track.setDiscNumber(discNumbers.toInt());
    }
}

void readId3Tags(const TagLib::ID3v2::Tag* id3Tags, Fy::Core::Track& track)
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

void readApeTags(const TagLib::APE::Tag* apeTags, Fy::Core::Track& track)
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

void readMp4Tags(const TagLib::MP4::Tag* mp4Tags, Fy::Core::Track& track, bool skipExtra = false)
{
    if(mp4Tags->isEmpty()) {
        return;
    }

    const auto& items = mp4Tags->itemMap();

    if(items.contains(Mp4::PerformerAlt)) {
        const auto performer = items[Mp4::PerformerAlt].toStringList();
        if(performer.size() > 0) {
            track.setPerformer(convertString(performer.toString()));
        }
    }

    if(items.contains(Mp4::Rating)) {
        const auto rating = items[Mp4::Rating].toStringList();
        if(rating.size() > 0) { }
    }

    if(items.contains(Mp4::TrackNumber)) {
        const TagLib::MP4::Item::IntPair& trackNumbers = items[Mp4::TrackNumber].toIntPair();
        if(trackNumbers.first > 0) {
            track.setTrackNumber(trackNumbers.first);
        }
        if(trackNumbers.second > 0) {
            track.setTrackTotal(trackNumbers.second);
        }
    }

    if(items.contains(Mp4::DiscNumber)) {
        const TagLib::MP4::Item::IntPair& discNumbers = items[Mp4::DiscNumber].toIntPair();
        if(discNumbers.first > 0) {
            track.setDiscNumber(discNumbers.first);
        }
        if(discNumbers.second > 0) {
            track.setDiscTotal(discNumbers.second);
        }
    }

    if(!skipExtra) {
        static const std::set<TagLib::String> baseMp4Tags
            = {Mp4::Title,    Mp4::Artist,    Mp4::Album,        Mp4::AlbumArtist, Mp4::Genre,
               Mp4::Composer, Mp4::Performer, Mp4::PerformerAlt, Mp4::Comment,     Mp4::Lyrics,
               Mp4::Date,     Mp4::Rating,    Mp4::TrackNumber,  Mp4::DiscNumber};

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
    const TagLib::MP4::Item coverArtItem = mp4Tags->item(Mp4::Cover);
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

void readXiphComment(const TagLib::Ogg::XiphComment* xiphTags, Fy::Core::Track& track)
{
    if(xiphTags->isEmpty()) {
        return;
    }

    const TagLib::Ogg::FieldListMap& fields = xiphTags->fieldListMap();

    if(fields.contains(Tag::TrackNumber)) {
        const TagLib::StringList& trackNumber = fields[Tag::TrackNumber];
        if(!trackNumber.isEmpty() && !trackNumber.front().isEmpty()) {
            track.setTrackNumber(trackNumber.front().toInt());
        }
    }

    if(fields.contains(Tag::TrackTotal)) {
        const TagLib::StringList& trackTotal = fields[Tag::TrackTotal];
        if(!trackTotal.isEmpty() && !trackTotal.front().isEmpty()) {
            track.setTrackTotal(trackTotal.front().toInt());
        }
    }

    if(fields.contains(Tag::DiscNumber)) {
        const TagLib::StringList& discNumber = fields[Tag::DiscNumber];
        if(!discNumber.isEmpty() && !discNumber.front().isEmpty()) {
            track.setDiscNumber(discNumber.front().toInt());
        }
    }

    if(fields.contains(Tag::DiscTotal)) {
        const TagLib::StringList& discTotal = fields[Tag::DiscTotal];
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

void readAsfTags(const TagLib::ASF::Tag* asfTags, Fy::Core::Track& track)
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

QString coverInDirectory(const QString& directory)
{
    static const QStringList CoverFileTypes{"*.jpg", "*.jpeg", "*.png", "*.gif", "*.tiff", "*.bmp"};

    const QDir baseDirectory{directory};
    const QStringList fileList = baseDirectory.entryList(CoverFileTypes, QDir::Files);
    if(!fileList.isEmpty()) {
        // Use first image found as album cover
        return baseDirectory.absolutePath() + "/" + fileList.constFirst();
    }
    return {};
}

void handleCover(const QByteArray& cover, Fy::Core::Track& track)
{
    QString coverPath = coverInDirectory(track.filepath());

    if(!cover.isEmpty()) {
        coverPath = Fy::Core::Constants::EmbeddedCover;
    }

    track.setCoverPath(coverPath);
}
} // namespace

namespace Fy::Core::Tagging {
struct TagReader::Private
{
    QMimeDatabase mimeDb;
};

TagReader::TagReader()
    : p{std::make_unique<Private>()}
{ }

TagReader::~TagReader() = default;

// TODO: Implement a file/mime type resolver
bool TagReader::readMetaData(Track& track, Quality quality)
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

    QString mimeType = p->mimeDb.mimeTypeForFile(filepath).name();
    const auto style = readStyle(quality);

    const auto readProperties = [](const TagLib::File& file, Track& track, bool skipExtra = false) {
        readAudioProperties(file, track);
        readGeneralProperties(file.properties(), track, skipExtra);
    };

    if(mimeType == "audio/ogg"_L1 || mimeType == "audio/x-vorbis+ogg"_L1) {
        // Workaround for opus files with ogg suffix returning incorrect type
        mimeType = p->mimeDb.mimeTypeForFile(filepath, QMimeDatabase::MatchContent).name();
    }
    if(mimeType == "audio/mpeg"_L1 || mimeType == "audio/mpeg3"_L1 || mimeType == "audio/x-mpeg"_L1) {
        TagLib::MPEG::File file(&stream, TagLib::ID3v2::FrameFactory::instance(), true, style);
        if(file.isValid()) {
            readProperties(file, track);
            if(file.hasID3v2Tag()) {
                readId3Tags(file.ID3v2Tag(), track);
                handleCover(readId3Cover(file.ID3v2Tag()), track);
            }
        }
    }
    else if(mimeType == "audio/x-aiff"_L1 || mimeType == "audio/x-aifc"_L1) {
        const TagLib::RIFF::AIFF::File file(&stream, true, style);
        if(file.isValid()) {
            readProperties(file, track);
            if(file.hasID3v2Tag()) {
                readId3Tags(file.tag(), track);
                handleCover(readId3Cover(file.tag()), track);
            }
        }
    }
    else if(mimeType == "audio/vnd.wave"_L1 || mimeType == "audio/wav"_L1 || mimeType == "audio/x-wav"_L1) {
        const TagLib::RIFF::WAV::File file(&stream, true, style);
        if(file.isValid()) {
            readProperties(file, track);
            if(file.hasID3v2Tag()) {
                readId3Tags(file.ID3v2Tag(), track);
                handleCover(readId3Cover(file.tag()), track);
            }
        }
    }
    else if(mimeType == "audio/x-musepack"_L1) {
        TagLib::MPC::File file(&stream, true, style);
        if(file.isValid()) {
            readProperties(file, track);
            if(file.APETag()) {
                readApeTags(file.APETag(), track);
                handleCover(readApeCover(file.APETag()), track);
            }
        }
    }
    else if(mimeType == "audio/x-ape"_L1) {
        TagLib::APE::File file(&stream, true, style);
        if(file.isValid()) {
            readProperties(file, track);
            if(file.APETag()) {
                readApeTags(file.APETag(), track);
                handleCover(readApeCover(file.APETag()), track);
            }
        }
    }
    else if(mimeType == "audio/x-wavpack"_L1) {
        TagLib::WavPack::File file(&stream, true, style);
        if(file.isValid()) {
            readProperties(file, track);
            if(file.APETag()) {
                readApeTags(file.APETag(), track);
                handleCover(readApeCover(file.APETag()), track);
            }
        }
    }
    else if(mimeType == "audio/mp4"_L1 || mimeType == "audio/vnd.audible.aax"_L1) {
        const TagLib::MP4::File file(&stream, true, style);
        if(file.isValid()) {
            readProperties(file, track, true);
            if(file.hasMP4Tag()) {
                readMp4Tags(file.tag(), track);
                handleCover(readMp4Cover(file.tag()), track);
            }
        }
    }
    else if(mimeType == "audio/flac"_L1) {
        TagLib::FLAC::File file(&stream, TagLib::ID3v2::FrameFactory::instance(), true, style);
        if(file.isValid()) {
            readProperties(file, track);
            if(file.hasXiphComment()) {
                readXiphComment(file.xiphComment(), track);
            }
            handleCover(readFlacCover(file.pictureList()), track);
        }
    }
    else if(mimeType == "audio/ogg"_L1 || mimeType == "audio/x-vorbis+ogg"_L1) {
        const TagLib::Ogg::Vorbis::File file(&stream, true, style);
        if(file.isValid()) {
            readProperties(file, track);
            if(file.tag()) {
                readXiphComment(file.tag(), track);
                handleCover(readFlacCover(file.tag()->pictureList()), track);
            }
        }
    }
    else if(mimeType == "audio/opus"_L1 || mimeType == "audio/x-opus+ogg"_L1) {
        const TagLib::Ogg::Opus::File file(&stream, true, style);
        if(file.isValid()) {
            readProperties(file, track);
            if(file.tag()) {
                readXiphComment(file.tag(), track);
                handleCover(readFlacCover(file.tag()->pictureList()), track);
            }
        }
    }
    else if(mimeType == "audio/x-ms-wma"_L1) {
        const TagLib::ASF::File file(&stream, true, style);
        if(file.isValid()) {
            readProperties(file, track);
            if(file.tag()) {
                readAsfTags(file.tag(), track);
                handleCover(readAsfCover(file.tag()), track);
            }
        }
    }
    else {
        qDebug() << "Unsupported mime type: " << mimeType;
    }

    track.setType(typeForMime(mimeType));

    return true;
}

QByteArray TagReader::readCover(const Track& track)
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

    QString mimeType = p->mimeDb.mimeTypeForFile(filepath).name();

    if(mimeType == "audio/ogg"_L1 || mimeType == "audio/x-vorbis+ogg"_L1) {
        // Workaround for opus files with ogg suffix returning incorrect type
        mimeType = p->mimeDb.mimeTypeForFile(filepath, QMimeDatabase::MatchContent).name();
    }
    if(mimeType == "audio/mpeg"_L1 || mimeType == "audio/mpeg3"_L1 || mimeType == "audio/x-mpeg"_L1) {
        TagLib::MPEG::File file(&stream, TagLib::ID3v2::FrameFactory::instance(), true);
        if(file.isValid() && file.hasID3v2Tag()) {
            return readId3Cover(file.ID3v2Tag());
        }
    }
    else if(mimeType == "audio/x-aiff"_L1 || mimeType == "audio/x-aifc"_L1) {
        const TagLib::RIFF::AIFF::File file(&stream, true);
        if(file.isValid() && file.hasID3v2Tag()) {
            return readId3Cover(file.tag());
        }
    }
    else if(mimeType == "audio/vnd.wave"_L1 || mimeType == "audio/wav"_L1 || mimeType == "audio/x-wav"_L1) {
        const TagLib::RIFF::WAV::File file(&stream, true);
        if(file.isValid() && file.hasID3v2Tag()) {
            return readId3Cover(file.tag());
        }
    }
    else if(mimeType == "audio/x-musepack"_L1) {
        TagLib::MPC::File file(&stream, true);
        if(file.isValid() && file.APETag()) {
            return readApeCover(file.APETag());
        }
    }
    else if(mimeType == "audio/x-ape"_L1) {
        TagLib::APE::File file(&stream, true);
        if(file.isValid() && file.APETag()) {
            return readApeCover(file.APETag());
        }
    }
    else if(mimeType == "audio/x-wavpack"_L1) {
        TagLib::WavPack::File file(&stream, true);
        if(file.isValid() && file.APETag()) {
            return readApeCover(file.APETag());
        }
    }
    else if(mimeType == "audio/mp4"_L1 || mimeType == "audio/vnd.audible.aax"_L1) {
        const TagLib::MP4::File file(&stream, true);
        if(file.isValid() && file.tag()) {
            return readMp4Cover(file.tag());
        }
    }
    else if(mimeType == "audio/flac"_L1) {
        TagLib::FLAC::File file(&stream, TagLib::ID3v2::FrameFactory::instance(), true);
        if(file.isValid()) {
            return readFlacCover(file.pictureList());
        }
    }
    else if(mimeType == "audio/ogg"_L1 || mimeType == "audio/x-vorbis+ogg"_L1) {
        const TagLib::Ogg::Vorbis::File file(&stream, true);
        if(file.isValid() && file.tag()) {
            return readFlacCover(file.tag()->pictureList());
        }
    }
    else if(mimeType == "audio/opus"_L1 || mimeType == "audio/x-opus+ogg"_L1) {
        const TagLib::Ogg::Opus::File file(&stream, true);
        if(file.isValid() && file.tag()) {
            return readFlacCover(file.tag()->pictureList());
        }
    }
    else if(mimeType == "audio/x-ms-wma"_L1) {
        const TagLib::ASF::File file(&stream, true);
        if(file.isValid() && file.tag()) {
            return readAsfCover(file.tag());
        }
    }

    return {};
}
} // namespace Fy::Core::Tagging
