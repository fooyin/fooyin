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
#include <core/player/playerdefs.h>
#include <core/track.h>
#include <utils/database/dbconnectionpool.h>

#include <QBasicTimer>
#include <QElapsedTimer>
#include <QFuture>
#include <QObject>

#include <optional>

namespace Fooyin {
class PlaybackHistoryDatabase;
class PlayerController;

class FYCORE_EXPORT PlaybackHistoryService : public QObject
{
    Q_OBJECT

public:
    PlaybackHistoryService(PlayerController* playerController, DbConnectionPoolPtr dbPool, QObject* parent = nullptr);
    ~PlaybackHistoryService() override;

    [[nodiscard]] std::optional<PlaybackEvent> currentEvent() const;
    [[nodiscard]] PlaybackSourceKind currentSourceKind() const;
    [[nodiscard]] QFuture<PlaybackHistoryOverview> queryOverview(PlaybackHistoryScope scope) const;
    [[nodiscard]] QFuture<PlaybackHistoryOverview> queryOverview(PlaybackHistoryScope scope,
                                                                 const std::optional<PlaybackEvent>& liveEvent) const;
    [[nodiscard]] QFuture<PlaybackHistoryPage> queryHistory(const PlaybackHistoryQuery& query) const;
    [[nodiscard]] QFuture<std::vector<StreamMetadataSpan>> queryMetadataSpans(qint64 eventId) const;
    [[nodiscard]] QFuture<std::vector<PlaybackSourceRanking>>
    querySourceRankings(const PlaybackRankingQuery& query) const;
    [[nodiscard]] QFuture<std::vector<PlaybackSourceRanking>>
    querySourceRankings(const PlaybackRankingQuery& query, const std::optional<PlaybackEvent>& liveEvent) const;
    [[nodiscard]] QFuture<LibraryStatistics> queryLibraryStatistics() const;
    [[nodiscard]] QFuture<std::vector<LibraryGroupStatistics>> queryLibraryBreakdown(const QString& script) const;

    /** Changes recording mode from now, keeping previously recorded statistics. */
    void setTrackingMode(int mode);
    /** Removes recorded history and resumes recording from now if tracking and playback are active. */
    bool clearHistory();

    /** Finalises the active event after a playback/decode error. */
    void playbackError();
    void shutdown();

Q_SIGNALS:
    void historyChanged(qint64 eventId);
    void historyCleared();

protected:
    void timerEvent(QTimerEvent* event) override;

private:
    void recoverOpenEvents();
    void startEvent(uint64_t itemId, const Track& track);
    void finaliseEvent();
    bool checkpointEvent();

    void updateListeningTime();
    void updateStreamMetadata(const Track& track, bool allowTitleOnly = false);
    bool closeMetadataSpan();
    void handlePlayState(Player::PlayState state, Player::PlayState previous);

    PlayerController* m_playerController;
    DbConnectionPoolPtr m_dbPool;

    std::unique_ptr<PlaybackHistoryDatabase> m_database;
    std::optional<PlaybackEvent> m_currentEvent;
    std::optional<StreamMetadataSpan> m_currentSpan;
    uint64_t m_currentItemId;

    QBasicTimer m_checkpointTimer;
    QElapsedTimer m_listeningTimer;
    ListeningHistoryMode m_trackingMode;
    uint64_t m_totalsCheckpointMs;
    qint64 m_totalsSessionId;
    bool m_shutdown;
};
} // namespace Fooyin
