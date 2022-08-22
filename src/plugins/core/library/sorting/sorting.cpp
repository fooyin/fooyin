/*
 * Fooyin
 * Copyright 2022, Luke Taylor <LukeT1@proton.me>
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

#include "sorting.h"

#include "library/models/track.h"

#include <algorithm>

namespace Library {
bool tracksBase(Track* tr1, Track* tr2)
{
    if(tr1->discNumber() != tr2->discNumber()) {
        return tr1->discNumber() < tr2->discNumber();
    }
    return tr1->trackNumber() < tr2->trackNumber();
}

bool tracksByTitleAsc(Track* tr1, Track* tr2)
{
    if(tr1->album() != tr2->album()) {
        return tr1->album() < tr2->album();
    }
    return tracksBase(tr1, tr2);
}

bool tracksByTitleDesc(Track* tr1, Track* tr2)
{
    if(tr1->album() != tr2->album()) {
        return tr1->album() < tr2->album();
    }
    return tracksBase(tr1, tr2);
}

bool tracksByYearAsc(Track* tr1, Track* tr2)
{
    if(tr1->albumArtist() != tr2->albumArtist()) {
        return tr1->albumArtist() < tr2->albumArtist();
    }
    if(tr1->year() != tr2->year()) {
        return tr1->year() < tr2->year();
    }
    return tracksByTitleDesc(tr1, tr2);
}

bool tracksByYearDesc(Track* tr1, Track* tr2)
{
    if(tr1->albumArtist() != tr2->albumArtist()) {
        return tr1->albumArtist() < tr2->albumArtist();
    }
    if(tr1->year() != tr2->year()) {
        return tr1->year() > tr2->year();
    }
    return tracksByTitleDesc(tr1, tr2);
}

void sortTracks(TrackPtrList& tracks, SortOrder sortOrder)
{
    switch(sortOrder) {
        case(SortOrder::YearDesc):
            return std::sort(tracks.begin(), tracks.end(), tracksByYearDesc);
        case(SortOrder::YearAsc):
            return std::sort(tracks.begin(), tracks.end(), tracksByYearAsc);
        case(SortOrder::TitleDesc):
            return std::sort(tracks.begin(), tracks.end(), tracksByTitleDesc);
        case(SortOrder::TitleAsc):
            return std::sort(tracks.begin(), tracks.end(), tracksByTitleAsc);
        case(SortOrder::NoSorting):
            return std::sort(tracks.begin(), tracks.end(), tracksBase);
    }
}
} // namespace Library
