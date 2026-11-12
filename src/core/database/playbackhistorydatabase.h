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

#include <core/library/librarystatistics.h>
#include <core/player/playbackhistorydefs.h>
#include <utils/database/dbmodule.h>

#include <optional>
#include <vector>

namespace Fooyin {
class FYCORE_EXPORT PlaybackHistoryDatabase : public DbModule
{
public:
    [[nodiscard]] bool clearHistory() const;
    [[nodiscard]] bool addListeningTime(PlaybackSourceKind kind, qint64 timestamp, uint64_t deltaMs) const;

    [[nodiscard]] std::optional<PlaybackSource> source(qint64 sourceId) const;
    [[nodiscard]] std::optional<PlaybackSource> sourceByStableKey(const QString& stableKey) const;
    [[nodiscard]] qint64 upsertSource(const PlaybackSource& source) const;

    [[nodiscard]] qint64 insertEvent(const PlaybackEvent& event) const;
    [[nodiscard]] bool checkpointEvent(const PlaybackEvent& event) const;
    bool finaliseEvent(const PlaybackEvent& event) const;
    [[nodiscard]] std::optional<PlaybackEvent> event(qint64 eventId) const;
    [[nodiscard]] std::vector<PlaybackEvent> openEvents() const;

    [[nodiscard]] qint64 insertMetadataSpan(const StreamMetadataSpan& span) const;
    [[nodiscard]] bool closeMetadataSpan(qint64 spanId, uint64_t endedOffsetMs) const;
    [[nodiscard]] std::vector<StreamMetadataSpan> metadataSpans(qint64 eventId) const;

    [[nodiscard]] PlaybackHistoryOverview overview(PlaybackHistoryScope scope, qint64 generatedAt,
                                                   const std::optional<PlaybackEvent>& liveEvent = {}) const;
    [[nodiscard]] PlaybackHistoryPage history(const PlaybackHistoryQuery& query,
                                              const std::optional<PlaybackEvent>& liveEvent = {}) const;
    [[nodiscard]] std::vector<PlaybackSourceRanking>
    sourceRankings(const PlaybackRankingQuery& query, const std::optional<PlaybackEvent>& liveEvent = {}) const;
    [[nodiscard]] LibraryStatistics libraryStatistics() const;
};
} // namespace Fooyin
