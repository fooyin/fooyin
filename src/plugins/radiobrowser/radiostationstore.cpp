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

#include "radiostationstore.h"

#include <utils/database/dbconnectionprovider.h>
#include <utils/helpers.h>
#include <utils/id.h>

#include <QDateTime>

using namespace Qt::StringLiterals;

namespace Fooyin::RadioBrowser {
namespace {
QString stationKey(const RadioStation& station)
{
    if(!station.uuid.isEmpty()) {
        return u"uuid:%1"_s.arg(station.uuid);
    }

    const QString url = station.effectiveStreamUrl();
    return url.isEmpty() ? QString{} : u"url:%1"_s.arg(url);
}

std::vector<RadioBrowserDatabase::StationEntry> stationEntries(const RadioStationList& stations)
{
    std::vector<RadioBrowserDatabase::StationEntry> entries;
    entries.reserve(stations.size());

    for(const RadioStation& station : stations) {
        entries.emplace_back(stationKey(station), station);
    }

    return entries;
}

QString uniqueSavedSearchName(const QString& name, const RadioSavedSearchList& searches, const QString& ignoreId = {})
{
    RadioSavedSearchList candidates;
    candidates.reserve(searches.size());

    for(const RadioSavedSearch& search : searches) {
        if(search.id != ignoreId) {
            candidates.push_back(search);
        }
    }

    return Utils::findUniqueString(name.trimmed(), candidates,
                                   [](const RadioSavedSearch& search) { return search.name; });
}
} // namespace

RadioStationStore::RadioStationStore(DbConnectionPoolPtr dbPool, QObject* parent)
    : QObject{parent}
    , m_dbPool{std::move(dbPool)}
    , m_dbHandler{m_dbPool}
{
    m_database.initialise(DbConnectionProvider{m_dbPool});
    m_database.initialiseDatabase();
    load();
}

RadioStationList RadioStationStore::savedStations() const
{
    return m_savedStations;
}

RadioSavedSearchList RadioStationStore::savedSearches() const
{
    return m_savedSearches;
}

bool RadioStationStore::isSaved(const RadioStation& station) const
{
    return m_savedKeys.contains(stationKey(station));
}

std::optional<RadioSavedSearch> RadioStationStore::savedSearchForRequest(const RadioSearchRequest& request) const
{
    for(const RadioSavedSearch& search : m_savedSearches) {
        if(sameSavedSearchRequest(search.request, request)) {
            return search;
        }
    }
    return {};
}

void RadioStationStore::saveStation(const RadioStation& station)
{
    const QString key = stationKey(station);
    if(key.isEmpty() || m_savedKeys.contains(key)) {
        return;
    }

    m_savedStations.emplace_back(station);
    m_savedKeys.emplace(key);
    storeStations();
    Q_EMIT savedStationsChanged(m_savedStations, SavedStationChange::Contents);
}

void RadioStationStore::updateStation(const RadioStation& previousStation, const RadioStation& station)
{
    const QString previousKey = stationKey(previousStation);
    const QString key         = stationKey(station);
    if(previousKey.isEmpty() || key.isEmpty()) {
        return;
    }

    for(RadioStation& savedStation : m_savedStations) {
        if(stationKey(savedStation) == previousKey) {
            savedStation = station;
            m_savedKeys.erase(previousKey);
            m_savedKeys.emplace(key);
            storeStations();
            Q_EMIT savedStationsChanged(m_savedStations, SavedStationChange::Contents);
            return;
        }
    }

    saveStation(station);
}

void RadioStationStore::removeStation(const RadioStation& station)
{
    const QString key = stationKey(station);
    if(key.isEmpty() || !m_savedKeys.contains(key)) {
        return;
    }

    std::erase_if(m_savedStations,
                  [&key](const RadioStation& savedStation) { return stationKey(savedStation) == key; });
    m_savedKeys.erase(key);
    storeStations();
    Q_EMIT savedStationsChanged(m_savedStations, SavedStationChange::Contents);
}

void RadioStationStore::reorderStations(const RadioStationList& stations)
{
    if(stations.size() != m_savedStations.size()) {
        return;
    }

    std::set<QString> keys;
    for(const RadioStation& station : stations) {
        const QString key = stationKey(station);
        if(key.isEmpty() || !m_savedKeys.contains(key) || keys.contains(key)) {
            return;
        }
        keys.emplace(key);
    }

    m_savedStations = stations;
    storeStations();
    Q_EMIT savedStationsChanged(m_savedStations, SavedStationChange::Order);
}

SavedStationImportResult RadioStationStore::importStations(const RadioStationList& stations,
                                                           SavedStationImportMode mode)
{
    SavedStationImportResult result;

    if(mode == SavedStationImportMode::Replace) {
        m_savedStations.clear();
        m_savedKeys.clear();
    }

    for(const RadioStation& station : stations) {
        const QString key = stationKey(station);
        if(key.isEmpty() || m_savedKeys.contains(key)) {
            ++result.skipped;
            continue;
        }

        m_savedStations.emplace_back(station);
        m_savedKeys.emplace(key);
        ++result.imported;
    }

    if(result.imported <= 0 && mode != SavedStationImportMode::Replace) {
        return result;
    }

    storeStations();
    Q_EMIT savedStationsChanged(m_savedStations, SavedStationChange::Contents);
    return result;
}

void RadioStationStore::recordPlayed(const RadioStation& station)
{
    const QString key = stationKey(station);
    if(key.isEmpty()) {
        return;
    }

    RadioStation updatedStation = station;

    for(RadioStation& savedStation : m_savedStations) {
        if(stationKey(savedStation) == key) {
            ++savedStation.playCount;
            savedStation.lastPlayed = QDateTime::currentMSecsSinceEpoch();
            updatedStation          = savedStation;
            m_database.updateStationStats(key, savedStation);
            Q_EMIT savedStationsChanged(m_savedStations, SavedStationChange::Contents);
            break;
        }
    }

    Q_EMIT stationStatsChanged(updatedStation);
}

void RadioStationStore::saveSearch(const QString& name, const RadioSearchRequest& request)
{
    const QString uniqueName = uniqueSavedSearchName(name, m_savedSearches);
    if(uniqueName.isEmpty() || isDefaultSearchRequest(request)) {
        return;
    }

    for(RadioSavedSearch& search : m_savedSearches) {
        if(sameSavedSearchRequest(search.request, request)) {
            const QString updatedName = uniqueSavedSearchName(name, m_savedSearches, search.id);
            if(updatedName.isEmpty()) {
                return;
            }
            search.name = updatedName;
            storeSearches();
            Q_EMIT savedSearchesChanged(m_savedSearches);
            return;
        }
    }

    RadioSavedSearch search;
    search.id             = QUuid::createUuid().toString(QUuid::WithoutBraces);
    search.name           = uniqueName;
    search.request        = request;
    search.request.offset = 0;
    search.request.limit  = 0;

    m_savedSearches.emplace_back(std::move(search));
    storeSearches();
    Q_EMIT savedSearchesChanged(m_savedSearches);
}

void RadioStationStore::renameSearch(const QString& id, const QString& name)
{
    const QString uniqueName = uniqueSavedSearchName(name, m_savedSearches, id);
    if(id.isEmpty() || uniqueName.isEmpty()) {
        return;
    }

    for(RadioSavedSearch& search : m_savedSearches) {
        if(search.id == id) {
            search.name = uniqueName;
            storeSearches();
            Q_EMIT savedSearchesChanged(m_savedSearches);
            return;
        }
    }
}

void RadioStationStore::removeSearch(const QString& id)
{
    if(id.isEmpty()) {
        return;
    }

    const auto previousSize = m_savedSearches.size();
    std::erase_if(m_savedSearches, [&id](const RadioSavedSearch& search) { return search.id == id; });
    if(m_savedSearches.size() == previousSize) {
        return;
    }

    storeSearches();
    Q_EMIT savedSearchesChanged(m_savedSearches);
}

void RadioStationStore::load()
{
    m_savedStations.clear();
    m_savedSearches.clear();
    m_savedKeys.clear();

    RadioStationList stations = m_database.savedStations();
    m_savedStations.reserve(stations.size());

    for(RadioStation& station : stations) {
        const QString key = stationKey(station);
        if(key.isEmpty() || m_savedKeys.contains(key)) {
            continue;
        }

        m_savedKeys.emplace(key);
        m_savedStations.emplace_back(std::move(station));
    }

    RadioSavedSearchList searches = m_database.savedSearches();
    m_savedSearches.reserve(searches.size());

    std::set<QString> searchIds;
    for(RadioSavedSearch& search : searches) {
        if(search.id.isEmpty()) {
            search.id = UId::create().toString(QUuid::WithoutBraces);
        }
        if(search.name.trimmed().isEmpty() || searchIds.contains(search.id) || isDefaultSearchRequest(search.request)) {
            continue;
        }

        searchIds.emplace(search.id);
        m_savedSearches.emplace_back(std::move(search));
    }
}

bool RadioStationStore::storeStations() const
{
    return m_database.replaceSavedStations(stationEntries(m_savedStations));
}

bool RadioStationStore::storeSearches() const
{
    return m_database.replaceSavedSearches(m_savedSearches);
}
} // namespace Fooyin::RadioBrowser
