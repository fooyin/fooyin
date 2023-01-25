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
#include "libraryitem.h"

#include <QDateTime>
#include <QSet>

namespace Core {
using ExtraTags = QMap<QString, QList<QString>>;

class Track : public LibraryItem
{
public:
    Track() = default;
    explicit Track(QString filepath);
    ~Track() override;

    [[nodiscard]] bool isEnabled() const;
    void setIsEnabled(bool enabled);

    [[nodiscard]] int id() const;
    void setId(int id);

    [[nodiscard]] QString filepath() const;

    [[nodiscard]] QString title() const;
    void setTitle(const QString& title);

    [[nodiscard]] QStringList artists() const;
    void setArtists(const QStringList& artist);

    [[nodiscard]] IdSet artistIds() const;
    void setArtistIds(const IdSet& ids);
    void addArtistId(int id);

    [[nodiscard]] int albumId() const;
    void setAlbumId(int id);

    [[nodiscard]] QString album() const;
    void setAlbum(const QString& title);

    [[nodiscard]] QString albumArtist() const;
    void setAlbumArtist(const QString& artist);

    [[nodiscard]] int albumArtistId() const;
    void setAlbumArtistId(int id);

    [[nodiscard]] quint16 trackNumber() const;
    void setTrackNumber(quint16 num);

    [[nodiscard]] quint16 trackTotal() const;
    void setTrackTotal(quint16 num);

    [[nodiscard]] quint8 discNumber() const;
    void setDiscNumber(quint8 num);

    [[nodiscard]] quint8 discTotal() const;
    void setDiscTotal(quint8 num);

    [[nodiscard]] IdSet genreIds() const;
    void setGenreIds(const IdSet& ids);
    void addGenreId(int id);

    [[nodiscard]] QStringList genres() const;
    void setGenres(const QStringList& genre);

    [[nodiscard]] QString composer() const;
    void setComposer(const QString& composer);

    [[nodiscard]] QString performer() const;
    void setPerformer(const QString& performer);

    [[nodiscard]] quint64 duration() const;
    void setDuration(quint64 duration);

    [[nodiscard]] QString lyrics() const;
    void setLyrics(const QString& lyrics);

    [[nodiscard]] QString comment() const;
    void setComment(const QString& comment);

    [[nodiscard]] quint16 year() const;
    void setYear(quint16 year);

    [[nodiscard]] QString coverPath() const;
    void setCoverPath(const QString& path);

    [[nodiscard]] bool hasCover() const;

    [[nodiscard]] quint16 bitrate() const;
    void setBitrate(quint16 rate);

    [[nodiscard]] quint16 sampleRate() const;
    void setSampleRate(quint16 rate);

    [[nodiscard]] quint16 playCount() const;
    void setPlayCount(quint16 count);

    [[nodiscard]] qint64 addedTime() const;
    void setAddedTime(qint64 time);

    [[nodiscard]] qint64 mTime() const;
    void setMTime(qint64 time);

    [[nodiscard]] bool isSingleDiscAlbum() const;

    [[nodiscard]] quint64 fileSize() const;
    void setFileSize(quint64 fileSize);

    [[nodiscard]] int libraryId() const;
    void setLibraryId(int id);

    [[nodiscard]] ExtraTags extra() const;

    void addExtra(const QString& tag, const QString& value);

    [[nodiscard]] QByteArray extraTagsToJson() const;
    void jsonToExtraTags(const QByteArray& ba);

    void resetIds();

private:
    bool m_enabled;
    int m_id;
    QString m_filepath;
    QString m_title;
    QStringList m_artists;
    IdSet m_artistIds;
    int m_albumId;
    QString m_album;
    QString m_albumArtist;
    int m_albumArtistId;
    quint16 m_trackNumber;
    quint16 m_trackTotal;
    quint8 m_discNumber;
    quint8 m_discTotal;
    IdSet m_genreIds;
    QStringList m_genres;
    QString m_composer;
    QString m_performer;
    quint64 m_duration;
    QString m_lyrics;
    QString m_comment;
    quint16 m_year;
    QString m_coverPath;
    quint64 m_bitrate;
    quint16 m_sampleRate;
    quint16 m_playcount;
    qint64 m_addedTime;
    qint64 m_mTime;
    quint64 m_filesize;
    int m_libraryId;
    ExtraTags m_extraTags;
};
} // namespace Core
