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

#include <core/library/libraryinfo.h>
#include <core/track.h>

#include <QFuture>
#include <QObject>

namespace Fooyin {
class TrackMetadataStore;
/*!
 * Represents a queued or in-progress library scan operation.
 *
 * Request completion is reported by scanFinished(). Some request types may emit
 * intermediate results before completion:
 * - Files: emits tracksScanned() for discovered standalone tracks and playlist-backed tracks.
 * - Tracks: emits tracksUpdated() via the library update path.
 * - Library: emits tracksAdded(), tracksUpdated(), and tracksDeleted() as changes are applied.
 * - Playlist: emits tracksScanned() with the resolved playlist entries.
 *
 * In-progress requests can be cancelled early using cancel().
 */
struct ScanRequest
{
    enum Type : uint8_t
    {
        Files = 0,
        Tracks,
        Library,
        Playlist,
    };

    Type type;
    int id{-1};
    std::function<void()> cancel;
};

/*!
 * Describes the current state of a scan request.
 *
 * @note total may be 0 when the amount of work is not yet known. In that case
 * percentage() returns 0 and callers should decide how to present indeterminate
 * progress.
 */
struct ScanProgress
{
    enum class Phase : uint8_t
    {
        Enumerating = 0,
        ReadingMetadata,
        WritingDatabase,
        Finalising,
        Finished,
    };

    ScanRequest::Type type;
    LibraryInfo info;
    int id{-1};
    int total{0};
    int current{0};
    int discovered{0};
    bool onlyModified{true};
    Phase phase{Phase::ReadingMetadata};
    QString file;

    /** Returns a clamped percentage in the range 0-100 when total is known. */
    [[nodiscard]] int percentage() const
    {
        if(id < 0) {
            return 100;
        }
        if(total <= 0) {
            return 0;
        }
        return std::clamp(static_cast<int>((static_cast<double>(current) / total) * 100), 0, 100);
    }
};

enum class WriteState : uint8_t
{
    Completed = 0,
    Cancelled,
};

struct WriteResult
{
    WriteState state{WriteState::Completed};
    int succeeded{0};
    int failed{0};
};

struct WriteRequest
{
    std::function<void()> cancel;
    QFuture<WriteResult> finished;
};

/*!
 * Represents a music library containing Track objects.
 * Acts as a unified library view for all tracks in all libraries,
 * and also holds unmanaged tracks (not associated with a library).
 */
class FYCORE_EXPORT MusicLibrary : public QObject
{
    Q_OBJECT

public:
    explicit MusicLibrary(QObject* parent = nullptr)
        : QObject{parent}
    { }

    /** Returns @c true if a library has been added by the user */
    [[nodiscard]] virtual bool hasLibrary() const = 0;
    /** Returns the library info with the id @p id if it exists */
    [[nodiscard]] virtual std::optional<LibraryInfo> libraryInfo(int id) const = 0;
    /** Returns the library info of the library which contains the path @p path */
    [[nodiscard]] virtual std::optional<LibraryInfo> libraryForPath(const QString& path) const = 0;

    /** Loads all tracks from the database */
    virtual void loadAllTracks() = 0;

    /** Returns @c true if there are no tracks */
    [[nodiscard]] virtual bool isEmpty() const = 0;

    /** Rescans modified tracks in all libraries */
    virtual void refreshAll() = 0;
    /** Rescans all tracks in all libraries */
    virtual void rescanAll() = 0;

    /** Rescans the modified tracks in @p library */
    virtual ScanRequest refresh(const LibraryInfo& library) = 0;
    /** Rescans the tracks in @p library */
    virtual ScanRequest rescan(const LibraryInfo& library) = 0;
    /** Cancels the in-progress or queued scan request with id @p id. */
    virtual void cancelScan(int id) = 0;

    /*!
     * Rescans the @p tracks, replacing existing metadata.
     * @returns a ScanRequest representing a queued scan operation.
     */
    virtual ScanRequest scanTracks(const TrackList& tracks) = 0;
    /*!
     * Rescans the @p tracks, replacing existing metadata if modified.
     * @returns a ScanRequest representing a queued scan operation.
     */
    virtual ScanRequest scanModifiedTracks(const TrackList& tracks) = 0;
    /*!
     * Scans the @p files for tracks and adds to library.
     * @returns a ScanRequest representing a queued scan operation.
     */
    virtual ScanRequest scanFiles(const QList<QUrl>& files) = 0;
    /*!
     * Loads the playlists and adds tracks to library.
     * @returns a ScanRequest representing a queued scan operation.
     */
    virtual ScanRequest loadPlaylist(const QList<QUrl>& files) = 0;

    /** Returns all tracks for all libraries */
    [[nodiscard]] virtual TrackList tracks() const = 0;
    /** Returns the track with an id of @p id, or an invalid track if not found.  */
    [[nodiscard]] virtual Track trackForId(int id) const = 0;
    /** Returns a TrackList containing each track (if) found with an id from @p ids  */
    [[nodiscard]] virtual TrackList tracksForIds(const TrackIds& ids) const = 0;
    /** Returns the metadata store used by resident library tracks. */
    [[nodiscard]] virtual std::shared_ptr<TrackMetadataStore> metadataStore() const = 0;

    /** Updates the track @p track in the library.  */
    virtual void updateTrack(const Track& track) = 0;
    /** Updates the tracks @p tracks in the library.  */
    virtual void updateTracks(const TrackList& tracks) = 0;

    /** Updates the metadata in the database for @p tracks.  */
    virtual void updateTrackMetadata(const TrackList& tracks) = 0;
    /** Updates the metadata in the database for @p tracks and writes metadata to files.  */
    virtual WriteRequest writeTrackMetadata(const TrackList& tracks) = 0;
    /*!
     * Writes the covers specified in @p coverData to all files in @p tracks.
     * @note if a cover's data is empty/null, it will be removed from the file.
     * @returns a WriteRequest which can be used to cancel the operation.
     */
    virtual WriteRequest writeTrackCovers(const TrackCoverData& coverData) = 0;

    /** Updates the statistics (playcount, rating etc) in the database for @p tracks.  */
    virtual void updateTrackStats(const TrackList& tracks) = 0;
    /** Updates the statistics (playcount, rating etc) in the database for @p track.  */
    virtual void updateTrackStats(const Track& track) = 0;

    /** Remove unavailable tracks from the library and database. */
    virtual WriteRequest removeUnavailbleTracks() = 0;

signals:
    /** Emitted whenever the progress state for a scan request changes. */
    void scanProgress(const Fooyin::ScanProgress& progress);
    /**
     * Emitted when a file or playlist scan has produced track results.
     * scanFinished() for the same request is emitted afterwards.
     */
    void tracksScanned(int id, const Fooyin::TrackList& tracks);
    /** Emitted exactly once when a scan request completes or is cancelled. */
    void scanFinished(int id, Fooyin::ScanRequest::Type type, bool cancelled);

    void tracksLoaded(const Fooyin::TrackList& tracks);
    void tracksAdded(const Fooyin::TrackList& tracks);
    void tracksMetadataChanged(const Fooyin::TrackList& tracks);
    void tracksUpdated(const Fooyin::TrackList& tracks);
    void tracksDeleted(const Fooyin::TrackList& tracks);
    void tracksSorted(const Fooyin::TrackList& tracks);
};
} // namespace Fooyin

Q_DECLARE_METATYPE(Fooyin::ScanProgress)
