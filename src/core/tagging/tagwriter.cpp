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

#include "tagdefs.h"

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
using namespace Fy::Core::Tagging;

constexpr std::array supportedMp4Tags{
    std::pair(Tag::Title, Mp4::Title),
    std::pair(Tag::Artist, Mp4::Artist),
    std::pair(Tag::Album, Mp4::Album),
    std::pair(Tag::AlbumArtist, Mp4::AlbumArtist),
    std::pair(Tag::Genre, Mp4::Genre),
    std::pair(Tag::Composer, Mp4::Composer),
    std::pair(Tag::Performer, Mp4::Performer),
    std::pair(Tag::Comment, Mp4::Comment),
    std::pair(Tag::Lyrics, Mp4::Lyrics),
    std::pair(Tag::Date, Mp4::Date),
    std::pair(Tag::Rating, Mp4::Rating),
    std::pair(Tag::TrackNumber, Mp4::TrackNumber),
    std::pair(Tag::DiscNumber, Mp4::DiscNumber),
    std::pair("COMPILATION", "cpil"),
    std::pair("BPM", "tmpo"),
    std::pair("COPYRIGHT", "cprt"),
    std::pair("ENCODEDBY", "\251too"),
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
    std::pair("ORIGINALDATE", "----:com.apple.iTunes:originaldate"),
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

TagLib::String findMp4Tag(const QString& tag)
{
    for(const auto& [key, value] : supportedMp4Tags) {
        if(tag == key) {
            return value;
        }
    }
    return {};
}

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

void writeGenericProperties(TagLib::PropertyMap& oldProperties, const Fy::Core::Track& track, bool skipExtra = false)
{
    if(!track.isValid()) {
        return;
    }

    if(!track.title().isEmpty()) {
        oldProperties.replace(Tag::Title, convertString(track.title()));
    }

    if(!track.artists().empty()) {
        oldProperties.replace(Tag::Artist, convertStringList(track.artists()));
    }

    if(!track.album().isEmpty()) {
        oldProperties.replace(Tag::Album, convertString(track.album()));
    }

    if(!track.albumArtist().isEmpty()) {
        oldProperties.replace(Tag::AlbumArtist, convertString(track.albumArtist()));
    }

    if(track.trackNumber() >= 0) {
        const auto trackNums = TStringToQString(oldProperties[Tag::TrackNumber].toString()).split('/');
        QString trackNumber  = QString::number(track.trackNumber());
        if(trackNums.size() > 1) {
            trackNumber += "/" + QString::number(track.trackTotal());
        }
        oldProperties.replace(Tag::TrackNumber, convertString(trackNumber));
    }

    if(track.discNumber() >= 0) {
        const auto discNums = TStringToQString(oldProperties[Tag::DiscNumber].toString()).split('/');
        QString discNumber  = QString::number(track.discNumber());
        if(discNums.size() > 1) {
            discNumber += u"/"_s + QString::number(track.discTotal());
        }
        oldProperties.replace(Tag::DiscNumber, convertString(discNumber));
    }

    if(!track.genres().empty()) {
        oldProperties.replace(Tag::Genre, convertStringList(track.genres()));
    }

    if(!track.composer().isEmpty()) {
        oldProperties.replace(Tag::Composer, convertString(track.composer()));
    }

    if(!track.performer().isEmpty()) {
        oldProperties.replace(Tag::Performer, convertString(track.performer()));
    }

    if(!track.comment().isEmpty()) {
        oldProperties.replace(Tag::Comment, convertString(track.comment()));
    }

    if(!track.lyrics().isEmpty()) {
        oldProperties.replace(Tag::Lyrics, convertString(track.lyrics()));
    }

    if(!track.date().isEmpty()) {
        if(track.year() >= 0) {
            oldProperties.replace("DATE", convertString(track.date()));
        }
    }

    if(!skipExtra) {
        static const std::set<TagLib::String> baseTags
            = {Tag::Title,      Tag::Artist,     Tag::Album,     Tag::AlbumArtist, Tag::TrackNumber,
               Tag::TrackTotal, Tag::DiscNumber, Tag::DiscTotal, Tag::Genre,       Tag::Composer,
               Tag::Performer,  Tag::Comment,    Tag::Lyrics,    Tag::Date,        Tag::Rating};

        const auto customTags = track.extraTags();
        for(const auto& [tag, values] : customTags) {
            const TagLib::String name = convertString(tag);
            if(!baseTags.contains(name)) {
                if(values.empty()) {
                    oldProperties.erase(name);
                }
                oldProperties.replace(name, convertStringList(values));
            }
        }
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

TagLib::String prefixMp4FreeFormName(const QString& name, const TagLib::MP4::ItemMap& items)
{
    if(name.isEmpty()) {
        return {};
    }

    if(name.startsWith("----"_L1) || (name.length() == 4 && name[0] == '\251'_L1)) {
        return {};
    }

    TagLib::String tagKey = convertString(name);

    if(name[0] == ':'_L1) {
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

    mp4Tags->setItem(Mp4::PerformerAlt, TagLib::StringList{convertString(track.performer())});

    static const std::set<QString> baseMp4Tags
        = {Tag::Title,   Tag::Artist, Tag::Album, Tag::AlbumArtist, Tag::Genre,       Tag::Composer,  Tag::Performer,
           Tag::Comment, Tag::Lyrics, Tag::Date,  Tag::Rating,      Tag::TrackNumber, Tag::DiscNumber};

    const auto customTags = track.extraTags();
    for(const auto& [tag, values] : customTags) {
        if(!baseMp4Tags.contains(tag)) {
            TagLib::String tagName = findMp4Tag(tag);
            if(tagName.isEmpty()) {
                tagName = prefixMp4FreeFormName(tag, mp4Tags->itemMap());
            }
            if(values.empty()) {
                mp4Tags->removeItem(tagName);
            }
            else {
                mp4Tags->setItem(tagName, convertStringList(values));
            }
        }
    }
}

void writeXiphComment(TagLib::Ogg::XiphComment* xiphTags, const Fy::Core::Track& track)
{
    if(track.trackNumber() < 0) {
        xiphTags->removeFields(Tag::TrackNumber);
    }
    else {
        xiphTags->addField(Tag::TrackNumber, TagLib::String::number(track.trackNumber()), true);
    }

    if(track.trackTotal() < 0) {
        xiphTags->removeFields(Tag::TrackTotal);
    }
    else {
        xiphTags->addField(Tag::TrackTotal, TagLib::String::number(track.trackTotal()), true);
    }

    if(track.discNumber() < 0) {
        xiphTags->removeFields(Tag::DiscNumber);
    }
    else {
        xiphTags->addField(Tag::DiscNumber, TagLib::String::number(track.discNumber()), true);
    }

    if(track.discTotal() < 0) {
        xiphTags->removeFields(Tag::DiscTotal);
    }
    else {
        xiphTags->addField(Tag::DiscTotal, TagLib::String::number(track.discTotal()), true);
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

    const auto writeProperties = [](TagLib::File& file, const Track& track, bool skipExtra = false) {
        auto savedProperties = file.properties();
        writeGenericProperties(savedProperties, track, skipExtra);
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
            writeProperties(file, track, true);
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
