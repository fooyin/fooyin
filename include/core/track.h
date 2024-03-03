/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
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

namespace Fooyin {
using ExtraTags = QMap<QString, QStringList>;

/*!
 * Represents a music track and it's associated metadata.
 * Metadata which is not explicitly handled is accessed using Track::extraTags.
 * @note this class is implicitly shared.
 */
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

    enum class Type : int
    {
        Unknown   = 0,
        MPEG      = 1,
        AIFF      = 2,
        WAV       = 3,
        MPC       = 4,
        APE       = 5,
        WavPack   = 6,
        MP4       = 7,
        FLAC      = 8,
        OggOpus   = 9,
        OggVorbis = 10,
        ASF       = 11,
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
    [[nodiscard]] bool isEnabled() const;
    [[nodiscard]] bool isInLibrary() const;
    [[nodiscard]] bool isInDatabase() const;
    [[nodiscard]] int libraryId() const;

    [[nodiscard]] int id() const;
    [[nodiscard]] QString hash() const;
    [[nodiscard]] QString albumHash() const;
    [[nodiscard]] Type type() const;
    [[nodiscard]] QString typeString() const;
    [[nodiscard]] QString filepath() const;
    [[nodiscard]] QString relativePath() const;
    [[nodiscard]] QString filename() const;
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
    [[nodiscard]] QString comment() const;
    [[nodiscard]] QString date() const;
    [[nodiscard]] int year() const;
    [[nodiscard]] QString coverPath() const;
    [[nodiscard]] bool hasCover() const;
    [[nodiscard]] bool hasEmbeddedCover() const;

    [[nodiscard]] QStringList extraTag(const QString& tag) const;
    [[nodiscard]] ExtraTags extraTags() const;
    [[nodiscard]] QStringList removedTags() const;
    [[nodiscard]] QByteArray serialiseExtrasTags() const;

    [[nodiscard]] uint64_t fileSize() const;
    [[nodiscard]] int bitrate() const;
    [[nodiscard]] int sampleRate() const;

    [[nodiscard]] int playCount() const;

    [[nodiscard]] uint64_t addedTime() const;
    [[nodiscard]] uint64_t modifiedTime() const;
    [[nodiscard]] uint64_t firstPlayed() const;
    [[nodiscard]] uint64_t lastPlayed() const;

    [[nodiscard]] QString sort() const;

    void setLibraryId(int id);
    void setIsEnabled(bool enabled);
    void setId(int id);
    void setHash(const QString& hash);
    void setType(Type type);
    void setFilePath(const QString& path);
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
    void setComment(const QString& comment);
    void setDate(const QString& date);
    void setYear(int year);
    void setCoverPath(const QString& path);

    void addExtraTag(const QString& tag, const QString& value);
    void removeExtraTag(const QString& tag);
    void replaceExtraTag(const QString& tag, const QString& value);
    void clearExtraTags();
    void storeExtraTags(const QByteArray& json);

    void setFileSize(uint64_t fileSize);
    void setBitrate(int rate);
    void setSampleRate(int rate);

    void setPlayCount(int count);

    void setAddedTime(uint64_t time);
    void setModifiedTime(uint64_t time);
    void setFirstPlayed(uint64_t time);
    void setLastPlayed(uint64_t time);

    void setSort(const QString& sort);

    static QStringList supportedFileExtensions();

private:
    struct Private;
    QSharedDataPointer<Private> p;
};
FYCORE_EXPORT size_t qHash(const Track& track);

using TrackIds      = std::vector<int>;
using TrackList     = std::vector<Track>;
using TrackIdMap    = std::unordered_map<int, Track>;
using TrackFieldMap = std::unordered_map<QString, Track>;
} // namespace Fooyin

FYCORE_EXPORT QDataStream& operator<<(QDataStream& stream, const Fooyin::TrackIds& tracks);
FYCORE_EXPORT QDataStream& operator>>(QDataStream& stream, Fooyin::TrackIds& tracks);
