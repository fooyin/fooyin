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

#include "tags.h"

#include "core/library/models/track.h"
#include "tagutils.h"

#include <QFileInfo>
#include <QPixmap>
#include <taglib/fileref.h>
#include <taglib/tfilestream.h>

namespace Tagging {
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
    };

    return readingProperties;
}

bool readMetaData(Track& track, Quality quality)
{
    const auto filepath = track.filepath();
    const auto fileInfo = QFileInfo(filepath);

    QDateTime md = fileInfo.lastModified();
    qint64 timeNow = QDateTime::currentMSecsSinceEpoch();

    track.setAddedTime(timeNow);
    track.setMTime(md.isValid() ? md.toMSecsSinceEpoch() : 0);

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
    parsedTag.map = fileRef.file()->properties();
    if(!parsedTag.tag) {
        return false;
    }

    const QStringList baseTags{"TITLE", "ARTIST",     "ALBUMARTIST", "GENRE",   "TRACKNUMBER",
                               "ALBUM", "DISCNUMBER", "DATE",        "COMMENT", "LYRICS"};

    const auto artists = convertStringList(parsedTag.map["ARTIST"]);
    const auto album = convertString(parsedTag.tag->album());
    const auto albumArtist = convertString(parsedTag.map["ALBUMARTIST"].toString());
    const auto title = convertString(parsedTag.tag->title());
    const auto genres = convertStringList(parsedTag.map["GENRE"]);
    const auto comment = convertString(parsedTag.tag->comment());
    const auto year = parsedTag.tag->year();
    const auto trackNumber = parsedTag.tag->track();
    const auto discData = convertString(parsedTag.map["DISCNUMBER"].toString());
    const auto disc = discData.contains("/") ? discData.split("/")[0].toInt() : discData.toInt();
    const auto bitrate = fileRef.audioProperties()->bitrate();
    const auto sampleRate = fileRef.audioProperties()->sampleRate();
    const auto lyrics = convertString(parsedTag.map["LYRICS"].toString());

    const auto length = fileRef.audioProperties()->lengthInMilliseconds();

    for(const auto& [tag, values] : parsedTag.map) {
        for(const auto& value : values) {
            const auto tagEntry = QString::fromStdString(tag.to8Bit(true));
            if(!baseTags.contains(tagEntry)) {
                track.addExtra(tagEntry, QString::fromStdString(value.to8Bit(true)));
            }
        }
    }

    track.setAlbum(album);
    track.setArtists(artists);
    track.setAlbumArtist(albumArtist);
    track.setTitle(title);
    track.setDuration(length);
    track.setYear(year);
    track.setGenres(genres);
    track.setTrackNumber(trackNumber);
    track.setDiscNumber(disc);
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
} // namespace Tagging
