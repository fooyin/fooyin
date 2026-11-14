/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <luket@pm.me>
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

#include "playbackstatisticsdatabase.h"

#include <core/library/playbackstatistics.h>
#include <core/track.h>
#include <utils/database/dbmodule.h>

#include <QCoreApplication>
#include <QSqlError>

#include <expected>
#include <memory>
#include <optional>
#include <set>

namespace Fooyin {
class TrackMetadataStore;

class FYCORE_EXPORT TrackDatabase : public DbModule
{
    Q_DECLARE_TR_FUNCTIONS(Fooyin::TrackDatabase)

public:
    void setMetadataStore(std::shared_ptr<TrackMetadataStore> store);

    bool storeTracks(TrackList& tracks);
    bool updateTracks(TrackList& tracks, bool overwritePlaycount = false);
    std::expected<void, QString> writeScanBatch(TrackList& tracksToStore, TrackList& tracksToUpdate,
                                                bool overwritePlaycount = false);
    std::expected<void, QString> writeScanBatch(TrackList& tracksToStore, TrackList& tracksToUpdate,
                                                bool overwritePlaycount,
                                                std::set<std::pair<QString, QString>>& migratedGroups);

    [[nodiscard]] bool reloadTrack(Track& track) const;
    [[nodiscard]] bool reloadTracks(TrackList& tracks) const;
    [[nodiscard]] TrackList getAllTracks() const;
    [[nodiscard]] TrackList tracksByHash(const QString& hash) const;
    [[nodiscard]] int idForTrack(Track& track) const;
    [[nodiscard]] std::expected<QString, QSqlError> rawRatingTag(int trackId, const QString& tag) const;

    struct MetadataUpdateResult
    {
        TrackList tracks;
        int failed{0};
    };
    MetadataUpdateResult updateTrackMetadata(const TrackList& tracks);
    [[nodiscard]] bool updateTrackModifiedTime(const Track& track) const;
    [[nodiscard]] bool updateTrackFileProperties(const Track& track) const;
    bool updateTrackStats(const Track& track);
    bool updateTrackStats(Track& track, Track::Stats updatedStats);
    bool updateTrackStats(const TrackList& tracks);
    bool updateTrackStats(TrackList& tracks);

    PlaybackStatisticsDatabase::MaintenanceResult
    manageStatistics(const PlaybackStatisticsRequest& request, QPromise<PlaybackStatisticsResult>* progress = nullptr);
    [[nodiscard]] TrackList tracksByStatisticsKeys(const std::set<QString>& keys) const;
    [[nodiscard]] bool refreshStaleStatistics(Track& track) const;

    bool deleteTrack(int id);
    bool deleteTracks(const TrackList& tracks);
    std::set<int> deleteLibraryTracks(int libraryId);

    void cleanupTracks();

    static void dropViews(const QSqlDatabase& db);
    static void insertViews(const QSqlDatabase& db);

private:
    bool updateTracks(TrackList& tracks, bool overwritePlaycount,
                      std::set<std::pair<QString, QString>>& migratedGroups);
    [[nodiscard]] int trackCount() const;
    [[nodiscard]] bool insertTrack(Track& track, bool ignoreDuplicates = false) const;
    std::expected<Track, QSqlError> updateTrack(Track track, PlaybackStatisticsDatabase& statistics,
                                                std::set<std::pair<QString, QString>>& migratedGroups,
                                                bool importStatistics, bool overwritePlaycount = false);
    void removeUnmanagedTracks() const;

    std::shared_ptr<TrackMetadataStore> m_metadataStore;
};
} // namespace Fooyin
