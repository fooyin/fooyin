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

#include <core/constants.h>
#include <core/track.h>
#include <utils/settings/settingsmanager.h>

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

void readGeneralProperties(const TagLib::PropertyMap& props, Fy::Core::Track& track)
{
    if(props.isEmpty()) {
        return;
    }

    if(props.contains("TITLE")) {
        track.setTitle(convertString(props["TITLE"].toString()));
    }
    if(props.contains("ARTIST")) {
        track.setArtists(convertStringList(props["ARTIST"]));
    }
    if(props.contains("ALBUM")) {
        track.setAlbum(convertString(props["ALBUM"].toString()));
    }
    if(props.contains("ALBUMARTIST")) {
        // TODO: Support multiple album artists
        track.setAlbumArtist(convertString(props["ALBUMARTIST"].toString()));
    }
    if(props.contains("GENRE")) {
        track.setGenres(convertStringList(props["GENRE"]));
    }
    if(props.contains("COMPOSER")) {
        track.setComposer(convertString(props["COMPOSER"].toString()));
    }
    if(props.contains("PERFORMER")) {
        track.setPerformer(convertString(props["PERFORMER"].toString()));
    }
    if(props.contains("COMMENT")) {
        track.setComment(convertString(props["COMMENT"].toString()));
    }
    if(props.contains("LYRICS")) {
        track.setLyrics(convertString(props["LYRICS"].toString()));
    }
    if(props.contains("DATE")) {
        track.setDate(convertString(props["DATE"].toString()));
        track.setYear(track.date().toInt());
    }
    if(props.contains("RATING")) {
        // TODO
    }

    static const std::set<TagLib::String> baseTags
        = {"TITLE", "ARTIST",   "ALBUM",     "ALBUMARTIST", "TRACKNUMBER", "TRACKTOTAL", "DISCNUMBER", "DISCTOTAL",
           "GENRE", "COMPOSER", "PERFORMER", "COMMENT",     "LYRICS",      "DATE",       "RATING"};

    for(const auto& [tag, values] : props) {
        if(!baseTags.contains(tag)) {
            const auto tagEntry = QString::fromStdString(tag.to8Bit(true));
            for(const auto& value : values) {
                track.addExtraTag(tagEntry, QString::fromStdString(value.to8Bit(true)));
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

void readMp4Tags(const TagLib::MP4::Tag* mp4Tags, Fy::Core::Track& track)
{
    if(mp4Tags->isEmpty()) {
        return;
    }

    if(mp4Tags->contains("TRKN")) {
        const TagLib::MP4::Item::IntPair& trackNumbers = mp4Tags->item("TRKN").toIntPair();
        if(trackNumbers.first > 0) {
            track.setTrackNumber(trackNumbers.first);
        }
        if(trackNumbers.second > 0) {
            track.setTrackTotal(trackNumbers.second);
        }
    }

    if(mp4Tags->contains("DISK")) {
        const TagLib::MP4::Item::IntPair& discNumbers = mp4Tags->item("DISK").toIntPair();
        if(discNumbers.first > 0) {
            track.setDiscNumber(discNumbers.first);
        }
        if(discNumbers.second > 0) {
            track.setDiscTotal(discNumbers.second);
        }
    }
}

QByteArray readMp4Cover(const TagLib::MP4::Tag* mp4Tags)
{
    const TagLib::MP4::Item coverArtItem = mp4Tags->item("covr");
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

    if(fields.contains("TRACKNUMBER")) {
        const TagLib::StringList& trackNumber = fields["TRACKNUMBER"];
        if(!trackNumber.isEmpty() && !trackNumber.front().isEmpty()) {
            track.setTrackNumber(trackNumber.front().toInt());
        }
    }

    if(fields.contains("TRACKTOTAL")) {
        const TagLib::StringList& trackTotal = fields["TRACKTOTAL"];
        if(!trackTotal.isEmpty() && !trackTotal.front().isEmpty()) {
            track.setTrackTotal(trackTotal.front().toInt());
        }
    }

    if(fields.contains("DISCNUMBER")) {
        const TagLib::StringList& discNumber = fields["DISCNUMBER"];
        if(!discNumber.isEmpty() && !discNumber.front().isEmpty()) {
            track.setDiscNumber(discNumber.front().toInt());
        }
    }

    if(fields.contains("DISCTOTAL")) {
        const TagLib::StringList& discTotal = fields["DISCTOTAL"];
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

    const auto readProperties = [](const TagLib::File& file, Track& track) {
        readAudioProperties(file, track);
        readGeneralProperties(file.properties(), track);
    };

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
                readId3Tags(file.tag(), track);
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
            readProperties(file, track);
            readMp4Tags(file.tag(), track);
            handleCover(readMp4Cover(file.tag()), track);
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
        // Workaround for opus files with ogg suffix returning incorrect type
        mimeType = p->mimeDb.mimeTypeForFile(filepath, QMimeDatabase::MatchContent).name();
        if(mimeType == "audio/opus"_L1 || mimeType == "audio/x-opus+ogg"_L1) {
            const TagLib::Ogg::Opus::File file(&stream, true, style);
            if(file.isValid()) {
                readProperties(file, track);
                if(file.tag()) {
                    readXiphComment(file.tag(), track);
                    handleCover(readFlacCover(file.tag()->pictureList()), track);
                }
            }
        }
        else {
            const TagLib::Ogg::Vorbis::File file(&stream, true, style);
            if(file.isValid()) {
                readProperties(file, track);
                if(file.tag()) {
                    readXiphComment(file.tag(), track);
                    handleCover(readFlacCover(file.tag()->pictureList()), track);
                }
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
            readAsfTags(file.tag(), track);
            handleCover(readAsfCover(file.tag()), track);
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

    const QString mimeType = p->mimeDb.mimeTypeForFile(filepath).name();

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
        if(file.isValid()) {
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
        if(file.isValid()) {
            return readAsfCover(file.tag());
        }
    }

    return {};
}
} // namespace Fy::Core::Tagging
