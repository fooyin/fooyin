/*
 * Fooyin
 * Copyright 2022-2024, Luke Taylor <LukeT1@proton.me>
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
 * There are two types of scan request:
 * - Tracks: Scans a TrackList; emits tracksScanned when finished.
 * - Library: Scans an entire library; emits tracksAdded, tracksUpdated, tracksDeleted.
 * In-progress requests can be cancelled early using cancel().
 */
struct ScanRequest
{
    enum Type : uint8_t
    {
        Tracks = 0,
        Library,
    };

    Type type;
    int id{-1};
    std::function<void()> cancel;
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

    /** Loads all tracks from the database */
    virtual void loadAllTracks() = 0;

    /** Returns @c true if there are no tracks */
    [[nodiscard]] virtual bool isEmpty() const = 0;

    /** Scans all tracks in all libraries */
    virtual void rescanAll() = 0;

    /** Scans the tracks in @p library */
    virtual void rescan(const LibraryInfo& library) = 0;

    /*!
     * Scans the @p tracks for metadata and adds to library.
     * @returns a ScanRequest representing a queued scan operation.
     * @note tracks will be considered unmanaged (not associated with an added library)
     */
    virtual ScanRequest* scanTracks(const TrackList& tracks) = 0;

    /** Returns all tracks for all libraries */
    [[nodiscard]] virtual TrackList tracks() const = 0;

    /** Returns a TrackList containing each track (if) found with an id from @p ids  */
    [[nodiscard]] virtual TrackList tracksForIds(const TrackIds& ids) const = 0;

    /** Updates the metdata in the database for @p tracks and writes metdata to files  */
    virtual void updateTrackMetadata(const TrackList& tracks) = 0;

    /** Updates the statistics (playcount, rating etc) in the database for @p track  */
    virtual void updateTrackStats(const Track& track) = 0;

signals:
    void scanProgress(int id, int percent);

    void tracksLoaded(const TrackList& tracks);
    void tracksAdded(const TrackList& tracks);
    void tracksScanned(const TrackList& tracks);
    void tracksUpdated(const TrackList& tracks);
    void tracksDeleted(const TrackList& tracks);
    void tracksSorted(const TrackList& tracks);

    void libraryAdded();
    void libraryChanged();
};
} // namespace Fooyin
