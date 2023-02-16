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

#include "tags.h"

#include "core/models/track.h"
#include "tagutils.h"

#include <QFileInfo>
#include <QPixmap>
#include <taglib/fileref.h>
#include <taglib/tfilestream.h>

namespace Core::Tagging {
struct ReadingProperties
{
    TagLib::AudioProperties::ReadStyle readStyle{TagLib::AudioProperties::ReadStyle::Fast};
    bool readAudioProperties{true};
};

ReadingProperties getReadingProperties(Quality quality)
{
    ReadingProperties readingProperties;

    switch(quality) {
        case Quality::Quality:
            readingProperties.readStyle = TagLib::AudioProperties::Accurate;
            break;
        case Quality::Standard:
            readingProperties.readStyle = TagLib::AudioProperties::Average;
            break;
        case Quality::Fast:
            readingProperties.readStyle = TagLib::AudioProperties::Fast;
            break;
    }

    return readingProperties;
}

bool readMetaData(Track& track, Quality quality)
{
    const auto filepath = track.filepath();
    const auto fileInfo = QFileInfo(filepath);

    const QDateTime md   = fileInfo.lastModified();
    const qint64 timeNow = QDateTime::currentMSecsSinceEpoch();

    track.setAddedTime(timeNow);
    track.setModifiedTime(md.isValid() ? md.toMSecsSinceEpoch() : 0);

    if(fileInfo.size() <= 0) {
        return false;
    }

    const auto readingProperties = getReadingProperties(quality);
    auto fileRef = TagLib::FileRef(TagLib::FileName(filepath.toUtf8()), readingProperties.readAudioProperties,
                                   readingProperties.readStyle);

    if(!isValidFile(fileRef)) {
        qDebug() << "Cannot open tags for " << filepath << ": Err 1";
        return false;
    }

    auto parsedTag = tagsFromFile(fileRef);
    parsedTag.map  = fileRef.file()->properties();
    if(!parsedTag.tag) {
        return false;
    }

    const QStringList baseTags{"TITLE", "ARTIST",     "ALBUMARTIST", "GENRE",   "TRACKNUMBER",
                               "ALBUM", "DISCNUMBER", "DATE",        "COMMENT", "LYRICS"};

    const auto artists     = convertStringList(parsedTag.map.value("ARTIST"));
    const auto album       = convertString(parsedTag.map.value("ALBUM").toString());
    const auto albumArtist = convertString(parsedTag.map.value("ALBUMARTIST").toString());
    const auto title       = convertString(parsedTag.map.value("TITLE").toString());
    const auto genres      = convertStringList(parsedTag.map.value("GENRE").toString());
    const auto comment     = convertString(parsedTag.map.value("COMMENT").toString());
    const auto date        = convertString(parsedTag.map.value("DATE").toString());
    const auto lyrics      = convertString(parsedTag.map.value("LYRICS").toString());

    // TODO: Check for TRACKTOTAL and DISCTOTAL tags when X/Y not standard i.e. FLAC
    auto trackNum           = 0;
    auto trackTotal         = 0;
    const auto trackNumData = convertString(parsedTag.map.value("TRACKNUMBER").toString());

    if(trackNumData.contains("/")) {
        trackNum   = trackNumData.split("/")[0].toInt();
        trackTotal = trackNumData.split("/")[1].toInt();
    }
    else {
        trackNum = trackNumData.toInt();
    }

    int disc            = 0;
    int discTotal       = 0;
    const auto discData = convertString(parsedTag.map.value("DISCNUMBER").toString());

    if(discData.contains("/")) {
        disc      = discData.split("/")[0].toInt();
        discTotal = discData.split("/")[1].toInt();
    }
    else {
        disc = discData.toInt();
    }

    const auto bitrate    = fileRef.audioProperties()->bitrate();
    const auto sampleRate = fileRef.audioProperties()->sampleRate();
    const auto length     = fileRef.audioProperties()->lengthInMilliseconds();

    for(const auto& [tag, values] : parsedTag.map) {
        for(const auto& value : values) {
            const auto tagEntry = QString::fromStdString(tag.to8Bit(true));
            if(!baseTags.contains(tagEntry)) {
                track.addExtraTag(tagEntry, QString::fromStdString(value.to8Bit(true)));
            }
        }
    }

    track.setAlbum(album);
    track.setArtists(artists);
    track.setAlbumArtist(albumArtist);
    track.setTitle(title);
    track.setDuration(length);
    track.setDate(date);
    track.setGenres(genres);
    track.setTrackNumber(trackNum);
    track.setTrackTotal(trackTotal);
    track.setDiscNumber(disc);
    track.setDiscTotal(discTotal);
    track.setBitrate(bitrate);
    track.setSampleRate(sampleRate);
    track.setFileSize(fileInfo.size());
    track.setLyrics(lyrics);
    track.setComment(comment);

    return true;
}

QPixmap readCover(const QString& filepath)
{
    const auto readingProperties = getReadingProperties(Quality::Quality);
    auto fileRef = TagLib::FileRef(TagLib::FileName(filepath.toUtf8()), readingProperties.readAudioProperties,
                                   readingProperties.readStyle);

    return coverFromFile(fileRef);
}

bool writeMetaData(const Track& track)
{
    const auto filepath = track.filepath();
    const auto fileInfo = QFileInfo(filepath);
    if(fileInfo.size() <= 0) {
        return false;
    }

    auto fileRef = TagLib::FileRef(TagLib::FileName(filepath.toUtf8()));

    const auto album       = convertString(track.album());
    const auto artist      = convertStringList(track.artists());
    const auto albumArtist = convertString(track.albumArtist());
    const auto title       = convertString(track.title());
    const auto composer    = convertString(track.composer());
    const auto performer   = convertString(track.performer());
    const auto genre       = convertStringList(track.genres());
    const auto year        = convertString(track.year());
    const auto comment     = convertString(track.comment());

    // TODO: Add option for saving to TRACKTOTAL and DISCTOTAL tags when X/Y not standard i.e. FLAC
    auto trackNumber = convertString(track.trackNumber());
    auto disc        = convertString(track.discNumber());

    const auto trackTotal = track.trackTotal();
    const auto discTotal  = track.discTotal();

    if(trackTotal > 0) {
        trackNumber += "/";
        trackNumber.append(convertString(trackTotal));
    }

    if(discTotal > 0) {
        disc += "/";
        disc.append(convertString(discTotal));
    }

    auto parsedTag = tagsFromFile(fileRef);
    parsedTag.map  = fileRef.file()->properties();
    if(!parsedTag.tag) {
        return false;
    }

    parsedTag.map.replace("ALBUM", album);
    parsedTag.map.replace("ARTIST", artist);
    parsedTag.map.replace("ALBUMARTIST", albumArtist);
    parsedTag.map.replace("TITLE", title);
    parsedTag.map.replace("GENRE", genre);
    parsedTag.map.replace("DATE", year);
    parsedTag.map.replace("TRACKNUMBER", trackNumber);
    parsedTag.map.replace("DISCNUMBER", disc);
    parsedTag.map.replace("COMPOSER", composer);
    parsedTag.map.replace("PERFORMER", performer);
    parsedTag.map.replace("COMMENT", comment);

    fileRef.file()->setProperties(parsedTag.map);
    return fileRef.save();
}
} // namespace Core::Tagging
