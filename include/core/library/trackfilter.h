/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
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

class QString;

namespace Fooyin::Filter {
/*!
 * Filters @p tracks using the @p search string
 *
 * Tracks are currently filtered on the following fields:
 * - Title
 * - Album
 * - Artist
 * - Album Artist
 * @note the search is case-insensitive
 * @param tracks the tracks to filter
 * @param search the search string
 * @returns a new TrackList containing the tracks which match @p search
 */
FYCORE_EXPORT TrackList filterTracks(const TrackList& tracks, const QString& search);
} // namespace Fooyin::Filter
