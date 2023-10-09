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

#include "fycore_export.h"

#include <QList>
#include <QSharedDataPointer>

#include <map>

namespace Fy::Core {
using ExtraTags = QMap<QString, QList<QString>>;

class FYCORE_EXPORT Track
{
public:
    struct TrackHash
    {
        size_t operator()(const Track& track) const
        {
            return qHash(track.filepath());
        }
    };

    Track();
    explicit Track(QString filepath);
    ~Track();

    Track(const Track& other);
    Track& operator=(const Track& other);
    bool operator==(const Track& other) const;
    bool operator!=(const Track& other) const;

    QString generateHash();

    [[nodiscard]] bool isValid() const;
    [[nodiscard]] bool enabled() const;
    [[nodiscard]] int libraryId() const;

    [[nodiscard]] int id() const;
    [[nodiscard]] QString hash() const;
    [[nodiscard]] QString albumHash() const;
    [[nodiscard]] QString filepath() const;
    [[nodiscard]] QString relativePath() const;
    [[nodiscard]] QString title() const;
    [[nodiscard]] QStringList artists() const;
    [[nodiscard]] QString artist() const;
    [[nodiscard]] QString album() const;
    [[nodiscard]] QString albumArtist() const;
    [[nodiscard]] int trackNumber() const;
    [[nodiscard]] int trackTotal() const;
    [[nodiscard]] int discNumber() const;
    [[nodiscard]] int discTotal() const;
    [[nodiscard]] QStringList genres() const;
    [[nodiscard]] QString genre() const;
    [[nodiscard]] QString composer() const;
    [[nodiscard]] QString performer() const;
    [[nodiscard]] uint64_t duration() const;
    [[nodiscard]] QString lyrics() const;
    [[nodiscard]] QString comment() const;
    [[nodiscard]] QString date() const;
    [[nodiscard]] int year() const;
    [[nodiscard]] QString coverPath() const;
    [[nodiscard]] bool hasCover() const;
    [[nodiscard]] bool hasEmbeddedCover() const;
    [[nodiscard]] QString thumbnailPath() const;
    [[nodiscard]] bool isSingleDiscAlbum() const;

    [[nodiscard]] ExtraTags extraTags() const;
    [[nodiscard]] QByteArray serialiseExtrasTags() const;

    [[nodiscard]] uint64_t fileSize() const;
    [[nodiscard]] int bitrate() const;
    [[nodiscard]] int sampleRate() const;

    [[nodiscard]] int playCount() const;

    [[nodiscard]] uint64_t addedTime() const;
    [[nodiscard]] uint64_t modifiedTime() const;

    [[nodiscard]] QString sort() const;

    void setLibraryId(int id);
    void setEnabled(bool enabled);
    void setId(int id);
    void setHash(const QString& hash);
    void setRelativePath(const QString& path);
    void setTitle(const QString& title);
    void setArtists(const QStringList& artists);
    void setAlbum(const QString& title);
    void setAlbumArtist(const QString& artist);
    void setTrackNumber(int num);
    void setTrackTotal(int num);
    void setDiscNumber(int num);
    void setDiscTotal(int num);
    void setGenres(const QStringList& genres);
    void setComposer(const QString& composer);
    void setPerformer(const QString& performer);
    void setDuration(uint64_t duration);
    void setLyrics(const QString& lyrics);
    void setComment(const QString& comment);
    void setDate(const QString& date);
    void setYear(int year);
    void setCoverPath(const QString& path);

    void addExtraTag(const QString& tag, const QString& value);
    void storeExtraTags(const QByteArray& json);

    void setFileSize(uint64_t fileSize);
    void setBitrate(int rate);
    void setSampleRate(int rate);

    void setPlayCount(int count);

    void setAddedTime(uint64_t time);
    void setModifiedTime(uint64_t time);

    void setSort(const QString& sort);

private:
    struct Private;
    QSharedDataPointer<Private> p;
};

size_t qHash(const Track& track);

using TrackList    = std::vector<Track>;
using TrackIdMap   = std::unordered_map<int, Track>;
using TrackPathMap = std::unordered_map<QString, Track>;
} // namespace Fy::Core
