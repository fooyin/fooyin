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

#include "filterfwd.h"

#include <core/ratingsymbols.h>
#include <core/track.h>
#include <gui/scripting/richtext.h>
#include <utils/crypto.h>

#include <QFont>

namespace Fooyin {
class LibraryManager;

namespace Filters {
using RowKey = Md5Hash;

struct FilterRow
{
    RowKey key;
    QStringList columns;
    QStringList sortColumns;
    std::vector<RichText> richColumns;
    TrackIds trackIds;
};

using FilterRowList = std::vector<FilterRow>;

struct FilterRowBuildContext
{
    QFont font;
    RatingStarSymbols ratingSymbols;
    bool useVarious{false};
};

[[nodiscard]] FilterRowList buildFilterRows(LibraryManager* libraryManager, const FilterColumnList& columns,
                                            const TrackList& tracks, const FilterRowBuildContext& context);
[[nodiscard]] FilterRowList patchFilterRows(LibraryManager* libraryManager, const FilterColumnList& columns,
                                            const FilterRowList& previousRows, const TrackList& previousTracks,
                                            const TrackList& tracks, const TrackIds& changedTrackIds,
                                            const FilterRowBuildContext& context);
[[nodiscard]] TrackList filterTracksBySearch(const QString& search, const TrackList& tracks);
[[nodiscard]] FilterRowList filterRowsBySearch(const QString& search, const FilterRowList& rows,
                                               const TrackList& tracks);
} // namespace Filters
} // namespace Fooyin
