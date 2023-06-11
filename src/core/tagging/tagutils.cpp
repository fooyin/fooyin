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

#include "tagutils.h"

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

#include <QBuffer>
#include <QByteArray>
#include <QFile>
#include <QPixmap>
#include <QString>

namespace Fy::Core::Tagging {
using FlacPicture  = TagLib::FLAC::Picture;
using PictureFrame = TagLib::ID3v2::AttachedPictureFrame;

QByteArray getCoverFromMpeg(TagLib::MPEG::File* file)
{
    if(!file || !file->ID3v2Tag()) {
        return {};
    }

    TagLib::ID3v2::FrameList frames = file->ID3v2Tag()->frameListMap()["APIC"];
    if(frames.isEmpty()) {
        return {};
    }

    for(const auto& frame : std::as_const(frames)) {
        if(frame) {
            if(auto* coverFrame = dynamic_cast<PictureFrame*>(frame)) {
                const auto type = coverFrame->type();
                if(type == PictureFrame::FrontCover || type == PictureFrame::Other) {
                    return {coverFrame->picture().data(), coverFrame->picture().size()};
                }
            }
        }
    }
    return {};
}

QByteArray getCoverFromFlac(TagLib::FLAC::File* file)
{
    const TagLib::List<FlacPicture*> pictureList = file->pictureList();
    if(pictureList.isEmpty()) {
        return {};
    }

    for(const auto& picture : std::as_const(pictureList)) {
        if(picture) {
            const auto type = picture->type();
            if(type == FlacPicture::FrontCover || type == FlacPicture::Other) {
                return {picture->data().data(), picture->data().size()};
            }
        }
    }
    return {};
}

QByteArray getCoverFromMp4(TagLib::MP4::File* file)
{
    const TagLib::MP4::Item coverItem            = file->tag()->item("covr");
    const TagLib::MP4::CoverArtList coverArtList = coverItem.toCoverArtList();

    if(coverArtList.isEmpty()) {
        return {};
    }

    const auto& coverArt = coverArtList.front();
    return {coverArt.data().data(), coverArt.data().size()};
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
        parsedTag.tag  = file->ID3v2Tag();
        parsedTag.type = Tagging::TagType::ID3v2;
    }

    else if(file && file->hasID3v1Tag()) {
        parsedTag.tag  = file->ID3v1Tag();
        parsedTag.type = Tagging::TagType::ID3v1;
    }

    else if(file) {
        parsedTag.tag  = file->ID3v2Tag(true);
        parsedTag.type = Tagging::TagType::ID3v2;
    }

    return parsedTag;
}

FileTags tagsFromFlac(TagLib::FLAC::File* file)
{
    FileTags parsedTag;

    if(file && file->hasXiphComment()) {
        parsedTag.tag  = file->xiphComment();
        parsedTag.type = Tagging::TagType::Xiph;
    }

    else if(file && file->hasID3v2Tag()) {
        parsedTag.tag  = file->ID3v2Tag();
        parsedTag.type = Tagging::TagType::ID3v2;
    }

    else if(file && file->hasID3v1Tag()) {
        parsedTag.tag  = file->ID3v1Tag();
        parsedTag.type = Tagging::TagType::ID3v1;
    }

    else if(file && file->tag()) {
        parsedTag.tag  = file->tag();
        parsedTag.type = Tagging::TagType::Unknown;
    }

    else if(file && !file->tag()) {
        parsedTag.tag  = file->ID3v2Tag(true);
        parsedTag.type = Tagging::TagType::ID3v2;
    }

    return parsedTag;
}

FileTags tagsFromMP4(TagLib::MP4::File* file)
{
    FileTags parsedTag;

    if(file && file->hasMP4Tag()) {
        parsedTag.tag  = file->tag();
        parsedTag.type = Tagging::TagType::MP4;
    }

    return parsedTag;
}

bool isValidFile(const TagLib::FileRef& fileRef)
{
    return (!fileRef.isNull() && fileRef.tag() && fileRef.file() && fileRef.file()->isValid());
}

FileTags tagsFromFile(const TagLib::FileRef& fileRef)
{
    FileTags parsedTag;

    parsedTag.tag  = nullptr;
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
        parsedTag.tag  = fileRef.tag();
        parsedTag.type = TagType::Unknown;
    }

    const auto tagType = getTagTypeFromTag(fileRef.tag());
    if(tagType != Tagging::TagType::Unsupported) {
        parsedTag.type = tagType;
    }

    return parsedTag;
}

bool hasEmbeddedCover(const TagLib::FileRef& fileRef)
{
    QByteArray data;

    if(auto* mpg = dynamic_cast<TagLib::MPEG::File*>(fileRef.file())) {
        data = getCoverFromMpeg(mpg);
    }

    else if(auto* flac = dynamic_cast<TagLib::FLAC::File*>(fileRef.file())) {
        data = getCoverFromFlac(flac);
    }

    else if(auto* mp4 = dynamic_cast<TagLib::MP4::File*>(fileRef.file())) {
        data = getCoverFromMp4(mp4);
    }

    return !data.isEmpty();
}

QPixmap coverFromFile(const TagLib::FileRef& fileRef)
{
    QByteArray data;

    if(auto* mpg = dynamic_cast<TagLib::MPEG::File*>(fileRef.file())) {
        data = getCoverFromMpeg(mpg);
    }

    else if(auto* flac = dynamic_cast<TagLib::FLAC::File*>(fileRef.file())) {
        data = getCoverFromFlac(flac);
    }

    else if(auto* mp4 = dynamic_cast<TagLib::MP4::File*>(fileRef.file())) {
        data = getCoverFromMp4(mp4);
    }

    QPixmap cover;
    cover.loadFromData(data);
    return cover;
}

bool saveCover(const QPixmap& cover, const QString& path)
{
    QFile file{path};
    file.open(QIODevice::WriteOnly);
    return cover.save(&file, "JPG", 100);
}

TagLib::String convertString(const QString& str)
{
    return {str.toUtf8().data(), TagLib::String::Type::UTF8};
}

QString convertString(const TagLib::String& str)
{
    return QString::fromStdString(str.to8Bit(true));
}

TagLib::String convertString(int num)
{
    auto str = QString::number(num);
    return convertString(str);
}

int convertNumber(const TagLib::String& num)
{
    return convertString(num).toInt();
}

TagLib::StringList convertStringList(const QStringList& str)
{
    TagLib::StringList list;

    for(const auto& string : str) {
        list.append(TagLib::String(string.toUtf8(), TagLib::String::Type::UTF8));
    }

    return list;
}

QStringList convertStringList(const TagLib::StringList& str)
{
    QStringList list;

    for(const auto& string : str) {
        list.emplace_back(TStringToQString(string));
    }

    std::sort(list.begin(), list.end());

    return list;
}
} // namespace Fy::Core::Tagging
