/*
 * Fooyin
 * Copyright 2022-2023, Luke Taylor <LukeT1@proton.me>
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

#include <core/trackfwd.h>

class QString;

namespace Fooyin {
struct ParsedScript;

namespace Sorting {
/*!
 * Calculates the sort fields @p tracks using the @p sort script
 * @param sort the sort script as a string
 * @param tracks the tracks to calculate
 * @returns a new TrackList with the calculated sortFields
 */
TrackList FYCORE_EXPORT calcSortFields(const QString& sort, const TrackList& tracks);

/*!
 * Calculates the sort fields of @p tracks using the parsed @p sort script
 * @param sortScript the parsed sort script
 * @param tracks the tracks to calculate
 * @returns a new TrackList with the calculated sortFields
 */
TrackList FYCORE_EXPORT calcSortFields(const ParsedScript& sortScript, const TrackList& tracks);

/*!
 * Sorts @p tracks using their current sort fields
 * @param tracks the tracks to sort
 * @returns a new sorted TrackList
 */
TrackList FYCORE_EXPORT sortTracks(const TrackList& tracks);

/*!
 * Calculates the sort fields and then sorts @p tracks
 * @param sort the sort script as a string
 * @param tracks the tracks to sort
 * @returns a new sorted TrackList
 */
TrackList FYCORE_EXPORT calcSortTracks(const QString& sort, const TrackList& tracks);

/*!
 * Calculates the sort fields and then sorts @p tracks
 * @param sortScript the parsed sort script
 * @param tracks the tracks to sort
 * @returns a new sorted TrackList
 */
TrackList FYCORE_EXPORT calcSortTracks(const ParsedScript& sortScript, const TrackList& tracks);
} // namespace Sorting
} // namespace Fooyin
