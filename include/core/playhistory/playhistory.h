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

#include <core/track.h>
#include <utils/database/dbconnectionpool.h>

#include <QFuture>
#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>

#include <memory>
#include <vector>

namespace Fooyin {
class PlayHistoryDatabase;

/*!
 * A single recorded listen.
 *
 * Track metadata is copied into the entry at the time of the listen, so history remains meaningful
 * if the track is later retagged, moved or removed from the library.
 */
struct FYCORE_EXPORT PlayHistoryEntry
{
    //! Milliseconds since the Unix epoch at which the listen was counted.
    qint64 timestamp{0};
    QString hash;
    QString title;
    QStringList artists;
    QString album;
    QStringList albumArtists;
    QStringList genres;
    QStringList composers;
    int year{0};
    //! Track duration in milliseconds.
    uint64_t duration{0};
    //! Bitrate in kbps, or 0 when unknown.
    int bitrate{0};
    //! 1 for lossless codecs, 0 for lossy codecs, -1 when unknown.
    int lossless{-1};

    //! Identifies the album this entry belongs to, or an empty string when it has no album.
    [[nodiscard]] QString albumKey() const;

    //! Builds the key identifying an album from its title and album artists.
    [[nodiscard]] static QString albumKey(const QString& album, const QStringList& albumArtists);

    static PlayHistoryEntry fromTrack(const Track& track, qint64 timestamp);
};

/*!
 * Listens within a time range, along with the lifetime aggregates needed to put them in context
 * (e.g. whether a track was first heard in the range).
 */
struct FYCORE_EXPORT PlayHistoryData
{
    //! Listens in the requested range, oldest first.
    std::vector<PlayHistoryEntry> entries;
    //! Timestamp of the earliest listen of any entry in the history.
    qint64 oldestTimestamp{0};
    //! First ever listen of each track hash.
    QHash<QString, qint64> firstListenByHash;
    //! Most recent listen of each track hash before the start of the requested range.
    QHash<QString, qint64> lastListenBeforeByHash;
    //! First ever listen of each artist.
    QHash<QString, qint64> firstListenByArtist;
};

//! Overview of everything in the history.
struct FYCORE_EXPORT PlayHistorySummary
{
    int totalListens{0};
    qint64 oldestTimestamp{0};
    qint64 newestTimestamp{0};
    //! Total duration of every recorded listen in milliseconds.
    qint64 totalListenMs{0};
    //! Calendar years (local time) containing at least one listen, ascending.
    std::vector<int> years;

    [[nodiscard]] bool isEmpty() const
    {
        return totalListens == 0;
    }
};

/*!
 * Records each counted listen so that statistics can be derived from them.
 *
 * Fooyin's per-track play count only keeps totals and the first/last play times, which can't answer
 * questions such as when or how often a track was played. Every time a listen is counted the track's
 * metadata and the time are stored here instead.
 */
class FYCORE_EXPORT PlayHistory : public QObject
{
    Q_OBJECT

public:
    explicit PlayHistory(DbConnectionPoolPtr dbPool, QObject* parent = nullptr);
    ~PlayHistory() override;

    //! Records a listen of @p track at the current time.
    void recordPlay(const Track& track);

    //! Loads the listens in [@p from, @p to) (milliseconds since the epoch) without blocking the caller.
    [[nodiscard]] QFuture<PlayHistoryData> load(qint64 from, qint64 to) const;
    [[nodiscard]] QFuture<PlayHistorySummary> summary() const;

Q_SIGNALS:
    void playRecorded();

private:
    DbConnectionPoolPtr m_dbPool;
    std::unique_ptr<PlayHistoryDatabase> m_database;
};
} // namespace Fooyin
