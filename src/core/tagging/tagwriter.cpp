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

#include <core/tagging/tagwriter.h>

#include <core/track.h>

#include <taglib/aifffile.h>
#include <taglib/apefile.h>
#include <taglib/apetag.h>
#include <taglib/asffile.h>
#include <taglib/asftag.h>
#include <taglib/fileref.h>
#include <taglib/flacfile.h>
#include <taglib/id3v2framefactory.h>
#include <taglib/id3v2tag.h>
#include <taglib/mp4file.h>
#include <taglib/mp4tag.h>
#include <taglib/mpcfile.h>
#include <taglib/mpegfile.h>
#include <taglib/opusfile.h>
#include <taglib/tag.h>
#include <taglib/textidentificationframe.h>
#include <taglib/tfilestream.h>
#include <taglib/tpropertymap.h>
#include <taglib/vorbisfile.h>
#include <taglib/wavfile.h>
#include <taglib/wavpackfile.h>

#include <QFileInfo>
#include <QMimeDatabase>

#include <set>

using namespace Qt::Literals::StringLiterals;

namespace {
TagLib::String convertString(const QString& str)
{
    return QStringToTString(str);
}

TagLib::StringList convertStringList(const QStringList& strList)
{
    TagLib::StringList list;

    for(const QString& str : strList) {
        list.append(convertString(str));
    }

    return list;
}

void writeGenericProperties(TagLib::PropertyMap& oldProperties, const Fy::Core::Track& track)
{
    if(!track.isValid()) {
        return;
    }

    if(!track.title().isEmpty()) {
        oldProperties.replace("TITLE", convertString(track.title()));
    }

    if(!track.artists().empty()) {
        oldProperties.replace("ARTIST", convertStringList(track.artists()));
    }

    if(!track.album().isEmpty()) {
        oldProperties.replace("ALBUM", convertString(track.album()));
    }

    if(!track.albumArtist().isEmpty()) {
        oldProperties.replace("ALBUMARTIST", convertString(track.albumArtist()));
    }

    if(track.trackNumber() >= 0) {
        const auto trackNums = TStringToQString(oldProperties["TRACKNUMBER"].toString()).split('/');
        QString trackNumber  = QString::number(track.trackNumber());
        if(trackNums.size() > 1) {
            trackNumber += "/" + QString::number(track.trackTotal());
        }
        oldProperties.replace("TRACKNUMBER", convertString(trackNumber));
    }

    if(track.discNumber() >= 0) {
        const auto discNums = TStringToQString(oldProperties["DISCNUMBER"].toString()).split('/');
        QString discNumber  = QString::number(track.discNumber());
        if(discNums.size() > 1) {
            discNumber += "/" + QString::number(track.discTotal());
        }
        oldProperties.replace("DISCNUMBER", convertString(discNumber));
    }

    if(!track.genres().empty()) {
        oldProperties.replace("GENRE", convertStringList(track.genres()));
    }

    if(!track.composer().isEmpty()) {
        oldProperties.replace("COMPOSER", convertString(track.composer()));
    }

    if(!track.performer().isEmpty()) {
        oldProperties.replace("PERFORMER", convertString(track.performer()));
    }

    if(!track.comment().isEmpty()) {
        oldProperties.replace("COMMENT", convertString(track.comment()));
    }

    if(!track.lyrics().isEmpty()) {
        oldProperties.replace("LYRICS", convertString(track.lyrics()));
    }

    if(!track.date().isEmpty()) {
        if(track.year() >= 0) {
            oldProperties.replace("DATE", convertString(track.date()));
        }
    }

    static const std::set<TagLib::String> baseTags
        = {"TITLE", "ARTIST",   "ALBUM",     "ALBUMARTIST", "TRACKNUMBER", "TRACKTOTAL", "DISCNUMBER", "DISCTOTAL",
           "GENRE", "COMPOSER", "PERFORMER", "COMMENT",     "LYRICS",      "DATE",       "RATING"};

    const auto customTags = track.extraTags();
    for(const auto& [tag, values] : customTags) {
        oldProperties.replace(convertString(tag), convertStringList(values));
    }

    TagLib::StringList tagsToRemove;

    for(const auto& [tag, values] : oldProperties) {
        if(!baseTags.contains(tag) && !customTags.contains(TStringToQString(tag))) {
            tagsToRemove.append(tag);
        }
    }

    for(const TagLib::String& tag : tagsToRemove) {
        oldProperties.erase(tag);
    }
}

QString getTrackNumber(const Fy::Core::Track& track)
{
    QString trackNumber;
    if(track.trackNumber() > 0) {
        trackNumber += QString::number(track.trackNumber());
    }
    if(track.trackTotal() > 0) {
        trackNumber += "/" + QString::number(track.trackTotal());
    }
    return trackNumber;
}

QString getDiscNumber(const Fy::Core::Track& track)
{
    QString discNumber;
    if(track.discNumber() > 0) {
        discNumber += QString::number(track.discNumber());
    }
    if(track.discTotal() > 0) {
        discNumber += "/" + QString::number(track.discTotal());
    }
    return discNumber;
}

void writeID3v2Tags(TagLib::ID3v2::Tag* id3Tags, const Fy::Core::Track& track)
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
    auto discFrame           = std::make_unique<TagLib::ID3v2::TextIdentificationFrame>("TPOS", TagLib::String::UTF8);
    discFrame->setText(convertString(discNumber));
    id3Tags->addFrame(discFrame.release());
}

void writeApeTags(TagLib::APE::Tag* apeTags, const Fy::Core::Track& track)
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
}

void writeMp4Tags(TagLib::MP4::Tag* mp4Tags, const Fy::Core::Track& track)
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
}

void writeXiphComment(TagLib::Ogg::XiphComment* xiphTags, const Fy::Core::Track& track)
{
    if(track.trackNumber() < 0) {
        xiphTags->removeFields("TRACKNUMBER");
    }
    else {
        xiphTags->addField("TRACKNUMBER", TagLib::String::number(track.trackNumber()), true);
    }

    if(track.trackTotal() < 0) {
        xiphTags->removeFields("TRACKTOTAL");
    }
    else {
        xiphTags->addField("TRACKTOTAL", TagLib::String::number(track.trackTotal()), true);
    }

    if(track.discNumber() < 0) {
        xiphTags->removeFields("DISCNUMBER");
    }
    else {
        xiphTags->addField("DISCNUMBER", TagLib::String::number(track.discNumber()), true);
    }

    if(track.discTotal() < 0) {
        xiphTags->removeFields("DISCTOTAL");
    }
    else {
        xiphTags->addField("DISCTOTAL", TagLib::String::number(track.discTotal()), true);
    }
}

void writeAsfTags(TagLib::ASF::Tag* asfTags, const Fy::Core::Track& track)
{
    asfTags->setAttribute("WM/TrackNumber", TagLib::String::number(track.trackNumber()));
    asfTags->setAttribute("WM/PartOfSet", TagLib::String::number(track.discNumber()));
}
} // namespace

namespace Fy::Core::Tagging {
struct TagWriter::Private
{
    QMimeDatabase mimeDb;
};

TagWriter::TagWriter()
    : p{std::make_unique<Private>()}
{ }

TagWriter::~TagWriter() = default;

bool TagWriter::writeMetaData(const Track& track)
{
    const QString filepath = track.filepath();

    TagLib::FileStream stream(filepath.toUtf8().constData(), false);

    if(!stream.isOpen() || stream.readOnly()) {
        return false;
    }

    const auto writeProperties = [](TagLib::File& file, const Track& track) {
        auto savedProperties = file.properties();
        writeGenericProperties(savedProperties, track);
        file.setProperties(savedProperties);
    };

    QString mimeType = p->mimeDb.mimeTypeForFile(filepath).name();

    if(mimeType == "audio/ogg"_L1 || mimeType == "audio/x-vorbis+ogg"_L1) {
        // Workaround for opus files with ogg suffix returning incorrect type
        mimeType = p->mimeDb.mimeTypeForFile(filepath, QMimeDatabase::MatchContent).name();
    }
    if(mimeType == "audio/mpeg"_L1 || mimeType == "audio/mpeg3"_L1 || mimeType == "audio/x-mpeg"_L1) {
        TagLib::MPEG::File file(&stream, TagLib::ID3v2::FrameFactory::instance(), false);
        if(file.isValid()) {
            writeProperties(file, track);
            if(file.hasID3v2Tag()) {
                writeID3v2Tags(file.ID3v2Tag(), track);
            }
            file.save();
        }
    }
    else if(mimeType == "audio/x-aiff"_L1 || mimeType == "audio/x-aifc"_L1) {
        TagLib::RIFF::AIFF::File file(&stream, false);
        if(file.isValid()) {
            writeProperties(file, track);
            if(file.hasID3v2Tag()) {
                writeID3v2Tags(file.tag(), track);
            }
            file.save();
        }
    }
    else if(mimeType == "audio/vnd.wave"_L1 || mimeType == "audio/wav"_L1 || mimeType == "audio/x-wav"_L1) {
        TagLib::RIFF::WAV::File file(&stream, false);
        if(file.isValid()) {
            writeProperties(file, track);
            if(file.hasID3v2Tag()) {
                writeID3v2Tags(file.ID3v2Tag(), track);
            }
            file.save();
        }
    }
    else if(mimeType == "audio/x-musepack"_L1) {
        TagLib::MPC::File file(&stream, false);
        if(file.isValid()) {
            writeProperties(file, track);
            if(file.hasAPETag()) {
                writeApeTags(file.APETag(), track);
            }
            file.save();
        }
    }
    else if(mimeType == "audio/x-ape"_L1) {
        TagLib::APE::File file(&stream, false);
        if(file.isValid()) {
            writeProperties(file, track);
            if(file.hasAPETag()) {
                writeApeTags(file.APETag(), track);
            }
            file.save();
        }
    }
    else if(mimeType == "audio/x-wavpack"_L1) {
        TagLib::WavPack::File file(&stream, false);
        if(file.isValid()) {
            writeProperties(file, track);
            if(file.hasAPETag()) {
                writeApeTags(file.APETag(), track);
            }
            file.save();
        }
    }
    else if(mimeType == "audio/mp4"_L1) {
        TagLib::MP4::File file(&stream, false);
        if(file.isValid()) {
            writeProperties(file, track);
            if(file.hasMP4Tag()) {
                writeMp4Tags(file.tag(), track);
            }
            file.save();
        }
    }
    else if(mimeType == "audio/flac"_L1) {
        TagLib::FLAC::File file(&stream, TagLib::ID3v2::FrameFactory::instance(), false);
        if(file.isValid()) {
            writeProperties(file, track);
            if(file.hasXiphComment()) {
                writeXiphComment(file.xiphComment(), track);
            }
            file.save();
        }
    }
    else if(mimeType == "audio/ogg"_L1 || mimeType == "audio/x-vorbis+ogg"_L1) {
        TagLib::Ogg::Vorbis::File file(&stream, false);
        if(file.isValid()) {
            writeProperties(file, track);
            if(file.tag()) {
                writeXiphComment(file.tag(), track);
            }
            file.save();
        }
    }
    else if(mimeType == "audio/opus"_L1 || mimeType == "audio/x-opus+ogg"_L1) {
        TagLib::Ogg::Opus::File file(&stream, false);
        if(file.isValid()) {
            writeProperties(file, track);
            writeXiphComment(file.tag(), track);
            file.save();
        }
    }
    else if(mimeType == "audio/x-ms-wma"_L1) {
        TagLib::ASF::File file(&stream, false);
        if(file.isValid()) {
            writeProperties(file, track);
            if(file.tag()) {
                writeAsfTags(file.tag(), track);
            }
            file.save();
        }
    }
    return true;
}
} // namespace Fy::Core::Tagging
