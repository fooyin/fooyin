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

#include "track.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace Fy::Core {
Track::Track(QString filepath)
    : MusicItem()
    , m_enabled{true}
    , m_libraryId{-1}
    , m_id{-1}
    , m_filepath{std::move(filepath)}
    , m_albumId{-1}
    , m_albumArtistId{-1}
    , m_trackNumber{0}
    , m_trackTotal{0}
    , m_discNumber{0}
    , m_discTotal{0}
    , m_duration{0}
    , m_filesize{0}
    , m_bitrate{0}
    , m_sampleRate{0}
    , m_playcount{0}
    , m_modifiedTime{0}
{ }

bool Track::isEnabled() const
{
    return m_enabled;
}

int Track::libraryId() const
{
    return m_libraryId;
}

int Track::id() const
{
    return m_id;
}

QString Track::uid() const
{
    return {album() + QString::number(id()) + title()};
}

QString Track::filepath() const
{
    return m_filepath;
}

QString Track::title() const
{
    return m_title;
}

ArtistList Track::artists() const
{
    return m_artists;
}

IdSet Track::artistIds() const
{
    return m_artistIds;
}

int Track::albumId() const
{
    return m_albumId;
}

QString Track::album() const
{
    return m_album;
}

QString Track::albumArtist() const
{
    return m_albumArtist;
}

int Track::albumArtistId() const
{
    return m_albumArtistId;
}

int Track::trackNumber() const
{
    return m_trackNumber;
}

int Track::trackTotal() const
{
    return m_trackTotal;
}

int Track::discNumber() const
{
    return m_discNumber;
}

int Track::discTotal() const
{
    return m_discTotal;
}

IdSet Track::genreIds() const
{
    return m_genreIds;
}

GenreList Track::genres() const
{
    return m_genres;
}

QString Track::composer() const
{
    return m_composer;
}

QString Track::performer() const
{
    return m_performer;
}

uint64_t Track::duration() const
{
    return m_duration;
}

QString Track::lyrics() const
{
    return m_lyrics;
}

QString Track::comment() const
{
    return m_comment;
}

QString Track::date() const
{
    return m_date;
}

int Track::year() const
{
    return m_year;
}

QString Track::coverPath() const
{
    return m_coverPath;
}

bool Track::hasCover() const
{
    return !m_coverPath.isEmpty();
}

bool Track::isSingleDiscAlbum() const
{
    return m_discTotal <= 1;
}

ExtraTags Track::extraTags() const
{
    return m_extraTags;
}

QByteArray Track::extraTagsToJson() const
{
    QJsonObject extra;
    QJsonArray extraArray;
    for(const auto& [tag, values] : m_extraTags) {
        QJsonObject tagObject;
        const auto tagArray = QJsonArray::fromStringList(values);
        tagObject[tag]      = tagArray;
        extraArray.append(tagObject);
    }
    extra["tags"] = extraArray;

    QByteArray json = QJsonDocument(extra).toJson(QJsonDocument::Compact);
    return json;
}

uint64_t Track::fileSize() const
{
    return m_filesize;
}

int Track::bitrate() const
{
    return m_bitrate;
}

int Track::sampleRate() const
{
    return m_sampleRate;
}

int Track::playCount() const
{
    return m_playcount;
}

uint64_t Track::addedTime() const
{
    return m_addedTime;
}

uint64_t Track::modifiedTime() const
{
    return m_modifiedTime;
}

void Track::setIsEnabled(bool enabled)
{
    m_enabled = enabled;
}

void Track::setLibraryId(int id)
{
    m_libraryId = id;
}

void Track::setId(int id)
{
    m_id = id;
}

void Track::setTitle(const QString& title)
{
    m_title = title;
}

void Track::setArtists(const ArtistList& artists)
{
    m_artists = artists;
}

void Track::setArtistIds(const IdSet& ids)
{
    m_artistIds = ids;
}

void Track::addArtistId(int id)
{
    m_artistIds.emplace(id);
}

void Track::setAlbumId(int id)
{
    m_albumId = id;
}

void Track::setAlbum(const QString& title)
{
    m_album = title;
}

void Track::setAlbumArtist(const QString& artist)
{
    m_albumArtist = artist;
}

void Track::setAlbumArtistId(int id)
{
    m_albumArtistId = id;
}

void Track::setTrackNumber(int number)
{
    m_trackNumber = number;
}

void Track::setTrackTotal(int total)
{
    m_trackTotal = total;
}

void Track::setDiscNumber(int number)
{
    m_discNumber = number;
}

void Track::setDiscTotal(int total)
{
    m_discTotal = total;
}

void Track::setGenreIds(const IdSet& ids)
{
    m_genreIds = ids;
}

void Track::addGenreId(int id)
{
    m_genreIds.emplace(id);
}

void Track::setGenres(const GenreList& genres)
{
    m_genres = genres;
}

void Track::setComposer(const QString& composer)
{
    m_composer = composer;
}

void Track::setPerformer(const QString& performer)
{
    m_performer = performer;
}

void Track::setDuration(uint64_t duration)
{
    m_duration = duration;
}

void Track::setLyrics(const QString& lyrics)
{
    m_lyrics = lyrics;
}

void Track::setComment(const QString& comment)
{
    m_comment = comment;
}

void Track::setDate(const QString& date)
{
    m_date = date;
    m_year = date.toInt();
}

void Track::setCoverPath(const QString& path)
{
    m_coverPath = path;
}

void Track::addExtraTag(const QString& tag, const QString& value)
{
    if(!tag.isEmpty() && !value.isEmpty()) {
        if(m_extraTags.count(tag)) {
            auto entry = m_extraTags.at(tag);
            entry.append(value);
            m_extraTags.emplace(tag, entry);
        }
    }
    m_extraTags.emplace(tag, value);
}

void Track::jsonToExtraTags(const QByteArray& json)
{
    const QJsonDocument jsonDoc = QJsonDocument::fromJson(json);

    if(!jsonDoc.isNull()) {
        QJsonObject json = jsonDoc.object();

        if(json.contains("tags") && json["tags"].isArray()) {
            const QJsonArray extraArray = json["tags"].toArray();

            for(auto i = extraArray.constBegin(); i != extraArray.constEnd(); ++i) {
                QJsonObject tagObject = i->toObject();
                for(QJsonObject::const_iterator j = tagObject.constBegin(); j != tagObject.constEnd(); ++j) {
                    const QString tag = j.key();
                    if(tagObject[tag].isArray()) {
                        const QJsonArray tagArray = j.value().toArray();
                        QList<QString> values;
                        for(const auto& value : tagArray) {
                            values.append(value.toString());
                        }
                        m_extraTags.emplace(tag, values);
                    }
                }
            }
        }
    }
}

void Track::setFileSize(uint64_t fileSize)
{
    m_filesize = fileSize;
}

void Track::setBitrate(int rate)
{
    m_bitrate = rate;
}

void Track::setSampleRate(int rate)
{
    m_sampleRate = rate;
}

void Track::setPlayCount(int count)
{
    m_playcount = count;
}

void Track::setAddedTime(uint64_t time)
{
    m_addedTime = time;
}

void Track::setModifiedTime(uint64_t time)
{
    m_modifiedTime = time;
}

void Track::resetIds()
{
    m_artistIds.clear();
    m_genreIds.clear();
}
} // namespace Fy::Core
