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

#include <QString>
#include <QStringList>

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace Fooyin {
enum class PlaybackSourceKind : uint8_t
{
    LocalTrack = 0,
    RemoteStream,
};

enum class ListeningHistoryMode : uint8_t
{
    Off = 0,
    TotalsOnly,
    FullHistory,
};

enum class PlaybackHistoryScope : uint8_t
{
    All = 0,
    LocalTracks,
    RemoteStreams,
};

struct FYCORE_EXPORT PlaybackHistoryTotals
{
    uint64_t listenCount{0};
    uint64_t uniqueSourceCount{0};
    uint64_t listeningTimeMs{0};
    std::optional<qint64> firstActivityAt;

    bool operator==(const PlaybackHistoryTotals&) const = default;
};

struct FYCORE_EXPORT PlaybackHistoryOverview
{
    PlaybackHistoryScope scope{PlaybackHistoryScope::All};
    PlaybackHistoryTotals lifetime;
    PlaybackHistoryTotals today;
    PlaybackHistoryTotals last7Days;
    PlaybackHistoryTotals last30Days;

    bool operator==(const PlaybackHistoryOverview&) const = default;
};

struct FYCORE_EXPORT PlaybackSource
{
    qint64 id{-1};
    PlaybackSourceKind kind{PlaybackSourceKind::LocalTrack};
    QString stableKey;
    QString displayName;
    QString originalUri;

    bool operator==(const PlaybackSource&) const = default;
};

struct FYCORE_EXPORT PlaybackEvent
{
    qint64 id{-1};
    qint64 sourceId{-1};
    qint64 startedAt{0};
    std::optional<qint64> endedAt;
    qint64 lastCheckpointAt{0};
    uint64_t listeningTimeMs{0};
    //! Time the local track reached the played threshold; unset for unqualified plays and remote streams.
    std::optional<qint64> playedAt;
    QString title;
    QStringList artists;
    QString album;
    QStringList albumArtists;
    QStringList genres;
    QStringList composers;
    QString date;
    int year{0};
    QString trackNumber;
    QString discNumber;
    QString filepath;
    uint64_t durationMs{0};
    QString codec;
    QString codecProfile;
    QString encoding;
    int bitrate{0};
    int bitDepth{0};
    int sampleRate{0};

    [[nodiscard]] bool isOpen() const
    {
        return !endedAt.has_value();
    }

    bool operator==(const PlaybackEvent&) const = default;
};

struct FYCORE_EXPORT StreamMetadataSpan
{
    qint64 id{-1};
    qint64 eventId{-1};
    uint64_t startedOffsetMs{0};
    std::optional<uint64_t> endedOffsetMs;
    QString rawStreamTitle;
    QString title;
    QStringList artists;
    QString album;

    [[nodiscard]] bool isOpen() const
    {
        return !endedOffsetMs.has_value();
    }

    bool operator==(const StreamMetadataSpan&) const = default;
};

struct FYCORE_EXPORT PlaybackHistoryQuery
{
    PlaybackHistoryScope scope{PlaybackHistoryScope::All};
    std::optional<qint64> from;
    std::optional<qint64> to;
    QString filter;
    uint64_t limit{100};
    //! Fetch older entries using the last page’s (StartedAt, EventID) pair.
    std::optional<std::pair<qint64, qint64>> before;
    //! Restrict returned entries to one event; totalCount still counts all matching history.
    std::optional<qint64> eventId;

    bool operator==(const PlaybackHistoryQuery&) const = default;
};

struct FYCORE_EXPORT PlaybackHistoryEntry
{
    PlaybackSource source;
    PlaybackEvent event;
    uint64_t metadataSpanCount{0};

    bool operator==(const PlaybackHistoryEntry&) const = default;
};

struct FYCORE_EXPORT PlaybackHistoryPage
{
    uint64_t totalCount{0};
    bool hasMore{false};
    std::vector<PlaybackHistoryEntry> entries;

    bool operator==(const PlaybackHistoryPage&) const = default;
};

struct FYCORE_EXPORT PlaybackRankingQuery
{
    PlaybackHistoryScope scope{PlaybackHistoryScope::All};
    //! Includes the local calendar day containing this timestamp.
    std::optional<qint64> from;
    //! Includes calendar days intersecting the range up to this exclusive timestamp.
    std::optional<qint64> to;
    uint64_t limit{10};
    std::optional<qint64> sourceId;

    bool operator==(const PlaybackRankingQuery&) const = default;
};

struct FYCORE_EXPORT PlaybackSourceRanking
{
    PlaybackSource source;
    uint64_t listenCount{0};
    uint64_t listeningTimeMs{0};
    qint64 lastListenedAt{0};

    bool operator==(const PlaybackSourceRanking&) const = default;
};
} // namespace Fooyin
