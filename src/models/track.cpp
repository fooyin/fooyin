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

#include "track.h"

#include "utils/helpers.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <utility>

Track::Track(QString filepath)
    : LibraryItem()
    , m_enabled(true)
    , m_id(-1)
    , m_filepath(std::move(filepath))
    , m_albumId()
    , m_albumArtistId()
    , m_trackNumber(0)
    , m_discNumber(0)
    , m_duration(0)
    , m_bitrate(0)
    , m_sampleRate(0)
    , m_playcount(0)
    , m_addedTime(0)
    , m_mTime(0)
    , m_filesize(0)
    , m_libraryId(-1)
{ }

bool Track::isEnabled() const
{
    return m_enabled;
}

void Track::setIsEnabled(bool enabled)
{
    m_enabled = enabled;
}

Track::~Track() = default;

int Track::id() const
{
    return m_id;
}

void Track::setId(int id)
{
    m_id = id;
}

QString Track::filepath() const
{
    return m_filepath;
}

QString Track::title() const
{
    return m_title;
}

void Track::setTitle(const QString& title)
{
    m_title = title;
}

QStringList Track::artists() const
{
    return m_artists;
}

void Track::setArtists(const QStringList& artists)
{
    m_artists = artists;
}

IdSet Track::artistIds() const
{
    return m_artistIds;
}

void Track::setArtistIds(const IdSet& ids)
{
    m_artistIds = ids;
}

void Track::addArtistId(int id)
{
    m_artistIds.insert(id);
}

int Track::albumId() const
{
    return m_albumId;
}

void Track::setAlbumId(int id)
{
    m_albumId = id;
}

QString Track::album() const
{
    return m_album;
}

void Track::setAlbum(const QString& title)
{
    m_album = title;
}

QString Track::albumArtist() const
{
    return m_albumArtist;
}

void Track::setAlbumArtist(const QString& artist)
{
    m_albumArtist = artist;
}

int Track::albumArtistId() const
{
    return m_albumArtistId;
}

void Track::setAlbumArtistId(int id)
{
    m_albumArtistId = id;
}

quint16 Track::trackNumber() const
{
    return m_trackNumber;
}

void Track::setTrackNumber(quint16 num)
{
    m_trackNumber = num;
}

quint8 Track::discNumber() const
{
    return m_discNumber;
}

void Track::setDiscNumber(quint8 num)
{
    m_discNumber = num;
}

IdSet Track::genreIds() const
{
    return m_genreIds;
}

void Track::setGenreIds(const IdSet& ids)
{
    m_genreIds = ids;
}

void Track::addGenreId(int id)
{
    m_genreIds.insert(id);
}

QStringList Track::genres() const
{
    return m_genres;
}

void Track::setGenres(const QStringList& genre)
{
    m_genres = genre;
}

quint64 Track::duration() const
{
    return m_duration;
}

void Track::setDuration(quint64 duration)
{
    m_duration = duration;
}

QString Track::lyrics() const
{
    return m_lyrics;
}

void Track::setLyrics(const QString& lyrics)
{
    m_lyrics = lyrics;
}

QString Track::comment() const
{
    return m_comment;
}

void Track::setComment(const QString& comment)
{
    m_comment = comment;
}

quint16 Track::year() const
{
    return m_year;
}

void Track::setYear(quint16 year)
{
    m_year = year;
}

QString Track::coverPath() const
{
    return m_coverPath;
}

void Track::setCoverPath(const QString& path)
{
    m_coverPath = path;
}

quint16 Track::bitrate() const
{
    return m_bitrate;
}

void Track::setBitrate(quint16 rate)
{
    m_bitrate = rate;
}

quint16 Track::sampleRate() const
{
    return m_sampleRate;
}

void Track::setSampleRate(quint16 rate)
{
    m_sampleRate = rate;
}
quint16 Track::playCount() const
{
    return m_playcount;
}

void Track::setPlayCount(quint16 count)
{
    m_playcount = count;
}

qint64 Track::addedTime() const
{
    return m_addedTime;
}

void Track::setAddedTime(qint64 time)
{
    m_addedTime = time;
}

qint64 Track::mTime() const
{
    return m_mTime;
}

void Track::setMTime(qint64 time)
{
    m_mTime = time;
}

bool Track::isSingleDiscAlbum() const
{
    return !m_coverPath.isEmpty();
}

quint64 Track::fileSize() const
{
    return m_filesize;
}

void Track::setFileSize(quint64 fileSize)
{
    m_filesize = fileSize;
}

int Track::libraryId() const
{
    return m_libraryId;
}

void Track::setLibraryId(int id)
{
    m_libraryId = id;
}

ExtraTags Track::extra() const
{
    return m_extraTags;
}

void Track::addExtra(const QString& tag, const QString& value)
{
    if(!tag.isEmpty() && !value.isEmpty())
    {
        if(m_extraTags.contains(tag))
        {
            auto entry = m_extraTags.value(tag);
            entry.append(value);
            m_extraTags.insert(tag, entry);
        }
    }
    m_extraTags.insert(tag, {value});
}

QByteArray Track::extraTagsToJson() const
{
    QJsonObject extra;
    QJsonArray extraArray;
    for(const auto& [tag, values] : asRange(m_extraTags))
    {
        QJsonObject tagObject;
        const auto tagArray = QJsonArray::fromStringList(values);
        tagObject[tag] = tagArray;
        extraArray.append(tagObject);
    }
    extra["tags"] = extraArray;

    QByteArray json = QJsonDocument(extra).toJson(QJsonDocument::Compact);
    return json;
}

void Track::jsonToExtraTags(const QByteArray& ba)
{
    QJsonDocument jsonDoc = QJsonDocument::fromJson(ba);

    if(!jsonDoc.isNull())
    {
        QJsonObject json = jsonDoc.object();

        if(json.contains("tags") && json["tags"].isArray())
        {
            QJsonArray extraArray = json["tags"].toArray();

            for(QJsonArray::const_iterator i = extraArray.constBegin(); i != extraArray.constEnd(); i++)
            {
                QJsonObject tagObject = i->toObject();
                for(QJsonObject::const_iterator j = tagObject.constBegin(); j != tagObject.constEnd(); j++)
                {
                    QString tag = j.key();
                    if(tagObject[tag].isArray())
                    {
                        QJsonArray tagArray = j.value().toArray();
                        QList<QString> values;
                        for(const auto& value : tagArray)
                        {
                            values.append(value.toString());
                        }
                        m_extraTags.insert(tag, values);
                    }
                }
            }
        }
    }
}
