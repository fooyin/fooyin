/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
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

#include <core/library/playbackstatistics.h>
#include <utils/database/dbmodule.h>

#include <QCoreApplication>
#include <QPromise>
#include <QSqlDatabase>
#include <QSqlError>

#include <expected>
#include <map>
#include <memory>
#include <optional>

namespace Fooyin {
class TrackMetadataStore;

class FYCORE_EXPORT PlaybackStatisticsDatabase : public DbModule
{
    Q_DECLARE_TR_FUNCTIONS(Fooyin::PlaybackStatisticsDatabase)

public:
    struct Statistics
    {
        uint64_t added{0};
        uint64_t firstPlayed{0};
        uint64_t lastPlayed{0};
        int playCount{0};
        float rating{-1.0F};
        bool loved{false};

        [[nodiscard]] Track toTrack() const;
        void applyTo(Track& track) const;
    };

    struct MaintenanceResult
    {
        PlaybackStatisticsResult result;
        TrackList tracks;
        std::map<int, Track::Stats> fileTagChanges;
    };

    explicit PlaybackStatisticsDatabase(const DbConnectionProvider& dbProvider);

    [[nodiscard]] const PlaybackStatisticsConfig& config() const;
    [[nodiscard]] uint64_t revision() const;
    [[nodiscard]] QString keyForTrack(const Track& track) const;
    void prepareTrack(Track& track) const;

    [[nodiscard]] std::expected<Track, QSqlError> trackStatistics(int trackId) const;
    bool refreshStaleStatistics(Track& track) const;
    bool refreshStatistics(TrackList& tracks) const;
    [[nodiscard]] std::expected<std::optional<Statistics>, QSqlError> existingStats(const QString& key) const;
    [[nodiscard]] std::expected<Statistics, QSqlError> updateStats(const Track& track, Track::Stats fields) const;
    [[nodiscard]] std::expected<Statistics, QSqlError> updateStats(const Track& track, Track::Stats fields,
                                                                   const Statistics& current,
                                                                   bool overwritePlaycount = false) const;
    [[nodiscard]] bool replaceStats(const QString& key, const Track& track) const;

    MaintenanceResult manageStatistics(const PlaybackStatisticsRequest& request,
                                       const std::shared_ptr<TrackMetadataStore>& metadataStore,
                                       QPromise<PlaybackStatisticsResult>* progress = nullptr);
    void cleanupStats() const;

private:
    PlaybackStatisticsConfig m_config;
    uint64_t m_revision;
};
} // namespace Fooyin
