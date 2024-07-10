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

#include <core/track.h>

#include <QObject>

namespace Fooyin {
struct LibraryInfo;

/*!
 * There are three types of scan request:
 * - Files: Scans a list of files; emits tracksScanned when finished.
 * - Tracks: Scans a TrackList; emits tracksScanned when finished.
 * - Library: Scans an entire library; emits tracksAdded, tracksUpdated, tracksDeleted.
 * In-progress requests can be cancelled early using cancel().
 */
struct ScanRequest
{
    enum Type : uint8_t
    {
        Files = 0,
        Tracks,
        Library,
    };

    Type type;
    int id{-1};
    std::function<void()> cancel;
};

struct ScanProgress
{
    ScanRequest::Type type;
    int id{-1};
    int total{0};
    int current{0};

    [[nodiscard]] int percentage() const
    {
        if(id < 0 || total == 0) {
            return 100;
        }
        return std::max(0, static_cast<int>((static_cast<double>(current) / total) * 100));
    }
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

    /*!
     * Scans the @p tracks for metadata and adds to library.
     * @returns a ScanRequest representing a queued scan operation.
     * @note tracks will be considered unmanaged (not associated with an added library)
     */
    virtual ScanRequest scanTracks(const TrackList& tracks) = 0;
    /*!
     * Scans the @p files for tracks and adds to library.
     * @returns a ScanRequest representing a queued scan operation.
     * @note tracks will be considered unmanaged (not associated with an added library)
     */
    virtual ScanRequest scanFiles(const QList<QUrl>& files) = 0;

    /** Returns all tracks for all libraries */
    [[nodiscard]] virtual TrackList tracks() const = 0;

    /** Returns the track with an id of @p id, or an invalid track if not found.  */
    [[nodiscard]] virtual Track trackForId(int id) const = 0;
    /** Returns a TrackList containing each track (if) found with an id from @p ids  */
    [[nodiscard]] virtual TrackList tracksForIds(const TrackIds& ids) const = 0;

    /** Updates the metdata in the database for @p tracks and writes metdata to files  */
    virtual void updateTrackMetadata(const TrackList& tracks) = 0;

    /** Updates the statistics (playcount, rating etc) in the database for @p tracks  */
    virtual void updateTrackStats(const TrackList& tracks) = 0;
    /** Updates the statistics (playcount, rating etc) in the database for @p track  */
    virtual void updateTrackStats(const Track& track) = 0;

signals:
    void scanProgress(const Fooyin::ScanProgress& progress);
    void tracksScanned(int id, const Fooyin::TrackList& tracks);

    void tracksLoaded(const Fooyin::TrackList& tracks);
    void tracksAdded(const Fooyin::TrackList& tracks);
    void tracksUpdated(const Fooyin::TrackList& tracks);
    void tracksPlayed(const Fooyin::TrackList& tracks);
    void tracksDeleted(const Fooyin::TrackList& tracks);
    void tracksSorted(const Fooyin::TrackList& tracks);
};
} // namespace Fooyin
