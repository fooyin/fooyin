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

#include <core/track.h>

#include <QFuture>

#include <optional>

namespace Fooyin {
//! Script whose hashed output reproduces the original statistics hash/identity.
FYCORE_EXPORT QString defaultPlaybackStatisticsScript();

enum class PlaycountMergePolicy : uint8_t
{
    Maximum = 0,
    Sum,
    Clear
};

enum class RatingMergePolicy : uint8_t
{
    Highest = 0,
    LowestRated,
    Clear
};

enum class LovedMergePolicy : uint8_t
{
    Any = 0,
    All,
    Clear
};

struct PlaybackStatisticsConfig
{
    QString script{defaultPlaybackStatisticsScript()};
    PlaycountMergePolicy playcount{PlaycountMergePolicy::Maximum};
    RatingMergePolicy rating{RatingMergePolicy::Highest};
    LovedMergePolicy loved{LovedMergePolicy::Any};
    bool clearFirstPlayed{false};
    bool clearLastPlayed{false};
    bool clearSplitHistory{false};

    bool operator==(const PlaybackStatisticsConfig& other) const = default;
};

struct PlaybackStatisticsRequest
{
    enum class Operation : uint8_t
    {
        Read = 0,
        Preview,
        Rebuild,
        Reset,
        PreviewReset
    };

    Operation operation{Operation::Read};
    PlaybackStatisticsConfig config;
    TrackIds trackIds;
    Track::Stats resetStats{Track::Stat::Playcount};
    //! Overrides the current metadata write settings for this operation
    std::optional<Track::Stats> fileTagStats;
};

struct PlaybackStatisticsResult
{
    PlaybackStatisticsConfig config;
    QString error;
    //! Number of tracks whose statistics identity or stored values change
    int tracks{0};
    int groups{0};
    int mergedGroups{0};
    int splitGroups{0};
    int fallbackTracks{0};
    int orphanedGroups{0};
    int failedTagWrites{0};
};

class FYCORE_EXPORT PlaybackStatisticsProvider
{
public:
    virtual ~PlaybackStatisticsProvider() = default;

    virtual QFuture<PlaybackStatisticsResult> handlePlaybackStatistics(const PlaybackStatisticsRequest& request) = 0;
};
} // namespace Fooyin
