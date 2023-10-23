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
#include <QStringBuilder>

#include <set>

namespace {
QString convertString(const TagLib::String& str)
{
    return QString::fromStdString(str.to8Bit(true));
}

QStringList convertStringList(const TagLib::StringList& strList)
{
    QStringList list;
    list.reserve(strList.size());

    for(const auto& string : strList) {
        list.push_back(convertString(string));
    }

    return list;
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
    if(props.contains("TRACKNUMBER")) {
        // TODO: Handle in separate methods based on type
        const auto trackNums = convertString(props["TRACKNUMBER"].toString()).split('/');
        if(trackNums.empty()) {
            return;
        }

        track.setTrackNumber(trackNums.constFirst().toInt());

        if(trackNums.size() > 1) {
            track.setTrackTotal(trackNums.at(1).toInt());
        }
    }
    if(props.contains("TRACKTOTAL")) {
        track.setTrackTotal(props["TRACKTOTAL"].toString().toInt());
    }
    if(props.contains("DISCNUMBER")) {
        // TODO: Handle in separate methods based on type
        const auto discNums = convertString(props["DISCNUMBER"].toString()).split('/');
        if(discNums.empty()) {
            return;
        }

        track.setDiscNumber(discNums.constFirst().toInt());

        if(discNums.size() > 1) {
            track.setDiscTotal(discNums.at(1).toInt());
        }
    }
    if(props.contains("DISCTOTAL")) {
        track.setDiscTotal(props["DISCTOTAL"].toString().toInt());
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
        = {"TITLE",    "ARTIST",    "ALBUM",   "ALBUMARTIST", "TRACKNUMBER", "DISCNUMBER", "GENRE",
           "COMPOSER", "PERFORMER", "COMMENT", "LYRICS",      "DATE",        "RATING"};

    for(const auto& [tag, values] : props) {
        if(!baseTags.contains(tag)) {
            const auto tagEntry = QString::fromStdString(tag.to8Bit(true));
            for(const auto& value : values) {
                track.addExtraTag(tagEntry, QString::fromStdString(value.to8Bit(true)));
            }
        }
    }
}

QByteArray readId3Cover(const TagLib::ID3v2::Tag* id3Tags)
{
    if(id3Tags->isEmpty()) {
        return {};
    }

    TagLib::ID3v2::FrameList frames = id3Tags->frameListMap()["APIC"];

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

QByteArray readApeCover(const TagLib::APE::Tag* apeTags)
{
    if(apeTags->isEmpty()) {
        return {};
    }

    const TagLib::APE::ItemListMap& items          = apeTags->itemListMap();
    TagLib::APE::ItemListMap::ConstIterator itemIt = items.find("COVER ART (FRONT)");

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

QByteArray readMp4Cover(const TagLib::MP4::Tag* mp4Tags)
{
    TagLib::MP4::Item coverArtItem = mp4Tags->item("covr");
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

QByteArray readFlacCover(const TagLib::List<TagLib::FLAC::Picture*> pictures)
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
        return baseDirectory.absolutePath() % "/" % fileList.constFirst();
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
        qWarning() << u"Unable to open file readonly: " << filepath;
        return false;
    }

    const QString mimeType                         = p->mimeDb.mimeTypeForFile(filepath).name();
    const TagLib::AudioProperties::ReadStyle style = readStyle(quality);

    const auto readProperties = [](const TagLib::File& file, Track& track) {
        readAudioProperties(file, track);
        readGeneralProperties(file.properties(), track);
    };

    if(mimeType == u"audio/mpeg" || mimeType == u"audio/mpeg3" || mimeType == u"audio/x-mpeg") {
        TagLib::MPEG::File file(&stream, TagLib::ID3v2::FrameFactory::instance(), true, style);
        if(file.isValid()) {
            readProperties(file, track);
            if(file.hasID3v2Tag()) {
                handleCover(readId3Cover(file.ID3v2Tag()), track);
            }
        }
    }
    else if(mimeType == u"audio/x-aiff" || mimeType == u"audio/x-aifc") {
        TagLib::RIFF::AIFF::File file(&stream, true, style);
        if(file.isValid()) {
            readProperties(file, track);
            if(file.hasID3v2Tag()) {
                handleCover(readId3Cover(file.tag()), track);
            }
        }
    }
    else if(mimeType == u"audio/vnd.wave" || mimeType == u"audio/wav" || mimeType == u"audio/x-wav") {
        TagLib::RIFF::WAV::File file(&stream, true, style);
        if(file.isValid()) {
            readProperties(file, track);
            if(file.hasID3v2Tag()) {
                handleCover(readId3Cover(file.tag()), track);
            }
        }
    }
    else if(mimeType == u"audio/x-musepack") {
        TagLib::MPC::File file(&stream, true, style);
        if(file.isValid()) {
            readProperties(file, track);
            if(file.APETag()) {
                handleCover(readApeCover(file.APETag()), track);
            }
        }
    }
    else if(mimeType == u"audio/x-ape") {
        TagLib::APE::File file(&stream, true, style);
        if(file.isValid()) {
            readProperties(file, track);
            if(file.APETag()) {
                handleCover(readApeCover(file.APETag()), track);
            }
        }
    }
    else if(mimeType == u"audio/x-wavpack") {
        TagLib::WavPack::File file(&stream, true, style);
        if(file.isValid()) {
            readProperties(file, track);
            if(file.APETag()) {
                handleCover(readApeCover(file.APETag()), track);
            }
        }
    }
    else if(mimeType == u"audio/mp4" || mimeType == u"audio/vnd.audible.aax") {
        TagLib::MP4::File file(&stream, true, style);
        if(file.isValid()) {
            readProperties(file, track);
            handleCover(readMp4Cover(file.tag()), track);
        }
    }
    else if(mimeType == u"audio/flac") {
        TagLib::FLAC::File file(&stream, TagLib::ID3v2::FrameFactory::instance(), true, style);
        if(file.isValid()) {
            readProperties(file, track);
            handleCover(readFlacCover(file.pictureList()), track);
        }
    }
    else if(mimeType == u"audio/ogg" || mimeType == u"audio/x-vorbis+ogg") {
        // Workaround for opus files with ogg suffix returning incorrect type
        const QString accurateType = p->mimeDb.mimeTypeForFile(filepath, QMimeDatabase::MatchContent).name();
        if(accurateType == u"audio/opus" || accurateType == u"audio/x-opus+ogg") {
            TagLib::Ogg::Opus::File file(&stream, true, style);
            if(file.isValid()) {
                readProperties(file, track);
                if(file.tag()) {
                    handleCover(readFlacCover(file.tag()->pictureList()), track);
                }
            }
        }
        else {
            TagLib::Ogg::Vorbis::File file(&stream, true, style);
            if(file.isValid()) {
                readProperties(file, track);
                if(file.tag()) {
                    handleCover(readFlacCover(file.tag()->pictureList()), track);
                }
            }
        }
    }
    else if(mimeType == u"audio/opus" || mimeType == u"audio/x-opus+ogg") {
        TagLib::Ogg::Opus::File file(&stream, true, style);
        if(file.isValid()) {
            readProperties(file, track);
            if(file.tag()) {
                handleCover(readFlacCover(file.tag()->pictureList()), track);
            }
        }
    }
    else if(mimeType == u"audio/x-ms-wma") {
        TagLib::ASF::File file(&stream, true, style);
        if(file.isValid()) {
            readProperties(file, track);
            handleCover(readAsfCover(file.tag()), track);
        }
    }
    else {
        qDebug() << "Unsupported mime type: " << mimeType;
    }

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
        qWarning() << u"Unable to open file readonly: " << filepath;
        return {};
    }

    const QString mimeType = p->mimeDb.mimeTypeForFile(filepath).name();

    if(mimeType == u"audio/mpeg" || mimeType == u"audio/mpeg3" || mimeType == u"audio/x-mpeg") {
        TagLib::MPEG::File file(&stream, TagLib::ID3v2::FrameFactory::instance(), true);
        if(file.isValid() && file.hasID3v2Tag()) {
            return readId3Cover(file.ID3v2Tag());
        }
    }
    else if(mimeType == u"audio/x-aiff" || mimeType == u"audio/x-aifc") {
        TagLib::RIFF::AIFF::File file(&stream, true);
        if(file.isValid() && file.hasID3v2Tag()) {
            return readId3Cover(file.tag());
        }
    }
    else if(mimeType == u"audio/vnd.wave" || mimeType == u"audio/wav" || mimeType == u"audio/x-wav") {
        TagLib::RIFF::WAV::File file(&stream, true);
        if(file.isValid() && file.hasID3v2Tag()) {
            return readId3Cover(file.tag());
        }
    }
    else if(mimeType == u"audio/x-musepack") {
        TagLib::MPC::File file(&stream, true);
        if(file.isValid() && file.APETag()) {
            return readApeCover(file.APETag());
        }
    }
    else if(mimeType == u"audio/x-ape") {
        TagLib::APE::File file(&stream, true);
        if(file.isValid() && file.APETag()) {
            return readApeCover(file.APETag());
        }
    }
    else if(mimeType == u"audio/x-wavpack") {
        TagLib::WavPack::File file(&stream, true);
        if(file.isValid() && file.APETag()) {
            return readApeCover(file.APETag());
        }
    }
    else if(mimeType == u"audio/mp4" || mimeType == u"audio/vnd.audible.aax") {
        TagLib::MP4::File file(&stream, true);
        if(file.isValid()) {
            return readMp4Cover(file.tag());
        }
    }
    else if(mimeType == u"audio/flac") {
        TagLib::FLAC::File file(&stream, TagLib::ID3v2::FrameFactory::instance(), true);
        if(file.isValid()) {
            return readFlacCover(file.pictureList());
        }
    }
    else if(mimeType == u"audio/ogg" || mimeType == u"audio/x-vorbis+ogg") {
        TagLib::Ogg::Vorbis::File file(&stream, true);
        if(file.isValid() && file.tag()) {
            return readFlacCover(file.tag()->pictureList());
        }
    }
    else if(mimeType == u"audio/opus" || mimeType == u"audio/x-opus+ogg") {
        TagLib::Ogg::Opus::File file(&stream, true);
        if(file.isValid() && file.tag()) {
            return readFlacCover(file.tag()->pictureList());
        }
    }
    else if(mimeType == u"audio/x-ms-wma") {
        TagLib::ASF::File file(&stream, true);
        if(file.isValid()) {
            return readAsfCover(file.tag());
        }
    }

    return {};
}
} // namespace Fy::Core::Tagging
