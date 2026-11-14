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

#include "playbackhistorydatabase.h"

#include <core/constants.h>
#include <core/track.h>
#include <utils/database/dbquery.h>
#include <utils/database/dbtransaction.h>

#include <QDateTime>
#include <QTimeZone>
#include <QVariant>

#include <limits>

using namespace Qt::StringLiterals;

namespace Fooyin {
namespace {
std::vector<LibraryGroupStatistics> libraryGroups(const QSqlDatabase& db, const QString& expression,
                                                  const QString& order)
{
    const QString statement = u"SELECT %1 AS GroupName, COUNT(*), COALESCE(SUM(Tracks.Duration), 0), "
                              "COALESCE(SUM(Tracks.FileSize), 0), COALESCE(SUM(TrackStats.PlayCount), 0) "
                              "FROM Tracks LEFT JOIN TrackStats ON Tracks.StatisticsKey = TrackStats.StatisticsKey "
                              "LEFT JOIN Libraries ON Tracks.LibraryID = Libraries.LibraryID "
                              "WHERE Tracks.LibraryID >= 0 GROUP BY GroupName ORDER BY %2;"_s.arg(expression, order);
    DbQuery query{db, statement};
    if(!query.exec()) {
        return {};
    }

    std::vector<LibraryGroupStatistics> groups;
    while(query.next()) {
        groups.push_back(LibraryGroupStatistics{
            .name            = query.value(0).toString(),
            .trackCount      = query.value(1).toULongLong(),
            .totalDurationMs = query.value(2).toULongLong(),
            .totalFileSize   = query.value(3).toULongLong(),
            .totalPlayCount  = query.value(4).toULongLong(),
        });
    }
    return groups;
}

PlaybackSource sourceFromQuery(const DbQuery& query, int offset = 0)
{
    PlaybackSource source;
    source.id          = query.value(offset).toLongLong();
    source.kind        = static_cast<PlaybackSourceKind>(query.value(offset + 1).toInt());
    source.stableKey   = query.value(offset + 2).toString();
    source.displayName = query.value(offset + 3).toString();
    source.originalUri = query.value(offset + 4).toString();
    return source;
}

QString eventMetadataColumns()
{
    return u"Events.Title, Events.Artists, Events.Album, Events.AlbumArtists, Events.Genres, Events.Composers, "
           "Events.Date, Events.Year, Events.TrackNumber, Events.DiscNumber, Events.FilePath, "
           "Events.DurationMs, Events.Codec, Events.CodecProfile, Events.Encoding, Events.Bitrate, Events.BitDepth, "
           "Events.SampleRate"_s;
}

QStringList splitTagValues(const QString& values)
{
    return values.isEmpty() ? QStringList{} : values.split(QLatin1StringView{Constants::UnitSeparator});
}

PlaybackEvent eventFromQuery(const DbQuery& query, int offset = 0)
{
    PlaybackEvent event;
    event.id        = query.value(offset).toLongLong();
    event.sourceId  = query.value(offset + 1).toLongLong();
    event.startedAt = query.value(offset + 2).toLongLong();
    event.endedAt   = query.value(offset + 3).isNull() ? std::optional<qint64>{}
                                                       : std::optional<qint64>{query.value(offset + 3).toLongLong()};
    event.lastCheckpointAt = query.value(offset + 4).toLongLong();
    event.listeningTimeMs  = query.value(offset + 5).toULongLong();
    event.playedAt     = query.value(offset + 6).isNull() ? std::optional<qint64>{}
                                                          : std::optional<qint64>{query.value(offset + 6).toLongLong()};
    event.title        = query.value(offset + 7).toString();
    event.artists      = splitTagValues(query.value(offset + 8).toString());
    event.album        = query.value(offset + 9).toString();
    event.albumArtists = splitTagValues(query.value(offset + 10).toString());
    event.genres       = splitTagValues(query.value(offset + 11).toString());
    event.composers    = splitTagValues(query.value(offset + 12).toString());
    event.date         = query.value(offset + 13).toString();
    event.year         = query.value(offset + 14).toInt();
    event.trackNumber  = query.value(offset + 15).toString();
    event.discNumber   = query.value(offset + 16).toString();
    event.filepath     = query.value(offset + 17).toString();
    event.durationMs   = query.value(offset + 18).toULongLong();
    event.codec        = query.value(offset + 19).toString();
    event.codecProfile = query.value(offset + 20).toString();
    event.encoding     = query.value(offset + 21).toString();
    event.bitrate      = query.value(offset + 22).toInt();
    event.bitDepth     = query.value(offset + 23).toInt();
    event.sampleRate   = query.value(offset + 24).toInt();
    return event;
}

StreamMetadataSpan spanFromQuery(const DbQuery& query)
{
    StreamMetadataSpan span;
    span.id              = query.value(0).toLongLong();
    span.eventId         = query.value(1).toLongLong();
    span.startedOffsetMs = query.value(2).toULongLong();
    span.endedOffsetMs
        = query.value(3).isNull() ? std::optional<uint64_t>{} : std::optional<uint64_t>{query.value(3).toULongLong()};
    span.rawStreamTitle = query.value(4).toString();
    span.title          = query.value(5).toString();
    span.artists        = splitTagValues(query.value(6).toString());
    span.album          = query.value(7).toString();
    return span;
}

void bindEventProgress(DbQuery& query, const PlaybackEvent& event)
{
    query.bindValue(u":eventId"_s, event.id);
    query.bindValue(u":lastCheckpointAt"_s, event.lastCheckpointAt);
    query.bindValue(u":listeningTimeMs"_s, QVariant::fromValue(event.listeningTimeMs));
    query.bindValue(u":playedAt"_s, event.playedAt ? QVariant{*event.playedAt} : QVariant{});
}

qint64 listeningDay(qint64 timestamp)
{
    return QDateTime::fromMSecsSinceEpoch(timestamp).date().toJulianDay();
}

QString sourceActivityStatement()
{
    return uR"(
        WITH DailyTime AS (
            SELECT SourceID, SUM(ListeningTimeMs) AS ListeningTimeMs, MAX(LastListenedAt) AS LastListenedAt
            FROM (
                SELECT SourceID, Day, ListeningTimeMs, LastListenedAt FROM PlaybackDailyTotals
                UNION ALL
                SELECT SourceID, :liveDay, :liveListeningTimeMs - ListeningTimeMs, :liveLastCheckpointAt
                FROM PlaybackEvents WHERE EventID = :liveEventId AND :liveListeningTimeMs > ListeningTimeMs
            )
            WHERE Day >= :firstDay AND Day <= :lastDay
            GROUP BY SourceID
        ), EventProgress AS (
            SELECT SourceID, StartedAt, EndedAt,
                CASE WHEN EventID = :liveEventId THEN :liveLastCheckpointAt ELSE LastCheckpointAt END AS LastCheckpointAt,
                CASE WHEN EventID = :liveEventId THEN :liveListeningTimeMs ELSE ListeningTimeMs END AS ListeningTimeMs,
                CASE WHEN EventID = :liveEventId THEN :livePlayedAt ELSE PlayedAt END AS PlayedAt
            FROM PlaybackEvents
        ), ListenCounts AS (
            SELECT Events.SourceID,
                SUM(CASE
                    WHEN Sources.Kind = 0 AND Events.PlayedAt >= :periodStart AND Events.PlayedAt < :periodEnd
                        THEN 1
                    WHEN Sources.Kind != 0 AND Events.ListeningTimeMs > 0
                        THEN 1
                    ELSE 0
                END) AS ListenCount,
                MIN(Events.StartedAt) AS FirstActivityAt,
                MAX(COALESCE(Events.EndedAt, Events.LastCheckpointAt)) AS LastListenedAt
            FROM EventProgress AS Events
            JOIN PlaybackSources AS Sources ON Sources.SourceID = Events.SourceID
            WHERE (Events.StartedAt >= :periodStart AND Events.StartedAt < :periodEnd)
                OR (Sources.Kind = 0 AND Events.PlayedAt >= :periodStart AND Events.PlayedAt < :periodEnd)
            GROUP BY Events.SourceID
        ), Activity AS (
            SELECT Sources.SourceID, Sources.Kind, Sources.StableKey, Sources.DisplayName, Sources.OriginalURI,
                COALESCE(ListenCounts.ListenCount, 0) AS ListenCount,
                COALESCE(DailyTime.ListeningTimeMs, 0) AS ListeningTimeMs,
                COALESCE(DailyTime.LastListenedAt, ListenCounts.LastListenedAt, 0) AS LastListenedAt,
                ListenCounts.FirstActivityAt
            FROM PlaybackSources AS Sources
            LEFT JOIN ListenCounts ON ListenCounts.SourceID = Sources.SourceID
            LEFT JOIN DailyTime ON DailyTime.SourceID = Sources.SourceID
            WHERE ListenCounts.SourceID IS NOT NULL OR DailyTime.SourceID IS NOT NULL
        )
    )"_s;
}

void bindActivityPeriod(DbQuery& query, const std::optional<qint64>& from, const std::optional<qint64>& to,
                        const std::optional<PlaybackEvent>& liveEvent)
{
    query.bindValue(u":periodStart"_s, from.value_or(std::numeric_limits<qint64>::lowest()));
    query.bindValue(u":periodEnd"_s, to.value_or(std::numeric_limits<qint64>::max()));
    query.bindValue(u":firstDay"_s, from ? listeningDay(*from) : std::numeric_limits<qint64>::lowest());
    query.bindValue(u":lastDay"_s, to ? listeningDay(*to - 1) : std::numeric_limits<qint64>::max());
    query.bindValue(u":liveEventId"_s, liveEvent ? liveEvent->id : -1);
    query.bindValue(u":livePlayedAt"_s, liveEvent && liveEvent->playedAt ? QVariant{*liveEvent->playedAt} : QVariant{});
    query.bindValue(u":liveLastCheckpointAt"_s, liveEvent ? liveEvent->lastCheckpointAt : 0);
    query.bindValue(u":liveDay"_s, liveEvent ? listeningDay(liveEvent->lastCheckpointAt) : 0);
    query.bindValue(u":liveListeningTimeMs"_s,
                    QVariant::fromValue(liveEvent ? liveEvent->listeningTimeMs : uint64_t{0}));
}

PlaybackHistoryTotals totalsForPeriod(const QSqlDatabase& db, PlaybackHistoryScope scope,
                                      const std::optional<qint64>& periodStart, qint64 periodEnd,
                                      const std::optional<PlaybackEvent>& liveEvent)
{
    QString statement = sourceActivityStatement() + uR"(
        SELECT
            COALESCE(SUM(ListenCount), 0),
            COUNT(CASE WHEN ListenCount > 0 OR ListeningTimeMs > 0 THEN SourceID END),
            COALESCE(SUM(ListeningTimeMs), 0),
            MIN(FirstActivityAt)
        FROM Activity
        WHERE 1 = 1
    )"_s;

    switch(scope) {
        case PlaybackHistoryScope::All:
            break;
        case PlaybackHistoryScope::LocalTracks:
            statement += u" AND Kind = 0"_s;
            break;
        case PlaybackHistoryScope::RemoteStreams:
            statement += u" AND Kind = 1"_s;
            break;
    }
    statement += u';';

    DbQuery query{db, statement};
    bindActivityPeriod(query, periodStart, periodEnd, liveEvent);

    PlaybackHistoryTotals totals;
    if(!query.exec() || !query.next()) {
        return totals;
    }

    totals.listenCount       = query.value(0).toULongLong();
    totals.uniqueSourceCount = query.value(1).toULongLong();
    totals.listeningTimeMs   = query.value(2).toULongLong();
    if(!query.value(3).isNull()) {
        totals.firstActivityAt = query.value(3).toLongLong();
    }

    DbQuery summary{db, u"SELECT COALESCE(SUM(ListeningTimeMs), 0), MIN(Day) FROM PlaybackDailySummary "
                        "WHERE Day >= :firstDay AND Day <= :lastDay AND (:kind < 0 OR Kind = :kind);"_s};

    summary.bindValue(u":firstDay"_s, periodStart ? listeningDay(*periodStart) : std::numeric_limits<qint64>::lowest());
    summary.bindValue(u":lastDay"_s, listeningDay(periodEnd - 1));
    const int kind = scope == PlaybackHistoryScope::All ? -1 : scope == PlaybackHistoryScope::LocalTracks ? 0 : 1;
    summary.bindValue(u":kind"_s, kind);

    if(summary.exec() && summary.next()) {
        totals.listeningTimeMs += summary.value(0).toULongLong();
        if(!summary.value(1).isNull()) {
            const auto first = QDate::fromJulianDay(summary.value(1).toLongLong()).startOfDay().toMSecsSinceEpoch();
            totals.firstActivityAt = totals.firstActivityAt ? std::min(*totals.firstActivityAt, first) : first;
        }
    }

    if(liveEvent && liveEvent->id < -1 && liveEvent->listeningTimeMs > 0) {
        const auto day     = listeningDay(liveEvent->lastCheckpointAt);
        const int liveKind = Track::isRemotePath(liveEvent->filepath) ? 1 : 0;
        if((kind < 0 || kind == liveKind)
           && day >= (periodStart ? listeningDay(*periodStart) : std::numeric_limits<qint64>::lowest())
           && day <= listeningDay(periodEnd - 1)) {
            totals.listeningTimeMs += liveEvent->listeningTimeMs;
            const auto first       = QDate::fromJulianDay(day).startOfDay().toMSecsSinceEpoch();
            totals.firstActivityAt = totals.firstActivityAt ? std::min(*totals.firstActivityAt, first) : first;
        }
    }

    return totals;
}

QString escapedLikeFilter(QString filter)
{
    filter.replace(u'\\', u"\\\\"_s);
    filter.replace(u'%', u"\\%"_s);
    filter.replace(u'_', u"\\_"_s);
    return u"%"_s + filter + u"%"_s;
}

void appendHistoryConditions(QString& statement, const PlaybackHistoryQuery& historyQuery)
{
    switch(historyQuery.scope) {
        case PlaybackHistoryScope::All:
            break;
        case PlaybackHistoryScope::LocalTracks:
            statement += u" AND Sources.Kind = 0"_s;
            break;
        case PlaybackHistoryScope::RemoteStreams:
            statement += u" AND Sources.Kind = 1"_s;
            break;
    }
    if(historyQuery.from) {
        statement += u" AND Events.StartedAt >= :from"_s;
    }
    if(historyQuery.to) {
        statement += u" AND Events.StartedAt < :to"_s;
    }
    statement += u" AND (Sources.Kind != 0 OR Events.PlayedAt IS NOT NULL)"_s;
    if(!historyQuery.filter.trimmed().isEmpty()) {
        statement += uR"( AND (
            Sources.DisplayName LIKE :filter ESCAPE '\'
            OR Sources.OriginalURI LIKE :filter ESCAPE '\'
            OR Events.Title LIKE :filter ESCAPE '\'
            OR Events.Artists LIKE :filter ESCAPE '\'
            OR Events.Album LIKE :filter ESCAPE '\'
            OR Events.AlbumArtists LIKE :filter ESCAPE '\'
            OR Events.Genres LIKE :filter ESCAPE '\'
            OR Events.Composers LIKE :filter ESCAPE '\'
            OR Events.Date LIKE :filter ESCAPE '\'
            OR CAST(Events.Year AS TEXT) LIKE :filter ESCAPE '\'
            OR Events.TrackNumber LIKE :filter ESCAPE '\'
            OR Events.DiscNumber LIKE :filter ESCAPE '\'
            OR Events.FilePath LIKE :filter ESCAPE '\'
            OR Events.Codec LIKE :filter ESCAPE '\'
            OR Events.CodecProfile LIKE :filter ESCAPE '\'
            OR Events.Encoding LIKE :filter ESCAPE '\'
        ))"_s;
    }
}

void bindHistoryConditions(DbQuery& query, const PlaybackHistoryQuery& historyQuery)
{
    if(historyQuery.from) {
        query.bindValue(u":from"_s, *historyQuery.from);
    }
    if(historyQuery.to) {
        query.bindValue(u":to"_s, *historyQuery.to);
    }
    if(!historyQuery.filter.trimmed().isEmpty()) {
        query.bindValue(u":filter"_s, escapedLikeFilter(historyQuery.filter.trimmed()));
    }
}
} // namespace

bool PlaybackHistoryDatabase::clearHistory() const
{
    // Deleting sources also removes their events, metadata spans and daily totals
    DbTransaction transaction{db()};
    if(!transaction) {
        return false;
    }
    DbQuery sources{db(), u"DELETE FROM PlaybackSources;"_s};
    DbQuery summary{db(), u"DELETE FROM PlaybackDailySummary;"_s};
    return sources.exec() && summary.exec() && transaction.commit();
}

bool PlaybackHistoryDatabase::addListeningTime(PlaybackSourceKind kind, qint64 timestamp, uint64_t deltaMs) const
{
    if(deltaMs == 0) {
        return true;
    }
    DbQuery query{db(), uR"(
        INSERT INTO PlaybackDailySummary (Kind, Day, ListeningTimeMs) VALUES (:kind, :day, :delta)
        ON CONFLICT(Kind, Day) DO UPDATE SET
            ListeningTimeMs = PlaybackDailySummary.ListeningTimeMs + excluded.ListeningTimeMs;
    )"_s};
    query.bindValue(u":kind"_s, static_cast<int>(kind));
    query.bindValue(u":day"_s, listeningDay(timestamp));
    query.bindValue(u":delta"_s, QVariant::fromValue(deltaMs));
    return query.exec();
}

std::optional<PlaybackSource> PlaybackHistoryDatabase::source(const qint64 sourceId) const
{
    static const QString statement = u"SELECT SourceID, Kind, StableKey, DisplayName, OriginalURI "
                                     "FROM PlaybackSources WHERE SourceID = :sourceId;"_s;

    DbQuery query{db(), statement};
    query.bindValue(u":sourceId"_s, sourceId);
    if(!query.exec() || !query.next()) {
        return {};
    }

    return sourceFromQuery(query);
}

std::optional<PlaybackSource> PlaybackHistoryDatabase::sourceByStableKey(const QString& stableKey) const
{
    static const QString statement = u"SELECT SourceID, Kind, StableKey, DisplayName, OriginalURI "
                                     "FROM PlaybackSources WHERE StableKey = :stableKey;"_s;

    DbQuery query{db(), statement};
    query.bindValue(u":stableKey"_s, stableKey);
    if(!query.exec() || !query.next()) {
        return {};
    }

    return sourceFromQuery(query);
}

qint64 PlaybackHistoryDatabase::upsertSource(const PlaybackSource& source) const
{
    if(source.stableKey.isEmpty()) {
        return -1;
    }

    static const QString statement = uR"(
        INSERT INTO PlaybackSources (Kind, StableKey, DisplayName, OriginalURI)
        VALUES (:kind, :stableKey, :displayName, :originalUri)
        ON CONFLICT(StableKey) DO UPDATE SET
            Kind = excluded.Kind,
            DisplayName = COALESCE(NULLIF(excluded.DisplayName, ''), PlaybackSources.DisplayName),
            OriginalURI = COALESCE(NULLIF(excluded.OriginalURI, ''), PlaybackSources.OriginalURI);
    )"_s;

    DbQuery query{db(), statement};
    query.bindValue(u":kind"_s, static_cast<int>(source.kind));
    query.bindValue(u":stableKey"_s, source.stableKey);
    query.bindValue(u":displayName"_s, source.displayName);
    query.bindValue(u":originalUri"_s, source.originalUri);
    if(!query.exec()) {
        return -1;
    }

    const auto stored = sourceByStableKey(source.stableKey);
    return stored ? stored->id : -1;
}

qint64 PlaybackHistoryDatabase::insertEvent(const PlaybackEvent& event) const
{
    if(event.sourceId < 0) {
        return -1;
    }

    static const QString statement = uR"(
        INSERT INTO PlaybackEvents (
            SourceID, StartedAt, EndedAt, LastCheckpointAt, ListeningTimeMs, PlayedAt,
            Title, Artists, Album, AlbumArtists, Genres, Composers, Date, Year, TrackNumber, DiscNumber,
            FilePath, DurationMs, Codec, CodecProfile, Encoding, Bitrate, BitDepth, SampleRate
        ) VALUES (
            :sourceId, :startedAt, :endedAt, :lastCheckpointAt, :listeningTimeMs, :playedAt,
            :title, :artists, :album, :albumArtists, :genres, :composers, :date, :year,
            :trackNumber, :discNumber, :filepath, :durationMs, :codec, :codecProfile, :encoding,
            :bitrate, :bitDepth, :sampleRate
        );
    )"_s;

    DbQuery query{db(), statement};
    query.bindValue(u":sourceId"_s, event.sourceId);
    query.bindValue(u":startedAt"_s, event.startedAt);
    query.bindValue(u":endedAt"_s, event.endedAt ? QVariant{*event.endedAt} : QVariant{});
    query.bindValue(u":lastCheckpointAt"_s, event.lastCheckpointAt);
    query.bindValue(u":listeningTimeMs"_s, QVariant::fromValue(event.listeningTimeMs));
    query.bindValue(u":playedAt"_s, event.playedAt ? QVariant{*event.playedAt} : QVariant{});
    query.bindValue(u":title"_s, event.title);
    query.bindValue(u":artists"_s, event.artists.join(QLatin1StringView{Constants::UnitSeparator}));
    query.bindValue(u":album"_s, event.album);
    query.bindValue(u":albumArtists"_s, event.albumArtists.join(QLatin1StringView{Constants::UnitSeparator}));
    query.bindValue(u":genres"_s, event.genres.join(QLatin1StringView{Constants::UnitSeparator}));
    query.bindValue(u":composers"_s, event.composers.join(QLatin1StringView{Constants::UnitSeparator}));
    query.bindValue(u":date"_s, event.date);
    query.bindValue(u":year"_s, event.year);
    query.bindValue(u":trackNumber"_s, event.trackNumber);
    query.bindValue(u":discNumber"_s, event.discNumber);
    query.bindValue(u":filepath"_s, event.filepath);
    query.bindValue(u":durationMs"_s, QVariant::fromValue(event.durationMs));
    query.bindValue(u":codec"_s, event.codec);
    query.bindValue(u":codecProfile"_s, event.codecProfile);
    query.bindValue(u":encoding"_s, event.encoding);
    query.bindValue(u":bitrate"_s, event.bitrate);
    query.bindValue(u":bitDepth"_s, event.bitDepth);
    query.bindValue(u":sampleRate"_s, event.sampleRate);
    if(!query.exec()) {
        return -1;
    }

    return query.lastInsertId().toLongLong();
}

bool PlaybackHistoryDatabase::checkpointEvent(const PlaybackEvent& event) const
{
    DbTransaction transaction{db()};
    if(!transaction) {
        return false;
    }

    static const QString dailyStatement = uR"(
        INSERT INTO PlaybackDailyTotals (SourceID, Day, ListeningTimeMs, LastListenedAt)
        SELECT SourceID, :day, :listeningTimeMs - ListeningTimeMs, :lastCheckpointAt
        FROM PlaybackEvents
        WHERE EventID = :eventId AND EndedAt IS NULL AND :listeningTimeMs > ListeningTimeMs
        ON CONFLICT(SourceID, Day) DO UPDATE SET
            ListeningTimeMs = PlaybackDailyTotals.ListeningTimeMs + excluded.ListeningTimeMs,
            LastListenedAt = MAX(PlaybackDailyTotals.LastListenedAt, excluded.LastListenedAt);
    )"_s;

    DbQuery dailyQuery{db(), dailyStatement};
    dailyQuery.bindValue(u":eventId"_s, event.id);
    dailyQuery.bindValue(u":day"_s, listeningDay(event.lastCheckpointAt));
    dailyQuery.bindValue(u":lastCheckpointAt"_s, event.lastCheckpointAt);
    dailyQuery.bindValue(u":listeningTimeMs"_s, QVariant::fromValue(event.listeningTimeMs));
    if(!dailyQuery.exec()) {
        return false;
    }

    static const QString statement = uR"(
        UPDATE PlaybackEvents SET
            LastCheckpointAt = :lastCheckpointAt,
            ListeningTimeMs = :listeningTimeMs,
            PlayedAt = COALESCE(PlayedAt, :playedAt)
        WHERE EventID = :eventId AND EndedAt IS NULL AND :listeningTimeMs >= ListeningTimeMs;
    )"_s;

    DbQuery query{db(), statement};
    bindEventProgress(query, event);
    return query.exec() && query.numRowsAffected() == 1 && transaction.commit();
}

bool PlaybackHistoryDatabase::finaliseEvent(const PlaybackEvent& event) const
{
    if(!event.endedAt) {
        return false;
    }

    DbTransaction transaction{db()};
    if(!transaction || !checkpointEvent(event)) {
        return false;
    }

    // Close spans and finalise their event together so crash recovery sees a consistent boundary
    DbQuery closeSpans{db(), u"UPDATE StreamMetadataSpans "
                             "SET EndedOffsetMs = :listeningTimeMs "
                             "WHERE EventID = :eventId AND EndedOffsetMs IS NULL "
                             "AND StartedOffsetMs <= :listeningTimeMs;"_s};
    closeSpans.bindValue(u":eventId"_s, event.id);
    closeSpans.bindValue(u":listeningTimeMs"_s, QVariant::fromValue(event.listeningTimeMs));
    if(!closeSpans.exec()) {
        return false;
    }

    static const QString statement = u"UPDATE PlaybackEvents SET EndedAt = :endedAt "
                                     "WHERE EventID = :eventId AND EndedAt IS NULL;"_s;

    DbQuery query{db(), statement};
    query.bindValue(u":eventId"_s, event.id);
    query.bindValue(u":endedAt"_s, *event.endedAt);
    return query.exec() && query.numRowsAffected() == 1 && transaction.commit();
}

std::optional<PlaybackEvent> PlaybackHistoryDatabase::event(const qint64 eventId) const
{
    static const QString statement
        = u"SELECT EventID, SourceID, StartedAt, EndedAt, LastCheckpointAt, ListeningTimeMs, PlayedAt, %1 "
          "FROM PlaybackEvents AS Events WHERE EventID = :eventId;"_s.arg(eventMetadataColumns());

    DbQuery query{db(), statement};
    query.bindValue(u":eventId"_s, eventId);
    if(!query.exec() || !query.next()) {
        return {};
    }

    return eventFromQuery(query);
}

std::vector<PlaybackEvent> PlaybackHistoryDatabase::openEvents() const
{
    static const QString statement
        = u"SELECT EventID, SourceID, StartedAt, EndedAt, LastCheckpointAt, ListeningTimeMs, PlayedAt, %1 "
          "FROM PlaybackEvents AS Events WHERE EndedAt IS NULL ORDER BY StartedAt;"_s.arg(eventMetadataColumns());

    DbQuery query{db(), statement};
    if(!query.exec()) {
        return {};
    }

    std::vector<PlaybackEvent> events;
    while(query.next()) {
        events.push_back(eventFromQuery(query));
    }
    return events;
}

qint64 PlaybackHistoryDatabase::insertMetadataSpan(const StreamMetadataSpan& span) const
{
    if(span.eventId < 0) {
        return -1;
    }

    static const QString statement = uR"(
        INSERT INTO StreamMetadataSpans (
            EventID, StartedOffsetMs, EndedOffsetMs, RawStreamTitle, Title, Artists, Album
        ) VALUES (
            :eventId, :startedOffsetMs, :endedOffsetMs, :rawStreamTitle, :title, :artists, :album
        );
    )"_s;

    DbQuery query{db(), statement};
    query.bindValue(u":eventId"_s, span.eventId);
    query.bindValue(u":startedOffsetMs"_s, QVariant::fromValue(span.startedOffsetMs));
    query.bindValue(u":endedOffsetMs"_s, span.endedOffsetMs ? QVariant::fromValue(*span.endedOffsetMs) : QVariant{});
    query.bindValue(u":rawStreamTitle"_s, span.rawStreamTitle);
    query.bindValue(u":title"_s, span.title);
    query.bindValue(u":artists"_s, span.artists.join(QLatin1StringView{Constants::UnitSeparator}));
    query.bindValue(u":album"_s, span.album);
    if(!query.exec()) {
        return -1;
    }

    return query.lastInsertId().toLongLong();
}

bool PlaybackHistoryDatabase::closeMetadataSpan(const qint64 spanId, const uint64_t endedOffsetMs) const
{
    static const QString statement = u"UPDATE StreamMetadataSpans SET EndedOffsetMs = :endedOffsetMs "
                                     "WHERE SpanID = :spanId AND EndedOffsetMs IS NULL "
                                     "AND :endedOffsetMs >= StartedOffsetMs;"_s;

    DbQuery query{db(), statement};
    query.bindValue(u":spanId"_s, spanId);
    query.bindValue(u":endedOffsetMs"_s, QVariant::fromValue(endedOffsetMs));
    return query.exec() && query.numRowsAffected() == 1;
}

std::vector<StreamMetadataSpan> PlaybackHistoryDatabase::metadataSpans(const qint64 eventId) const
{
    static const QString statement
        = u"SELECT SpanID, EventID, StartedOffsetMs, EndedOffsetMs, RawStreamTitle, Title, Artists, Album "
          "FROM StreamMetadataSpans WHERE EventID = :eventId ORDER BY StartedOffsetMs, SpanID;"_s;

    DbQuery query{db(), statement};
    query.bindValue(u":eventId"_s, eventId);
    if(!query.exec()) {
        return {};
    }

    std::vector<StreamMetadataSpan> spans;
    while(query.next()) {
        spans.push_back(spanFromQuery(query));
    }
    return spans;
}

PlaybackHistoryOverview PlaybackHistoryDatabase::overview(PlaybackHistoryScope scope, qint64 generatedAt,
                                                          const std::optional<PlaybackEvent>& liveEvent) const
{
    const QDateTime generated = QDateTime::fromMSecsSinceEpoch(generatedAt);
    const qint64 todayStart   = generated.date().startOfDay(generated.timeZone()).toMSecsSinceEpoch();
    const auto dayStart       = [&generated](int daysAgo) {
        return generated.date().addDays(-daysAgo).startOfDay(generated.timeZone()).toMSecsSinceEpoch();
    };
    const qint64 periodEnd = generated.date().addDays(1).startOfDay(generated.timeZone()).toMSecsSinceEpoch();

    PlaybackHistoryOverview overview;
    overview.scope      = scope;
    overview.lifetime   = totalsForPeriod(db(), scope, {}, periodEnd, liveEvent);
    overview.today      = totalsForPeriod(db(), scope, todayStart, periodEnd, liveEvent);
    overview.last7Days  = totalsForPeriod(db(), scope, dayStart(6), periodEnd, liveEvent);
    overview.last30Days = totalsForPeriod(db(), scope, dayStart(29), periodEnd, liveEvent);
    return overview;
}

PlaybackHistoryPage PlaybackHistoryDatabase::history(const PlaybackHistoryQuery& historyQuery,
                                                     const std::optional<PlaybackEvent>& liveEvent) const
{
    PlaybackHistoryPage page;
    PlaybackHistoryQuery query = historyQuery;
    query.filter               = query.filter.trimmed();
    query.limit                = std::clamp<uint64_t>(query.limit, 1, 500);

    QString countStatement = uR"(
        SELECT COUNT(*)
        FROM PlaybackEvents AS Events
        JOIN PlaybackSources AS Sources ON Sources.SourceID = Events.SourceID
        WHERE 1 = 1
    )"_s;
    appendHistoryConditions(countStatement, query);
    countStatement += u';';

    DbQuery countQuery{db(), countStatement};
    bindHistoryConditions(countQuery, query);
    if(countQuery.exec() && countQuery.next()) {
        page.totalCount = countQuery.value(0).toULongLong();
    }

    QString selectStatement = uR"(
        SELECT
            Sources.SourceID, Sources.Kind, Sources.StableKey, Sources.DisplayName, Sources.OriginalURI,
            (SELECT COUNT(*) FROM StreamMetadataSpans AS Spans WHERE Spans.EventID = Events.EventID),
            Events.EventID, Events.SourceID, Events.StartedAt, Events.EndedAt,
            Events.LastCheckpointAt, Events.ListeningTimeMs,
            Events.PlayedAt, %1
        FROM PlaybackEvents AS Events
        JOIN PlaybackSources AS Sources ON Sources.SourceID = Events.SourceID
        WHERE 1 = 1
    )"_s.arg(eventMetadataColumns());
    appendHistoryConditions(selectStatement, query);
    if(query.before) {
        selectStatement += u" AND (Events.StartedAt, Events.EventID) < (:beforeTime, :beforeId)"_s;
    }
    if(query.eventId) {
        selectStatement += u" AND Events.EventID = :eventId"_s;
    }
    selectStatement += u" ORDER BY Events.StartedAt DESC, Events.EventID DESC LIMIT :limit;"_s;

    DbQuery selectQuery{db(), selectStatement};
    bindHistoryConditions(selectQuery, query);
    if(query.before) {
        selectQuery.bindValue(u":beforeTime"_s, query.before->first);
        selectQuery.bindValue(u":beforeId"_s, query.before->second);
    }
    if(query.eventId) {
        selectQuery.bindValue(u":eventId"_s, *query.eventId);
    }
    selectQuery.bindValue(u":limit"_s, QVariant::fromValue(query.limit + 1));
    if(!selectQuery.exec()) {
        return page;
    }

    page.entries.reserve(std::min<uint64_t>(query.limit, page.totalCount));
    while(selectQuery.next()) {
        auto event = eventFromQuery(selectQuery, 6);
        if(liveEvent && event.id == liveEvent->id) {
            event.lastCheckpointAt = liveEvent->lastCheckpointAt;
            event.listeningTimeMs  = liveEvent->listeningTimeMs;
        }
        page.entries.push_back(PlaybackHistoryEntry{
            .source            = sourceFromQuery(selectQuery),
            .event             = std::move(event),
            .metadataSpanCount = selectQuery.value(5).toULongLong(),
        });
    }
    page.hasMore = page.entries.size() > query.limit;
    if(page.hasMore) {
        page.entries.pop_back();
    }
    return page;
}

std::vector<PlaybackSourceRanking>
PlaybackHistoryDatabase::sourceRankings(const PlaybackRankingQuery& rankingQuery,
                                        const std::optional<PlaybackEvent>& liveEvent) const
{
    if(rankingQuery.from && rankingQuery.to && *rankingQuery.from >= *rankingQuery.to) {
        return {};
    }

    QString statement = sourceActivityStatement() + uR"(
        SELECT
            SourceID, Kind, StableKey, DisplayName, OriginalURI, ListenCount, ListeningTimeMs, LastListenedAt
        FROM Activity
        WHERE (ListenCount > 0 OR ListeningTimeMs > 0)
    )"_s;

    switch(rankingQuery.scope) {
        case PlaybackHistoryScope::All:
            break;
        case PlaybackHistoryScope::LocalTracks:
            statement += u" AND Kind = 0"_s;
            break;
        case PlaybackHistoryScope::RemoteStreams:
            statement += u" AND Kind = 1"_s;
            break;
    }
    if(rankingQuery.sourceId) {
        statement += u" AND SourceID = :sourceId"_s;
    }
    statement += uR"(
        ORDER BY ListeningTimeMs DESC, ListenCount DESC, LastListenedAt DESC, SourceID
        LIMIT :limit;
    )"_s;

    DbQuery query{db(), statement};
    const std::optional<qint64> from
        = rankingQuery.from
            ? std::optional{QDateTime::fromMSecsSinceEpoch(*rankingQuery.from).date().startOfDay().toMSecsSinceEpoch()}
            : std::nullopt;
    const std::optional<qint64> to = rankingQuery.to
                                       ? std::optional{QDateTime::fromMSecsSinceEpoch(*rankingQuery.to - 1)
                                                           .date()
                                                           .addDays(1)
                                                           .startOfDay()
                                                           .toMSecsSinceEpoch()}
                                       : std::nullopt;
    bindActivityPeriod(query, from, to, liveEvent);
    if(rankingQuery.sourceId) {
        query.bindValue(u":sourceId"_s, *rankingQuery.sourceId);
    }
    query.bindValue(u":limit"_s, QVariant::fromValue(std::clamp<uint64_t>(rankingQuery.limit, 1, 500)));
    if(!query.exec()) {
        return {};
    }

    std::vector<PlaybackSourceRanking> rankings;
    while(query.next()) {
        rankings.push_back(PlaybackSourceRanking{
            .source          = sourceFromQuery(query),
            .listenCount     = query.value(5).toULongLong(),
            .listeningTimeMs = query.value(6).toULongLong(),
            .lastListenedAt  = query.value(7).toLongLong(),
        });
    }
    return rankings;
}

LibraryStatistics PlaybackHistoryDatabase::libraryStatistics() const
{
    LibraryStatistics statistics;

    DbQuery totals{db(),
                   u"SELECT COUNT(*), COALESCE(SUM(Tracks.Duration), 0), COALESCE(SUM(Tracks.FileSize), 0), "
                   "COALESCE(SUM(TrackStats.PlayCount), 0), "
                   "AVG(CASE WHEN TrackStats.Rating > 0 AND TrackStats.Rating <= 1 THEN TrackStats.Rating END), "
                   "COALESCE(SUM(CASE WHEN TrackStats.Rating > 0 AND TrackStats.Rating <= 1 THEN 0 ELSE 1 END), 0) "
                   "FROM Tracks LEFT JOIN TrackStats ON Tracks.StatisticsKey = TrackStats.StatisticsKey "
                   "WHERE Tracks.LibraryID >= 0;"_s};
    if(totals.exec() && totals.next()) {
        statistics.trackCount      = totals.value(0).toULongLong();
        statistics.totalDurationMs = totals.value(1).toULongLong();
        statistics.totalFileSize   = totals.value(2).toULongLong();
        statistics.totalPlayCount  = totals.value(3).toULongLong();
        if(!totals.value(4).isNull()) {
            statistics.averageRating = totals.value(4).toDouble();
        }
        statistics.unratedTrackCount = totals.value(5).toULongLong();
    }

    statistics.formats
        = libraryGroups(db(), u"COALESCE(Tracks.Codec, '')"_s, u"COUNT(*) DESC, GroupName COLLATE NOCASE"_s);
    statistics.bitDepths
        = libraryGroups(db(), u"CASE WHEN Tracks.BitDepth > 0 THEN Tracks.BitDepth ELSE 0 END"_s, u"GroupName"_s);
    statistics.sampleRates
        = libraryGroups(db(), u"CASE WHEN Tracks.SampleRate > 0 THEN Tracks.SampleRate ELSE 0 END"_s, u"GroupName"_s);
    statistics.folders = libraryGroups(db(), u"COALESCE(Libraries.Path, '')"_s, u"GroupName COLLATE NOCASE"_s);
    // Group by the displayed star value so stored floating point differences don't create duplicate rows
    statistics.ratings = libraryGroups(db(),
                                       u"CASE WHEN TrackStats.Rating > 0 AND TrackStats.Rating <= 1 "
                                       "THEN ROUND(TrackStats.Rating * 5, 2) ELSE NULL END"_s,
                                       u"GroupName DESC"_s);
    return statistics;
}
} // namespace Fooyin
