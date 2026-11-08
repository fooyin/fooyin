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

#include "playhistorydatabase.h"

#include <core/constants.h>
#include <utils/database/dbquery.h>

#include <QString>

using namespace Qt::StringLiterals;

namespace {
QString joinList(const QStringList& list)
{
    return list.join(QLatin1String{Fooyin::Constants::UnitSeparator});
}

QStringList splitList(const QString& text)
{
    return text.isEmpty() ? QStringList{} : text.split(QLatin1String{Fooyin::Constants::UnitSeparator});
}
} // namespace

namespace Fooyin {
bool PlayHistoryDatabase::insert(const PlayHistoryEntry& entry) const
{
    static const QString statement
        = u"INSERT INTO PlayHistory (Timestamp, Hash, Title, Artists, Album, AlbumArtists, Genres, Composers, "
          "Year, Duration, Bitrate, Lossless) "
          "VALUES (:timestamp, :hash, :title, :artists, :album, :albumArtists, :genres, :composers, "
          ":year, :duration, :bitrate, :lossless);"_s;

    DbQuery query{db(), statement};
    query.bindValue(u":timestamp"_s, entry.timestamp);
    query.bindValue(u":hash"_s, entry.hash);
    query.bindValue(u":title"_s, entry.title);
    query.bindValue(u":artists"_s, joinList(entry.artists));
    query.bindValue(u":album"_s, entry.album);
    query.bindValue(u":albumArtists"_s, joinList(entry.albumArtists));
    query.bindValue(u":genres"_s, joinList(entry.genres));
    query.bindValue(u":composers"_s, joinList(entry.composers));
    query.bindValue(u":year"_s, entry.year);
    query.bindValue(u":duration"_s, static_cast<qint64>(entry.duration));
    query.bindValue(u":bitrate"_s, entry.bitrate);
    query.bindValue(u":lossless"_s, entry.lossless);

    return query.exec();
}

PlayHistoryData PlayHistoryDatabase::load(qint64 from, qint64 to) const
{
    PlayHistoryData data;

    {
        static const QString statement
            = u"SELECT Timestamp, Hash, Title, Artists, Album, AlbumArtists, Genres, Composers, "
              "Year, Duration, Bitrate, Lossless FROM PlayHistory "
              "WHERE Timestamp >= :from AND Timestamp < :to ORDER BY Timestamp, EntryID;"_s;

        DbQuery query{db(), statement};
        query.bindValue(u":from"_s, from);
        query.bindValue(u":to"_s, to);

        if(!query.exec()) {
            return data;
        }

        while(query.next()) {
            PlayHistoryEntry entry;
            entry.timestamp    = query.value(0).toLongLong();
            entry.hash         = query.value(1).toString();
            entry.title        = query.value(2).toString();
            entry.artists      = splitList(query.value(3).toString());
            entry.album        = query.value(4).toString();
            entry.albumArtists = splitList(query.value(5).toString());
            entry.genres       = splitList(query.value(6).toString());
            entry.composers    = splitList(query.value(7).toString());
            entry.year         = query.value(8).toInt();
            entry.duration     = query.value(9).toULongLong();
            entry.bitrate      = query.value(10).toInt();
            entry.lossless     = query.value(11).toInt();
            data.entries.emplace_back(std::move(entry));
        }
    }

    {
        DbQuery query{db(), u"SELECT MIN(Timestamp) FROM PlayHistory;"_s};
        if(query.exec() && query.next()) {
            data.oldestTimestamp = query.value(0).toLongLong();
        }
    }

    {
        DbQuery query{db(), u"SELECT Hash, MIN(Timestamp) FROM PlayHistory GROUP BY Hash;"_s};
        if(query.exec()) {
            while(query.next()) {
                data.firstListenByHash.insert(query.value(0).toString(), query.value(1).toLongLong());
            }
        }
    }

    {
        static const QString statement
            = u"SELECT Hash, MAX(Timestamp) FROM PlayHistory WHERE Timestamp < :from GROUP BY Hash;"_s;

        DbQuery query{db(), statement};
        query.bindValue(u":from"_s, from);
        if(query.exec()) {
            while(query.next()) {
                data.lastListenBeforeByHash.insert(query.value(0).toString(), query.value(1).toLongLong());
            }
        }
    }

    {
        DbQuery query{db(), u"SELECT Artists, MIN(Timestamp) FROM PlayHistory GROUP BY Artists;"_s};
        if(query.exec()) {
            while(query.next()) {
                const qint64 timestamp = query.value(1).toLongLong();
                const auto artists     = splitList(query.value(0).toString());
                for(const QString& artist : artists) {
                    auto it = data.firstListenByArtist.find(artist);
                    if(it == data.firstListenByArtist.end()) {
                        data.firstListenByArtist.insert(artist, timestamp);
                    }
                    else if(timestamp < it.value()) {
                        it.value() = timestamp;
                    }
                }
            }
        }
    }

    return data;
}

PlayHistorySummary PlayHistoryDatabase::summary() const
{
    PlayHistorySummary summary;

    {
        DbQuery query{db(), u"SELECT COUNT(*), MIN(Timestamp), MAX(Timestamp), SUM(Duration) FROM PlayHistory;"_s};
        if(query.exec() && query.next()) {
            summary.totalListens    = query.value(0).toInt();
            summary.oldestTimestamp = query.value(1).toLongLong();
            summary.newestTimestamp = query.value(2).toLongLong();
            summary.totalListenMs   = query.value(3).toLongLong();
        }
    }

    if(summary.isEmpty()) {
        return summary;
    }

    static const QString statement
        = u"SELECT DISTINCT CAST(strftime('%Y', Timestamp / 1000, 'unixepoch', 'localtime') AS INTEGER) AS Year "
          "FROM PlayHistory ORDER BY Year;"_s;

    DbQuery query{db(), statement};
    if(query.exec()) {
        while(query.next()) {
            summary.years.push_back(query.value(0).toInt());
        }
    }

    return summary;
}
} // namespace Fooyin
