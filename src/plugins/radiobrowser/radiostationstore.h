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

#include "radiobrowserdatabase.h"
#include "radiostation.h"

#include <utils/database/dbconnectionhandler.h>
#include <utils/database/dbconnectionpool.h>

#include <QObject>

#include <optional>
#include <set>

namespace Fooyin::RadioBrowser {
enum class SavedStationChange
{
    Contents,
    Order
};

enum class SavedStationImportMode
{
    Append,
    Replace
};

struct SavedStationImportResult
{
    int imported{0};
    int skipped{0};
};

class RadioStationStore : public QObject
{
    Q_OBJECT

public:
    explicit RadioStationStore(DbConnectionPoolPtr dbPool, QObject* parent = nullptr);

    [[nodiscard]] RadioStationList savedStations() const;
    [[nodiscard]] RadioSavedSearchList savedSearches() const;
    [[nodiscard]] bool isSaved(const RadioStation& station) const;
    [[nodiscard]] std::optional<RadioSavedSearch> savedSearchForRequest(const RadioSearchRequest& request) const;

    void saveStation(const RadioStation& station);
    void updateStation(const RadioStation& previousStation, const RadioStation& station);
    void removeStation(const RadioStation& station);
    void reorderStations(const RadioStationList& stations);
    [[nodiscard]] SavedStationImportResult importStations(const RadioStationList& stations,
                                                          SavedStationImportMode mode);
    void recordPlayed(const RadioStation& station);
    void saveSearch(const QString& name, const RadioSearchRequest& request);
    void renameSearch(const QString& id, const QString& name);
    void removeSearch(const QString& id);

Q_SIGNALS:
    void savedStationsChanged(const Fooyin::RadioBrowser::RadioStationList& stations,
                              Fooyin::RadioBrowser::SavedStationChange change);
    void savedSearchesChanged(const Fooyin::RadioBrowser::RadioSavedSearchList& searches);
    void stationStatsChanged(const Fooyin::RadioBrowser::RadioStation& station);

private:
    void load();
    bool storeStations() const;
    bool storeSearches() const;

    DbConnectionPoolPtr m_dbPool;
    DbConnectionHandler m_dbHandler;
    RadioBrowserDatabase m_database;
    RadioStationList m_savedStations;
    RadioSavedSearchList m_savedSearches;
    std::set<QString> m_savedKeys;
};
} // namespace Fooyin::RadioBrowser

Q_DECLARE_METATYPE(Fooyin::RadioBrowser::SavedStationImportResult)
