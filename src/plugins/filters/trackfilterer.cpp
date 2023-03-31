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

#include "trackfilterer.h"

#include <core/database/database.h>
#include <utils/helpers.h>

namespace Fy::Filters {
bool containsSearch(const QString& text, const QString& search)
{
    return text.contains(search, Qt::CaseInsensitive);
}

// TODO: Support user-defined tags
bool matchSearch(Core::Track* track, const QString& search)
{
    if(search.isEmpty()) {
        return true;
    }

    for(const auto& artist : track->artists()) {
        if(artist.contains(search, Qt::CaseInsensitive)) {
            return true;
        }
    }

    return (containsSearch(track->title(), search) || containsSearch(track->album(), search)
            || containsSearch(track->albumArtist(), search));
}

TrackFilterer::TrackFilterer(QObject* parent)
    : Worker{parent}
{ }

void TrackFilterer::filterTracks(const Core::TrackPtrList& tracks, const QString& search)
{
    const auto filteredTracks = Utils::filter(tracks, [search](Core::Track* track) {
        return matchSearch(track, search);
    });
    emit tracksFiltered(filteredTracks);
}
} // namespace Fy::Filters
