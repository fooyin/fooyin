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

#include <core/playhistory/playhistory.h>
#include <core/playhistory/playhistorystats.h>

#include <QDate>
#include <QFuture>
#include <QHash>
#include <QObject>

namespace Fooyin {
class MusicLibrary;

namespace Stats {
//! What the statistics are measured against in the library.
struct LibraryInfo
{
    int trackCount{0};
    //! Tracks that have never been played, according to their play count.
    int neverPlayed{0};
    qint64 durationMs{0};
    //! Number of tracks in each album, keyed by PlayHistoryEntry::albumKey().
    QHash<QString, int> albumTrackCounts;
};
} // namespace Stats

/*!
 * Provides the statistics shown by the statistics dialogs.
 *
 * The listens are loaded and summarised off the main thread.
 */
class StatisticsService : public QObject
{
    Q_OBJECT

public:
    StatisticsService(PlayHistory* history, MusicLibrary* library, QObject* parent = nullptr);

    //! Takes a fresh look at the library, which should be done before the statistics are shown.
    void refreshLibraryInfo();
    [[nodiscard]] const Stats::LibraryInfo& libraryInfo() const;

    [[nodiscard]] QFuture<PlayHistorySummary> summary() const;
    //! Statistics for the listens between @p firstDay and @p lastDay inclusive.
    [[nodiscard]] QFuture<PlayHistoryStats> compute(const QDate& firstDay, const QDate& lastDay,
                                                    bool monthlyTop = false) const;

Q_SIGNALS:
    //! Emitted when a listen has been added to the history.
    void historyChanged();

private:
    PlayHistory* m_history;
    MusicLibrary* m_library;
    Stats::LibraryInfo m_libraryInfo;
};
} // namespace Fooyin
