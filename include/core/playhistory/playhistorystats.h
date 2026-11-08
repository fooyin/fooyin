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

#include <core/playhistory/playhistory.h>

#include <QDate>
#include <QHash>
#include <QString>

#include <array>
#include <optional>
#include <vector>

namespace Fooyin {
//! A track as it appears in a ranking, with the context in which it was ranked.
struct FYCORE_EXPORT PlayHistoryStatsTrack
{
    QString hash;
    QString title;
    QString artist;
    QString album;
    //! Listens within the range.
    int count{0};
    //! Context-specific time in ms: first ever listen, day of the most plays, or end of a streak.
    qint64 timestamp{0};
    //! Context-specific number of days: gap since the previous listen, or streak length.
    int days{0};
};

struct FYCORE_EXPORT PlayHistoryStatsRank
{
    QString key;
    int count{0};
};

//! Listens grouped by a track field (artist, genre, ...), most listened first.
struct FYCORE_EXPORT PlayHistoryStatsCategory
{
    //! Highest ranked entries, capped to PlayHistoryStats::MaxTop.
    std::vector<PlayHistoryStatsRank> top;
    //! Listens not covered by the entries in top.
    int otherListens{0};
    //! Number of distinct values, including those not in top.
    int unique{0};
};

struct FYCORE_EXPORT PlayHistoryStatsSession
{
    qint64 startMs{0};
    qint64 endMs{0};
    int listens{0};

    [[nodiscard]] qint64 durationMs() const
    {
        return endMs - startMs;
    }
};

struct FYCORE_EXPORT PlayHistoryStatsStreak
{
    int days{0};
    QDate endDay;

    [[nodiscard]] QDate startDay() const
    {
        return endDay.addDays(1 - days);
    }
};

struct FYCORE_EXPORT PlayHistoryStatsAlbum
{
    QString name;
    int tracks{0};
    int listens{0};
};

/*!
 * Statistics derived from the listens in a range of days.
 *
 * Every day is a local calendar day and all times of day are in local time.
 */
struct FYCORE_EXPORT PlayHistoryStats
{
    enum class Category : uint8_t
    {
        Genre = 0,
        Artist,
        AlbumArtist,
        Composer,
        Album,
    };
    static constexpr int CategoryCount = 5;

    static constexpr int MaxTop               = 100;
    static constexpr int MaxObsessions        = 50;
    static constexpr int MaxNewArtists        = 10;
    static constexpr int MaxRediscoveries     = 5;
    static constexpr int SessionGapMs         = 15 * 60 * 1000;
    static constexpr int RediscoveryGapDays   = 180;
    static constexpr int HeavyRotationListens = 10;
    static constexpr int BitrateBucketCount   = 4;

    QDate firstDay;
    QDate lastDay;

    //! Listens on each day, index 0 being firstDay.
    std::vector<int> listensPerDay;
    int totalListens{0};
    int daysWithListens{0};
    int uniqueTracks{0};
    int uniqueArtists{0};
    int uniqueGenres{0};
    int uniqueAlbums{0};
    //! Estimated from the duration of each listened track.
    qint64 listenSeconds{0};

    std::array<int, 24> hours{};
    //! Sunday first.
    std::array<int, 7> weekdays{};
    //! Indexed by weekday (Sunday first) * 24 + hour.
    std::array<int, 7 * 24> weekHours{};
    //! Listens per calendar month, aggregated across years. Index 0 is January.
    std::array<int, 12> months{};

    std::vector<PlayHistoryStatsTrack> topTracks;
    std::array<PlayHistoryStatsCategory, CategoryCount> categories;

    //! Tracks first listened to within the range, most listened first.
    std::vector<PlayHistoryStatsTrack> newTracksTop;
    int newTracks{0};
    int newArtists{0};
    std::vector<PlayHistoryStatsRank> newArtistsTop;
    //! The track listened to the most in the range that was first heard before it.
    std::optional<PlayHistoryStatsTrack> oldestFavourite;

    //! Tracks by their most listens in a single day, highest first.
    std::vector<PlayHistoryStatsTrack> obsessions;
    std::optional<PlayHistoryStatsSession> longestSession;
    std::optional<PlayHistoryStatsStreak> longestStreak;
    int currentStreak{0};
    //! Most listened track in each calendar month, only populated when requested.
    std::array<std::optional<PlayHistoryStatsTrack>, 12> monthlyTop;

    QDate peakDay;
    int peakDayListens{0};

    //! Listens whose previous listen was the same track.
    int repeatListens{0};
    int oneTimeTracks{0};
    int heavyRotationTracks{0};

    //! Decade to listens, ascending.
    std::vector<PlayHistoryStatsRank> decades;
    //! Tracks listened to again after a long break.
    std::vector<PlayHistoryStatsTrack> rediscoveries;
    //! The track listened to on the most consecutive days.
    std::optional<PlayHistoryStatsTrack> longestTrackStreak;

    //! Listens below 128, 128-255, 256-319 and 320+ kbps.
    std::array<int, BitrateBucketCount> bitrateBuckets{};
    int losslessListens{0};
    int lossyListens{0};
    //! Albums for which every track was listened to.
    std::vector<PlayHistoryStatsAlbum> completedAlbums;

    //! Average minute of the day of the first and last listen of each day, or -1 when there are none.
    int avgFirstMinute{-1};
    int avgLastMinute{-1};

    [[nodiscard]] bool isEmpty() const
    {
        return totalListens == 0;
    }
    [[nodiscard]] int totalDays() const
    {
        return static_cast<int>(firstDay.daysTo(lastDay)) + 1;
    }
    [[nodiscard]] int listensOnDay(const QDate& day) const;
    [[nodiscard]] const PlayHistoryStatsCategory& category(Category category) const
    {
        return categories.at(static_cast<size_t>(category));
    }
};

struct FYCORE_EXPORT PlayHistoryStatsOptions
{
    //! Number of tracks in the track rankings.
    int topCount{PlayHistoryStats::MaxTop};
    //! Whether to work out the most listened track of each month.
    bool monthlyTop{false};
    //! Number of tracks in each album (keyed by PlayHistoryEntry::albumKey()), used to find completed albums.
    QHash<QString, int> albumTrackCounts;
};

/*!
 * Works out the statistics for the listens in @p data between @p firstDay and @p lastDay inclusive.
 *
 * The first day is raised to the day of the oldest listen so ranges reaching further back than the history
 * aren't padded with empty days.
 */
FYCORE_EXPORT PlayHistoryStats computePlayHistoryStats(const PlayHistoryData& data, QDate firstDay, QDate lastDay,
                                                       const PlayHistoryStatsOptions& options = {});
} // namespace Fooyin
