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

#include <core/trackfwd.h>
#include <utils/database/dbmodule.h>

#include <set>

namespace Fooyin {
class TrackDatabase : public DbModule
{
public:
    bool storeTracks(TrackList& tracksToStore);

    bool reloadTrack(Track& track) const;
    bool reloadTracks(TrackList& tracks) const;
    TrackList getAllTracks() const;
    TrackList tracksByHash(const QString& hash) const;

    bool updateTrack(const Track& track);
    bool updateTrackStats(Track& track);

    bool deleteTrack(int id);
    bool deleteTracks(const TrackList& tracks);
    std::set<int> deleteLibraryTracks(int libraryId);

    void cleanupTracks();

    static void updateViews(const QSqlDatabase& db);

private:
    int trackCount() const;
    bool insertTrack(Track& track) const;
    bool insertOrUpdateStats(Track& track) const;
    void removeUnmanagedTracks() const;
    void markUnusedStatsForDelete() const;
    void deleteExpiredStats() const;
};
} // namespace Fooyin
