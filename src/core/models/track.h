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

#pragma once

#include "core/typedefs.h"
#include "musicitem.h"

#include <QList>

#include <map>

namespace Core {
using GenreList  = QList<QString>;
using ArtistList = QList<QString>;
using ExtraTags  = std::map<QString, QList<QString>>;

class Track : public MusicItem
{
public:
    Track() = default;
    explicit Track(QString filepath);

    [[nodiscard]] bool isEnabled() const;

    [[nodiscard]] int libraryId() const;

    [[nodiscard]] int id() const;
    [[nodiscard]] QString uid() const;
    [[nodiscard]] QString filepath() const;
    [[nodiscard]] QString title() const;
    [[nodiscard]] ArtistList artists() const;
    [[nodiscard]] IdSet artistIds() const;
    [[nodiscard]] int albumId() const;
    [[nodiscard]] QString album() const;
    [[nodiscard]] QString albumArtist() const;
    [[nodiscard]] int albumArtistId() const;
    [[nodiscard]] int trackNumber() const;
    [[nodiscard]] int trackTotal() const;
    [[nodiscard]] int discNumber() const;
    [[nodiscard]] int discTotal() const;
    [[nodiscard]] IdSet genreIds() const;
    [[nodiscard]] GenreList genres() const;
    [[nodiscard]] QString composer() const;
    [[nodiscard]] QString performer() const;
    [[nodiscard]] quint64 duration() const;
    [[nodiscard]] QString lyrics() const;
    [[nodiscard]] QString comment() const;
    [[nodiscard]] QString date() const;
    [[nodiscard]] int year() const;
    [[nodiscard]] QString coverPath() const;
    [[nodiscard]] bool hasCover() const;
    [[nodiscard]] bool isSingleDiscAlbum() const;

    [[nodiscard]] ExtraTags extraTags() const;
    [[nodiscard]] QByteArray extraTagsToJson() const;

    [[nodiscard]] uint64_t fileSize() const;
    [[nodiscard]] int bitrate() const;
    [[nodiscard]] int sampleRate() const;

    [[nodiscard]] int playCount() const;

    [[nodiscard]] uint64_t addedTime() const;
    [[nodiscard]] uint64_t modifiedTime() const;

    void setIsEnabled(bool enabled);

    void setLibraryId(int id);

    void setId(int id);
    void setTitle(const QString& title);
    void setArtists(const ArtistList& artists);
    void setArtistIds(const IdSet& ids);
    void addArtistId(int id);
    void setAlbumId(int id);
    void setAlbum(const QString& title);
    void setAlbumArtist(const QString& artist);
    void setAlbumArtistId(int id);
    void setTrackNumber(int num);
    void setTrackTotal(int num);
    void setDiscNumber(int num);
    void setDiscTotal(int num);
    void setGenreIds(const IdSet& ids);
    void addGenreId(int id);
    void setGenres(const GenreList& genres);
    void setComposer(const QString& composer);
    void setPerformer(const QString& performer);
    void setDuration(quint64 duration);
    void setLyrics(const QString& lyrics);
    void setComment(const QString& comment);
    void setDate(const QString& date);
    void setCoverPath(const QString& path);

    void addExtraTag(const QString& tag, const QString& value);
    void jsonToExtraTags(const QByteArray& json);

    void setFileSize(uint64_t fileSize);
    void setBitrate(int rate);
    void setSampleRate(int rate);

    void setPlayCount(int count);

    void setAddedTime(uint64_t time);
    void setModifiedTime(uint64_t time);

    void resetIds();

private:
    bool m_enabled;

    int m_libraryId;

    int m_id;
    QString m_filepath;
    QString m_title;
    ArtistList m_artists;
    IdSet m_artistIds;
    int m_albumId;
    QString m_album;
    QString m_albumArtist;
    int m_albumArtistId;
    int m_trackNumber;
    int m_trackTotal;
    int m_discNumber;
    int m_discTotal;
    IdSet m_genreIds;
    GenreList m_genres;
    QString m_composer;
    QString m_performer;
    uint64_t m_duration;
    QString m_lyrics;
    QString m_comment;
    QString m_date;
    int m_year;
    QString m_coverPath;
    ExtraTags m_extraTags;

    uint64_t m_filesize;
    int m_bitrate;
    int m_sampleRate;

    int m_playcount;

    uint64_t m_addedTime;
    uint64_t m_modifiedTime;
};
} // namespace Core
