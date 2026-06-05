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

#include "radiostation.h"

#include <utils/database/dbmodule.h>

#include <QString>

#include <vector>

namespace Fooyin::RadioBrowser {
class RadioBrowserDatabase : public DbModule
{
public:
    void initialiseDatabase() const;

    [[nodiscard]] RadioStationList savedStations() const;
    [[nodiscard]] RadioSavedSearchList savedSearches() const;

    using StationEntry = std::pair<QString, RadioStation>;
    bool replaceSavedStations(const std::vector<StationEntry>& stations) const;
    bool updateStationStats(const QString& stationKey, const RadioStation& station) const;
    bool replaceSavedSearches(const RadioSavedSearchList& searches) const;
};
} // namespace Fooyin::RadioBrowser
