/*
 * Fooyin
 * Copyright 2022, Luke Taylor <LukeT1@proton.me>
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

#include "tagutils.h"

#include <QBuffer>
#include <QByteArray>
#include <QPixmap>
#include <QString>
#include <taglib/attachedpictureframe.h>
#include <taglib/fileref.h>
#include <taglib/flacfile.h>
#include <taglib/id3v1tag.h>
#include <taglib/id3v2tag.h>
#include <taglib/mp4file.h>
#include <taglib/mp4tag.h>
#include <taglib/mpegfile.h>
#include <taglib/tstring.h>
#include <taglib/xiphcomment.h>

namespace Tagging {
void scaleImage(QPixmap& image)
{
    static const int maximumSize = 400;
    const int width = image.size().width();
    const int height = image.size().height();
    if(width > maximumSize || height > maximumSize) {
        image = image.scaled(maximumSize, maximumSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
}

QByteArray getCoverFromMpeg(TagLib::MPEG::File* file)
{
    if(file && file->ID3v2Tag()) {
        TagLib::ID3v2::FrameList frames = file->ID3v2Tag()->frameListMap()["APIC"];
        if(!frames.isEmpty()) {
            using PictureFrame = TagLib::ID3v2::AttachedPictureFrame;
            for(const auto& frame : std::as_const(frames)) {
                auto* coverFrame = dynamic_cast<PictureFrame*>(frame);
                const auto type = coverFrame->type();
                if((frame && type == PictureFrame::FrontCover) || type == PictureFrame::Other) {
                    return {coverFrame->picture().data(), coverFrame->picture().size()};
                }
            }
        }
    }
    return {};
}

QByteArray getCoverFromFlac(TagLib::FLAC::File* file)
{
    const TagLib::List<TagLib::FLAC::Picture*> pictureList = file->pictureList();

    if(!pictureList.isEmpty()) {
        for(const auto& picture : std::as_const(pictureList)) {
            if((picture->type() == TagLib::FLAC::Picture::FrontCover
                || picture->type() == TagLib::FLAC::Picture::Other)) {
                return {picture->data().data(), picture->data().size()};
            }
        }
    }
    return {};
}

QByteArray getCoverFromMp4(TagLib::MP4::File* file)
{
    const TagLib::MP4::Item coverItem = file->tag()->item("covr");
    const TagLib::MP4::CoverArtList coverArtList = coverItem.toCoverArtList();
    if(!coverArtList.isEmpty()) {
        const TagLib::MP4::CoverArt& coverArt = coverArtList.front();
        return {coverArt.data().data(), coverArt.data().size()};
    }
    return {};
}

Tagging::TagType getTagTypeFromTag(TagLib::Tag* tag)
{
    if(dynamic_cast<TagLib::ID3v2::Tag*>(tag) != nullptr) {
        return Tagging::TagType::ID3v2;
    }

    if(dynamic_cast<TagLib::ID3v1::Tag*>(tag) != nullptr) {
        return Tagging::TagType::ID3v1;
    }

    if(dynamic_cast<TagLib::Ogg::XiphComment*>(tag) != nullptr) {
        return Tagging::TagType::Xiph;
    }

    if(dynamic_cast<TagLib::MP4::Tag*>(tag) != nullptr) {
        return Tagging::TagType::MP4;
    }

    return Tagging::TagType::Unsupported;
}

FileTags tagsFromMpeg(TagLib::MPEG::File* file)
{
    FileTags parsedTag;

    if(file && file->hasID3v2Tag()) {
        parsedTag.tag = file->ID3v2Tag();
        parsedTag.type = Tagging::TagType::ID3v2;
    }

    else if(file && file->hasID3v1Tag()) {
        parsedTag.tag = file->ID3v1Tag();
        parsedTag.type = Tagging::TagType::ID3v1;
    }

    else if(file) {
        parsedTag.tag = file->ID3v2Tag(true);
        parsedTag.type = Tagging::TagType::ID3v2;
    }

    return parsedTag;
}

FileTags tagsFromFlac(TagLib::FLAC::File* file)
{
    FileTags parsedTag;

    if(file && file->hasXiphComment()) {
        parsedTag.tag = file->xiphComment();
        parsedTag.type = Tagging::TagType::Xiph;
    }

    else if(file && file->hasID3v2Tag()) {
        parsedTag.tag = file->ID3v2Tag();
        parsedTag.type = Tagging::TagType::ID3v2;
    }

    else if(file && file->hasID3v1Tag()) {
        parsedTag.tag = file->ID3v1Tag();
        parsedTag.type = Tagging::TagType::ID3v1;
    }

    else if(file && file->tag()) {
        parsedTag.tag = file->tag();
        parsedTag.type = Tagging::TagType::Unknown;
    }

    else if(file && !file->tag()) {
        parsedTag.tag = file->ID3v2Tag(true);
        parsedTag.type = Tagging::TagType::ID3v2;
    }

    return parsedTag;
}

FileTags tagsFromMP4(TagLib::MP4::File* file)
{
    FileTags parsedTag;

    if(file && file->hasMP4Tag()) {
        parsedTag.tag = file->tag();
        parsedTag.type = Tagging::TagType::MP4;
    }

    return parsedTag;
}

TagLib::String convertString(const QString& str)
{
    return {str.toUtf8().data(), TagLib::String::Type::UTF8};
}

QString convertString(const TagLib::String& str)
{
    return QString::fromStdString(str.to8Bit(true));
}

QStringList convertStringList(const TagLib::StringList& str)
{
    QStringList list;

    for(const auto& string : str) {
        list += TStringToQString(string);
    }

    list.sort();

    return list;
}

FileTags tagsFromFile(const TagLib::FileRef& fileRef)
{
    FileTags parsedTag;

    parsedTag.tag = nullptr;
    parsedTag.type = TagType::Unsupported;

    if(auto* mpg = dynamic_cast<TagLib::MPEG::File*>(fileRef.file()); mpg) {
        parsedTag = tagsFromMpeg(mpg);
    }

    else if(auto* flac = dynamic_cast<TagLib::FLAC::File*>(fileRef.file()); flac) {
        parsedTag = tagsFromFlac(flac);
    }

    else if(auto* mp4 = dynamic_cast<TagLib::MP4::File*>(fileRef.file()); mp4) {
        parsedTag = tagsFromMP4(mp4);
    }

    else if(fileRef.file()) {
        parsedTag.tag = fileRef.tag();
        parsedTag.type = TagType::Unknown;
    }

    const auto tagType = getTagTypeFromTag(fileRef.tag());
    if(tagType != Tagging::TagType::Unsupported) {
        parsedTag.type = tagType;
    }

    return parsedTag;
}

QPixmap coverFromFile(const TagLib::FileRef& fileRef)
{
    QByteArray data;

    if(auto* mpg = dynamic_cast<TagLib::MPEG::File*>(fileRef.file()); mpg) {
        data = getCoverFromMpeg(mpg);
    }

    else if(auto* flac = dynamic_cast<TagLib::FLAC::File*>(fileRef.file()); flac) {
        data = getCoverFromFlac(flac);
    }

    else if(auto* mp4 = dynamic_cast<TagLib::MP4::File*>(fileRef.file()); mp4) {
        data = getCoverFromMp4(mp4);
    }

    QPixmap cover;
    cover.loadFromData(data);
    scaleImage(cover);
    return cover;
}

bool isValidFile(const TagLib::FileRef& fileRef)
{
    return (!fileRef.isNull() && fileRef.tag() && fileRef.file() && fileRef.file()->isValid());
}

} // namespace Tagging
