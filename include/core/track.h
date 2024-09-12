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
class Track;
class TrackPrivate;

using TrackList = std::vector<Track>;
using TrackIds  = std::vector<int>;

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
            return qHash(track.uniqueFilepath());
        }
    };

    enum class Cover : uint8_t
    {
        Front = 0,
        Back,
        Artist,
    };

    using ExtraTags       = QMap<QString, QStringList>;
    using ExtraProperties = QMap<QString, QString>;

    Track();
    explicit Track(const QString& filepath);
    Track(const QString& filepath, int subsong);
    ~Track();

    Track(const Track& other);
    Track& operator=(const Track& other);
    bool operator==(const Track& other) const;
    bool operator!=(const Track& other) const;
    bool operator<(const Track& other) const;

    QString generateHash();

    [[nodiscard]] bool isValid() const;
    [[nodiscard]] bool isEnabled() const;
    [[nodiscard]] bool isInLibrary() const;
    [[nodiscard]] bool isInDatabase() const;
    [[nodiscard]] bool metadataWasRead() const;
    [[nodiscard]] bool metadataWasModified() const;
    [[nodiscard]] bool exists() const;
    [[nodiscard]] bool isNewTrack() const;
    [[nodiscard]] int libraryId() const;

    [[nodiscard]] bool isInArchive() const;
    [[nodiscard]] QString archivePath() const;
    [[nodiscard]] QString pathInArchive() const;
    [[nodiscard]] QString relativeArchivePath() const;

    [[nodiscard]] int id() const;
    [[nodiscard]] QString hash() const;
    [[nodiscard]] QString albumHash() const;
    [[nodiscard]] QString filepath() const;
    [[nodiscard]] QString uniqueFilepath() const;
    [[nodiscard]] QString prettyFilepath() const;
    [[nodiscard]] QString filename() const;
    [[nodiscard]] QString path() const;
    [[nodiscard]] QString directory() const;
    [[nodiscard]] QString extension() const;
    [[nodiscard]] QString filenameExt() const;
    [[nodiscard]] QString title() const;
    [[nodiscard]] QString effectiveTitle() const;
    [[nodiscard]] QStringList artists() const;
    [[nodiscard]] QStringList uniqueArtists() const;
    [[nodiscard]] QString artist() const;
    [[nodiscard]] QString primaryArtist() const;
    [[nodiscard]] QString uniqueArtist() const;
    [[nodiscard]] QString album() const;
    [[nodiscard]] QStringList albumArtists() const;
    [[nodiscard]] QString albumArtist() const;
    [[nodiscard]] QString primaryAlbumArtist() const;
    [[nodiscard]] QString trackNumber() const;
    [[nodiscard]] QString trackTotal() const;
    [[nodiscard]] QString discNumber() const;
    [[nodiscard]] QString discTotal() const;
    [[nodiscard]] QStringList genres() const;
    [[nodiscard]] QString genre() const;
    [[nodiscard]] QString composer() const;
    [[nodiscard]] QString performer() const;
    [[nodiscard]] QString comment() const;
    [[nodiscard]] QString date() const;
    [[nodiscard]] int year() const;
    [[nodiscard]] float rating() const;
    [[nodiscard]] int ratingStars() const;

    [[nodiscard]] bool hasTrackGain() const;
    [[nodiscard]] bool hasAlbumGain() const;
    [[nodiscard]] bool hasTrackPeak() const;
    [[nodiscard]] bool hasAlbumPeak() const;
    [[nodiscard]] float rgTrackGain() const;
    [[nodiscard]] float rgAlbumGain() const;
    [[nodiscard]] float rgTrackPeak() const;
    [[nodiscard]] float rgAlbumPeak() const;

    [[nodiscard]] bool hasCue() const;
    [[nodiscard]] QString cuePath() const;

    [[nodiscard]] bool hasExtraTag(const QString& tag) const;
    [[nodiscard]] QStringList extraTag(const QString& tag) const;
    [[nodiscard]] ExtraTags extraTags() const;
    [[nodiscard]] QStringList removedTags() const;
    [[nodiscard]] QByteArray serialiseExtraTags() const;

    [[nodiscard]] bool hasExtraProperty(const QString& prop) const;
    [[nodiscard]] ExtraProperties extraProperties() const;
    [[nodiscard]] QByteArray serialiseExtraProperties() const;

    [[nodiscard]] int subsong() const;
    [[nodiscard]] uint64_t offset() const;
    [[nodiscard]] uint64_t duration() const;
    [[nodiscard]] uint64_t fileSize() const;
    [[nodiscard]] int bitrate() const;
    [[nodiscard]] int sampleRate() const;
    [[nodiscard]] int channels() const;
    [[nodiscard]] int bitDepth() const;
    [[nodiscard]] QString codec() const;

    [[nodiscard]] int playCount() const;

    [[nodiscard]] uint64_t addedTime() const;
    [[nodiscard]] uint64_t modifiedTime() const;
    [[nodiscard]] uint64_t lastModified() const;
    [[nodiscard]] uint64_t firstPlayed() const;
    [[nodiscard]] uint64_t lastPlayed() const;

    [[nodiscard]] QString sort() const;

    void setLibraryId(int id);
    void setIsEnabled(bool enabled);
    void setId(int id);
    void setHash(const QString& hash);
    void setCodec(const QString& codec);
    void setFilePath(const QString& path);
    void setTitle(const QString& title);
    void setArtists(const QStringList& artists);
    void setAlbum(const QString& title);
    void setAlbumArtists(const QStringList& artists);
    void setTrackNumber(const QString& num);
    void setTrackTotal(const QString& total);
    void setDiscNumber(const QString& num);
    void setDiscTotal(const QString& total);
    void setGenres(const QStringList& genres);
    void setComposer(const QString& composer);
    void setPerformer(const QString& performer);
    void setComment(const QString& comment);
    void setDate(const QString& date);
    void setYear(int year);
    void setRating(float rating);
    void setRatingStars(int rating);

    void setRGTrackGain(float gain);
    void setRGAlbumGain(float gain);
    void setRGTrackPeak(float peak);
    void setRGAlbumPeak(float peak);

    [[nodiscard]] QString metaValue(const QString& name) const;
    [[nodiscard]] QString techInfo(const QString& name) const;

    void setCuePath(const QString& path);

    void addExtraTag(const QString& tag, const QString& value);
    void removeExtraTag(const QString& tag);
    void replaceExtraTag(const QString& tag, const QString& value);
    void clearExtraTags();
    void storeExtraTags(const QByteArray& tags);

    void setExtraProperty(const QString& prop, const QString& value);
    void removeExtraProperty(const QString& prop);
    void clearExtraProperties();
    void storeExtraProperties(const QByteArray& props);

    void setSubsong(int index);
    void setOffset(uint64_t offset);
    void setDuration(uint64_t duration);
    void setFileSize(uint64_t fileSize);
    void setBitrate(int rate);
    void setSampleRate(int rate);
    void setChannels(int channels);
    void setBitDepth(int depth);

    void setPlayCount(int count);

    void setAddedTime(uint64_t time);
    void setModifiedTime(uint64_t time);
    void setFirstPlayed(uint64_t time);
    void setLastPlayed(uint64_t time);

    void setSort(const QString& sort);
    void clearWasModified();

    static QString findCommonField(const TrackList& tracks);

    static QStringList supportedMimeTypes();

private:
    QSharedDataPointer<TrackPrivate> p;
};
FYCORE_EXPORT size_t qHash(const Track& track);
} // namespace Fooyin
