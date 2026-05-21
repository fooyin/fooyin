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

#include <utils/containers.h>
#include <utils/datastream.h>

#include <QByteArray>
#include <QMap>
#include <QMetaType>
#include <QSharedDataPointer>
#include <QString>

#include <map>
#include <memory>

namespace Fooyin {
class Track;
class TrackPrivate;
class TrackMetadataStore;

enum class OpusRGWriteMode : uint8_t
{
    LeaveNull = 0,
    Track,
    Album,
};

using TrackList = std::vector<Track>;
using TrackIds  = std::vector<int>;

/*!
 * Represents a music track and its associated metadata.
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
        Other
    };

    enum class SegmentType : uint8_t
    {
        None = 0,
        Cue,
        Chapter
    };

    using ExtraTags       = FlatStringMap<QStringList>;
    using ExtraProperties = FlatStringMap<QString>;

    Track();
    explicit Track(std::shared_ptr<TrackMetadataStore> store);
    explicit Track(const QString& filepath);
    Track(const QString& filepath, std::shared_ptr<TrackMetadataStore> store);
    Track(const QString& filepath, int subsong);
    Track(const QString& filepath, int subsong, std::shared_ptr<TrackMetadataStore> store);
    ~Track();

    Track(const Track& other);
    Track& operator=(const Track& other);
    bool operator==(const Track& other) const;
    bool operator!=(const Track& other) const;
    bool operator<(const Track& other) const;
    /*!
     * Compares persistent identity rather than full content.
     *
     * Database ids are used when both tracks have one. Otherwise identity falls back to the physical track segment:
     * unique filepath, subsong, cue offset and duration.
     */
    [[nodiscard]] bool sameIdentityAs(const Track& other) const;
    /*!
     * Compares all stored Track fields, including ids, paths, metadata, technical info and transient flags.
     *
     * This is stricter than operator==(), which only compares unique filepath, duration and generated hash.
     */
    [[nodiscard]] bool sameDataAs(const Track& other) const;

    /*!
     * Generates and stores the library matching hash.
     *
     * The hash is derived from artist, album, disc/track number, title (or directory/filename fallback) and subsong.
     */
    QString generateHash();

    [[nodiscard]] bool isValid() const;
    [[nodiscard]] bool isEnabled() const;
    [[nodiscard]] bool isInLibrary() const;
    [[nodiscard]] bool isInDatabase() const;
    //! True once an AudioReader has successfully populated metadata for this instance.
    [[nodiscard]] bool metadataWasRead() const;
    //! True when user-editable metadata has been changed since the flag was last cleared.
    [[nodiscard]] bool metadataWasModified() const;
    [[nodiscard]] bool exists() const;
    [[nodiscard]] int libraryId() const;

    [[nodiscard]] bool isInArchive() const;
    [[nodiscard]] QString archivePath() const;
    [[nodiscard]] QString pathInArchive() const;
    [[nodiscard]] QString relativeArchivePath() const;

    [[nodiscard]] int id() const;
    [[nodiscard]] QString hash() const;
    //! Returns a grouping key for album-oriented views; this is not persisted like the track identity hash.
    [[nodiscard]] QString albumHash() const;
    [[nodiscard]] QString filepath() const;
    //! Returns filepath plus logical segment identity, so multiple entries in one file can be distinguished.
    [[nodiscard]] QString uniqueFilepath() const;
    //! Returns a display path; archive entries are shown as "archive/path/entry" instead of their unpack URL.
    [[nodiscard]] QString prettyFilepath() const;
    [[nodiscard]] QString filename() const;
    [[nodiscard]] QString path() const;
    [[nodiscard]] QString directory() const;
    [[nodiscard]] QString extension() const;
    [[nodiscard]] QString filenameExt() const;
    [[nodiscard]] QString title() const;
    //! Returns title when set, otherwise the filename without extension.
    [[nodiscard]] QString effectiveTitle() const;
    [[nodiscard]] bool hasArtists() const;
    [[nodiscard]] qsizetype artistCount() const;
    [[nodiscard]] QString artistAt(qsizetype index) const;
    [[nodiscard]] QStringList artists() const;
    [[nodiscard]] QString artistsJoined(const QString& sep) const;
    [[nodiscard]] QStringList uniqueArtists() const;
    //! Returns artists joined with Constants::UnitSeparator for script/metadata expansion.
    [[nodiscard]] QString artist() const;
    //! Returns artist, album artist, composer or performer, in that order.
    [[nodiscard]] QString primaryArtist() const;
    //! Returns artists not already present in albumArtists(), joined with Constants::UnitSeparator.
    [[nodiscard]] QString uniqueArtist() const;
    [[nodiscard]] QString album() const;
    [[nodiscard]] bool hasAlbumArtists() const;
    [[nodiscard]] qsizetype albumArtistCount() const;
    [[nodiscard]] QString albumArtistAt(qsizetype index) const;
    [[nodiscard]] QStringList albumArtists() const;
    [[nodiscard]] QString albumArtistsJoined(const QString& sep) const;
    //! Returns album artists joined with Constants::UnitSeparator for script/metadata expansion.
    [[nodiscard]] QString albumArtist() const;
    //! Returns album artist, optional "Various Artists" for compilation tags, then artist/composer/performer fallback.
    [[nodiscard]] QString effectiveAlbumArtist(bool useVarious = false) const;
    [[nodiscard]] QString trackNumber() const;
    [[nodiscard]] QString trackTotal() const;
    [[nodiscard]] QString discNumber() const;
    [[nodiscard]] QString discTotal() const;
    [[nodiscard]] bool hasGenres() const;
    [[nodiscard]] qsizetype genreCount() const;
    [[nodiscard]] QString genreAt(qsizetype index) const;
    [[nodiscard]] QStringList genres() const;
    [[nodiscard]] QString genresJoined(const QString& sep) const;
    //! Returns genres joined with Constants::UnitSeparator for script/metadata expansion.
    [[nodiscard]] QString genre() const;
    [[nodiscard]] QStringList composers() const;
    //! Returns composers joined with Constants::UnitSeparator for script/metadata expansion.
    [[nodiscard]] QString composer() const;
    [[nodiscard]] QStringList performers() const;
    //! Returns performers joined with Constants::UnitSeparator for script/metadata expansion.
    [[nodiscard]] QString performer() const;
    [[nodiscard]] QString comment() const;
    [[nodiscard]] QString date() const;
    [[nodiscard]] int year() const;
    [[nodiscard]] float rating() const;
    //! Returns rating() on the internal 0-10 half-star scale.
    [[nodiscard]] int ratingStars() const;
    [[nodiscard]] QString ratingStarsText() const;

    [[nodiscard]] bool hasRGInfo() const;
    [[nodiscard]] bool hasTrackGain() const;
    [[nodiscard]] bool hasAlbumGain() const;
    [[nodiscard]] bool hasTrackPeak() const;
    [[nodiscard]] bool hasAlbumPeak() const;
    [[nodiscard]] float rgTrackGain() const;
    [[nodiscard]] float rgAlbumGain() const;
    [[nodiscard]] float rgTrackPeak() const;
    [[nodiscard]] float rgAlbumPeak() const;
    [[nodiscard]] std::optional<int16_t> opusHeaderGainQ78() const;
    [[nodiscard]] bool hasOpusHeaderGain() const;
    //! Returns the Opus header output gain in dB. The stored value is Q7.8 fixed point.
    [[nodiscard]] float opusHeaderGainDb() const;
    //! Effective RG values include Opus header output gain when present.
    [[nodiscard]] bool hasEffectiveTrackGain() const;
    [[nodiscard]] bool hasEffectiveAlbumGain() const;
    [[nodiscard]] bool hasEffectiveTrackPeak() const;
    [[nodiscard]] bool hasEffectiveAlbumPeak() const;
    [[nodiscard]] float effectiveRGTrackGain() const;
    [[nodiscard]] float effectiveRGAlbumGain() const;
    [[nodiscard]] float effectiveRGTrackPeak() const;
    [[nodiscard]] float effectiveRGAlbumPeak() const;
    [[nodiscard]] bool isOpus() const;

    //! True for external cue tracks and tracks generated from embedded cue sheets.
    [[nodiscard]] bool hasCue() const;
    //! True when cuePath() is "Embedded".
    [[nodiscard]] bool hasEmbeddedCue() const;
    //! External cue file path, or "Embedded" for tracks generated from an embedded CUESHEET tag.
    [[nodiscard]] QString cuePath() const;
    //! Logical segment type for tracks that represent a bounded region of a continuous file.
    [[nodiscard]] SegmentType segmentType() const;
    //! True for CUE tracks and chapter tracks that are bounded by offset + duration.
    [[nodiscard]] bool isBoundedSegment() const;
    //! True for segments that can be played by continuing the same decoded stream.
    [[nodiscard]] bool isSameStreamSegment() const;

    //! Archive paths use fooyin's unpack:// URL format.
    static bool isArchivePath(const QString& path);
    //! True for built-in tags that can hold multiple values.
    static bool isMultiValueTag(const QString& tag);
    //! True when tag is not one of Track's built-in metadata fields.
    static bool isExtraTag(const QString& tag);

    [[nodiscard]] bool hasExtraTag(const QString& tag) const;
    //! Returns an extra tag by case-insensitive key. Extra tag keys are stored upper-case.
    [[nodiscard]] QStringList extraTag(const QString& tag) const;
    //! Returns metadata tags that are not represented by dedicated Track fields.
    [[nodiscard]] ExtraTags extraTags() const;
    //! Returns extra tag names removed during editing, for writers that need to delete tags from files.
    [[nodiscard]] QStringList removedTags() const;
    [[nodiscard]] QByteArray serialiseExtraTags() const;

    //! Returns the non-empty built-in metadata fields formatted for display.
    [[nodiscard]] QMap<QString, QString> metadata() const;

    [[nodiscard]] bool hasExtraProperty(const QString& prop) const;
    /*!
     * Returns technical/runtime properties that are not persisted as audio tags.
     *
     * These properties can be displayed in the track info UI unless their key starts with "_".
     */
    [[nodiscard]] ExtraProperties extraProperties() const;
    //! Serialises extraProperties(), omitting normalised-away internal defaults such as zero Opus header gain.
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
    [[nodiscard]] QString codecProfile() const;
    [[nodiscard]] QString tool() const;
    [[nodiscard]] QString tagType(const QString& sep = {}) const;
    [[nodiscard]] QStringList tagTypes() const;
    [[nodiscard]] QString encoding() const;

    [[nodiscard]] int playCount() const;

    [[nodiscard]] uint64_t createdTime() const;
    [[nodiscard]] uint64_t addedTime() const;
    [[nodiscard]] uint64_t modifiedTime() const;
    [[nodiscard]] uint64_t lastModified() const;
    [[nodiscard]] uint64_t firstPlayed() const;
    [[nodiscard]] uint64_t lastPlayed() const;

    //! Case-insensitive search across common display metadata and filepath.
    [[nodiscard]] bool hasMatch(const QString& term) const;
    [[nodiscard]] std::shared_ptr<TrackMetadataStore> metadataStore() const;

    void setLibraryId(int id);
    //! Moves interned string data into the supplied store while preserving resolved metadata values.
    void setMetadataStore(std::shared_ptr<TrackMetadataStore> store);
    void setIsEnabled(bool enabled);
    void setMetadataWasRead(bool wasRead);
    void setId(int id);
    void setHash(const QString& hash);
    //! Sets the physical path and refreshes archive fields when path uses the unpack:// archive URL format.
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
    void setComposers(const QStringList& composers);
    void setPerformers(const QStringList& performers);
    void setComment(const QString& comment);
    /*!
     * Sets the raw date string and derives year/date epoch fields when it contains YYYY, YYYY-MM or YYYY-MM-DD.
     */
    void setDate(const QString& date);
    void setYear(int year);
    //! Sets normalised rating in the range (0, 1]; any other value clears the rating.
    void setRating(float rating);
    //! Sets rating on the internal 0-10 half-star scale; 0 clears the rating.
    void setRatingStars(int rating);

    void setRGTrackGain(float gain);
    void setRGAlbumGain(float gain);
    void setRGTrackPeak(float peak);
    void setRGAlbumPeak(float peak);
    //! Stores Opus header output gain as Q7.8 fixed point in an internal extra property.
    void setOpusHeaderGainQ78(int16_t gainQ78);
    void clearOpusHeaderGain();
    void clearRGInfo();

    /*!
     * Returns values for built-in metadata, raw rating tags, or extra tags.
     *
     * Multi-value built-in fields are returned as separate list entries. Single-value fields return a one-item list.
     */
    [[nodiscard]] QStringList metaValues(const QString& name) const;
    //! Returns metaValues() as a scalar, joining extra-tag values with Constants::UnitSeparator where needed.
    [[nodiscard]] QString metaValue(const QString& name) const;
    //! Returns technical metadata by scripting name, falling back to extra tags for unknown names.
    [[nodiscard]] QString techInfo(const QString& name) const;
    //! Returns an epoch-milliseconds value for supported date fields, otherwise std::nullopt.
    [[nodiscard]] std::optional<int64_t> dateValue(const QString& name) const;

    //! Sets the cue source path; use "Embedded" for tracks generated from an embedded cue sheet.
    void setCuePath(const QString& path);
    //! Marks this track as a chapter segment; cue segments are driven by setCuePath().
    void setIsChapter(bool isChapter);

    //! Raw rating tag values are stored as internal extra properties so the configured rating tag can be round-tripped.
    [[nodiscard]] QString rawRatingTag(const QString& tag) const;
    void setRawRatingTag(const QString& tag, const QString& value);
    void removeRawRatingTag(const QString& tag);

    //! Appends a value to an extra tag. Empty tag names or values are ignored.
    void addExtraTag(const QString& tag, const QString& value);
    void addExtraTag(const QString& tag, const QStringList& value);
    //! Removes an extra tag and records the key in removedTags().
    void removeExtraTag(const QString& tag);
    //! Replaces an extra tag. Empty values remove the tag.
    void replaceExtraTag(const QString& tag, const QString& value);
    void replaceExtraTag(const QString& tag, const QStringList& value);
    void clearExtraTags();
    //! Replaces all extra tags from a serialised blob; invalid or empty blobs leave the map empty.
    void storeExtraTags(const QByteArray& tags);

    /*!
     * Sets a non-tag property used for technical details or internal state.
     *
     * Empty values remove the property. Properties whose names start with "_" are treated as internal and are not shown
     * in the track info UI.
     */
    void setExtraProperty(const QString& prop, const QString& value);
    void removeExtraProperty(const QString& prop);
    //! Removes internal extra properties whose value is equivalent to the default state.
    void normaliseExtraProperties();
    void clearExtraProperties();
    //! Replaces all extra properties from a serialised blob; invalid or empty blobs leave the map empty.
    void storeExtraProperties(const QByteArray& props);

    void setSubsong(int index);
    void setOffset(uint64_t offset);
    void setDuration(uint64_t duration);
    void setFileSize(uint64_t fileSize);
    void setBitrate(int rate);
    void setSampleRate(int rate);
    void setChannels(int channels);
    void setBitDepth(int depth);
    void setCodec(const QString& codec);
    void setCodecProfile(const QString& profile);
    void setTool(const QString& tool);
    void setTagTypes(const QStringList& tagTypes);
    void setEncoding(const QString& encoding);

    void setPlayCount(int count);

    void setCreatedTime(uint64_t time);
    void setAddedTime(uint64_t time);
    void setModifiedTime(uint64_t time);
    void setFirstPlayed(uint64_t time);
    void setLastPlayed(uint64_t time);

    //! Clears metadataWasModified() after changes have been saved or accepted.
    void clearWasModified();

    static QString findCommonField(const TrackList& tracks);
    static TrackIds trackIdsForTracks(const TrackList& tracks);

    static QStringList supportedMimeTypes();

private:
    QSharedDataPointer<TrackPrivate> p;
};
FYCORE_EXPORT size_t qHash(const Track& track);

struct CoverImage
{
    QString mimeType;
    QByteArray data;
};
using TrackCovers = std::map<Track::Cover, CoverImage>;

struct TrackCoverData
{
    TrackList tracks;
    TrackCovers coverData;
};

[[nodiscard]] FYCORE_EXPORT Track prepareOpusRGWriteTrack(const Track& track, OpusRGWriteMode mode);
} // namespace Fooyin

Q_DECLARE_METATYPE(Fooyin::TrackList)
Q_DECLARE_METATYPE(Fooyin::TrackIds)
