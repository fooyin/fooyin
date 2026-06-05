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

#include "radiobrowserdatabase.h"

#include <utils/database/dbquery.h>
#include <utils/database/dbtransaction.h>

#include <QJsonDocument>
#include <QJsonObject>

using namespace Qt::StringLiterals;

namespace Fooyin::RadioBrowser {
namespace {
void bindStation(DbQuery& query, const QString& key, const RadioStation& station, const int index)
{
    query.bindValue(u":stationKey"_s, key);
    query.bindValue(u":stationIndex"_s, index);
    query.bindValue(u":uuid"_s, station.uuid);
    query.bindValue(u":name"_s, station.name);
    query.bindValue(u":streamUrl"_s, station.streamUrl);
    query.bindValue(u":resolvedStreamUrl"_s, station.resolvedStreamUrl);
    query.bindValue(u":homepage"_s, station.homepage);
    query.bindValue(u":favicon"_s, station.favicon);
    query.bindValue(u":tags"_s, station.tags);
    query.bindValue(u":country"_s, station.country);
    query.bindValue(u":countryCode"_s, station.countryCode);
    query.bindValue(u":language"_s, station.language);
    query.bindValue(u":codec"_s, station.codec);
    query.bindValue(u":bitrate"_s, station.bitrate);
    query.bindValue(u":hls"_s, station.hls);
    query.bindValue(u":local"_s, station.local);
    query.bindValue(u":playCount"_s, station.playCount);
    query.bindValue(u":lastPlayed"_s, station.lastPlayed);
}

RadioStation stationFromQuery(DbQuery& query)
{
    RadioStation station;
    station.uuid              = query.value(0).toString();
    station.name              = query.value(1).toString();
    station.streamUrl         = query.value(2).toString();
    station.resolvedStreamUrl = query.value(3).toString();
    station.homepage          = query.value(4).toString();
    station.favicon           = query.value(5).toString();
    station.tags              = query.value(6).toString();
    station.country           = query.value(7).toString();
    station.countryCode       = query.value(8).toString();
    station.language          = query.value(9).toString();
    station.codec             = query.value(10).toString();
    station.bitrate           = query.value(11).toInt();
    station.hls               = query.value(12).toBool();
    station.local             = query.value(13).toBool();
    station.playCount         = query.value(14).toInt();
    station.lastPlayed        = query.value(15).toLongLong();
    return station;
}

QByteArray requestToJson(const RadioSearchRequest& request)
{
    return QJsonDocument{searchRequestToJson(request)}.toJson(QJsonDocument::Compact);
}
} // namespace

void RadioBrowserDatabase::initialiseDatabase() const
{
    DbQuery stations{db(), u"CREATE TABLE IF NOT EXISTS SavedStations ("
                           "Id INTEGER PRIMARY KEY, "
                           "StationKey TEXT UNIQUE NOT NULL, "
                           "StationIndex INTEGER NOT NULL, "
                           "Uuid TEXT, "
                           "Name TEXT NOT NULL, "
                           "StreamUrl TEXT, "
                           "ResolvedStreamUrl TEXT, "
                           "Homepage TEXT, "
                           "Favicon TEXT, "
                           "Tags TEXT, "
                           "Country TEXT, "
                           "CountryCode TEXT, "
                           "Language TEXT, "
                           "Codec TEXT, "
                           "Bitrate INTEGER, "
                           "Hls INTEGER NOT NULL DEFAULT 0, "
                           "Local INTEGER NOT NULL DEFAULT 0, "
                           "PlayCount INTEGER NOT NULL DEFAULT 0, "
                           "LastPlayed INTEGER NOT NULL DEFAULT 0);"_s};
    stations.exec();

    DbQuery stationIndex{db(), u"CREATE INDEX IF NOT EXISTS SavedStationsIndex "
                               "ON SavedStations (StationIndex);"_s};
    stationIndex.exec();

    DbQuery searches{db(), u"CREATE TABLE IF NOT EXISTS SavedSearches ("
                           "Id TEXT PRIMARY KEY, "
                           "Name TEXT NOT NULL, "
                           "RequestJson BLOB NOT NULL, "
                           "SearchIndex INTEGER NOT NULL);"_s};
    searches.exec();

    DbQuery searchIndex{db(), u"CREATE INDEX IF NOT EXISTS SavedSearchesIndex "
                              "ON SavedSearches (SearchIndex);"_s};
    searchIndex.exec();
}

RadioStationList RadioBrowserDatabase::savedStations() const
{
    DbQuery query{db(), u"SELECT Uuid, Name, StreamUrl, ResolvedStreamUrl, Homepage, Favicon, Tags, Country, "
                        "CountryCode, Language, Codec, Bitrate, Hls, Local, PlayCount, LastPlayed "
                        "FROM SavedStations ORDER BY StationIndex;"_s};

    if(!query.exec()) {
        return {};
    }

    RadioStationList stations;

    while(query.next()) {
        stations.emplace_back(stationFromQuery(query));
    }

    return stations;
}

RadioSavedSearchList RadioBrowserDatabase::savedSearches() const
{
    DbQuery query{db(), u"SELECT Id, Name, RequestJson FROM SavedSearches ORDER BY SearchIndex;"_s};

    if(!query.exec()) {
        return {};
    }

    RadioSavedSearchList searches;

    while(query.next()) {
        RadioSavedSearch search;
        search.id      = query.value(0).toString();
        search.name    = query.value(1).toString();
        search.request = searchRequestFromJson(QJsonDocument::fromJson(query.value(2).toByteArray()).object());
        searches.emplace_back(std::move(search));
    }

    return searches;
}

bool RadioBrowserDatabase::replaceSavedStations(const std::vector<StationEntry>& stations) const
{
    DbTransaction transaction{db()};

    DbQuery clear{db(), u"DELETE FROM SavedStations;"_s};
    if(!clear.exec()) {
        return false;
    }

    const auto statement = u"INSERT INTO SavedStations ("
                           "StationKey, StationIndex, Uuid, Name, StreamUrl, ResolvedStreamUrl, Homepage, Favicon, "
                           "Tags, Country, CountryCode, Language, Codec, Bitrate, Hls, Local, PlayCount, LastPlayed"
                           ") VALUES ("
                           ":stationKey, :stationIndex, :uuid, :name, :streamUrl, :resolvedStreamUrl, :homepage, "
                           ":favicon, :tags, :country, :countryCode, :language, :codec, :bitrate, :hls, :local, "
                           ":playCount, :lastPlayed);"_s;

    for(int i{0}; const auto& [key, station] : stations) {
        DbQuery insert{db(), statement};
        bindStation(insert, key, station, i++);
        if(!insert.exec()) {
            return false;
        }
    }

    return transaction.commit();
}

bool RadioBrowserDatabase::updateStationStats(const QString& stationKey, const RadioStation& station) const
{
    DbQuery query{db(), u"UPDATE SavedStations SET PlayCount = :playCount, LastPlayed = :lastPlayed "
                        "WHERE StationKey = :stationKey;"_s};

    query.bindValue(u":stationKey"_s, stationKey);
    query.bindValue(u":playCount"_s, station.playCount);
    query.bindValue(u":lastPlayed"_s, station.lastPlayed);

    return query.exec();
}

bool RadioBrowserDatabase::replaceSavedSearches(const RadioSavedSearchList& searches) const
{
    DbTransaction transaction{db()};

    DbQuery clear{db(), u"DELETE FROM SavedSearches;"_s};
    if(!clear.exec()) {
        return false;
    }

    const auto statement = u"INSERT INTO SavedSearches (Id, Name, RequestJson, SearchIndex) "
                           "VALUES (:id, :name, :requestJson, :searchIndex);"_s;

    for(int i{0}; const auto& search : searches) {
        DbQuery insert{db(), statement};
        insert.bindValue(u":id"_s, search.id);
        insert.bindValue(u":name"_s, search.name);
        insert.bindValue(u":requestJson"_s, requestToJson(search.request));
        insert.bindValue(u":searchIndex"_s, i++);
        if(!insert.exec()) {
            return false;
        }
    }

    return transaction.commit();
}
} // namespace Fooyin::RadioBrowser
