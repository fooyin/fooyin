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

#include <core/library/tracksort.h>

namespace Fooyin {
TrackSorter::TrackSorter()
    : TrackSorter{nullptr}
{ }

TrackSorter::TrackSorter(LibraryManager* libraryManager)
    : m_parser{new ScriptRegistry(libraryManager)}
{ }

TrackSorter::~TrackSorter() = default;

TrackList TrackSorter::calcSortFields(const QString& sort, const TrackList& tracks)
{
    return calcSortFields(parseScript(sort), tracks);
}

TrackList TrackSorter::calcSortFields(const ParsedScript& sortScript, const TrackList& tracks)
{
    const std::scoped_lock lock{m_parserGuard};

    TrackList calcTracks{tracks};
    for(Track& track : calcTracks) {
        track.setSort(m_parser.evaluate(sortScript, track));
    }
    return calcTracks;
}

TrackList TrackSorter::sortTracks(const TrackList& tracks, Qt::SortOrder order)
{
    TrackList sortedTracks{tracks};
    sortTracks(sortedTracks, std::identity{}, order);
    return sortedTracks;
}

TrackList TrackSorter::calcSortTracks(const QString& sort, const TrackList& tracks, Qt::SortOrder order)
{
    return calcSortTracks(parseScript(sort), tracks, order);
}

TrackList TrackSorter::calcSortTracks(const QString& sort, const TrackList& tracks, const std::vector<int>& indexes,
                                      Qt::SortOrder order)
{
    return calcSortTracks(parseScript(sort), tracks, indexes, order);
}

TrackList TrackSorter::calcSortTracks(const ParsedScript& sortScript, const TrackList& tracks, Qt::SortOrder order)
{
    const TrackList calcTracks = calcSortFields(sortScript, tracks);
    return sortTracks(calcTracks, order);
}

TrackList TrackSorter::calcSortTracks(const ParsedScript& sortScript, const TrackList& tracks,
                                      const std::vector<int>& indexes, Qt::SortOrder order)
{
    TrackList sortedTracks{tracks};
    TrackList tracksToSort;

    auto validIndexes = indexes | std::views::filter([&tracks](int index) {
                            return (index >= 0 && index < static_cast<int>(tracks.size()));
                        });

    for(const int index : validIndexes) {
        tracksToSort.push_back(tracks.at(index));
    }

    const TrackList calcTracks      = calcSortFields(sortScript, tracksToSort);
    const TrackList sortedSubTracks = sortTracks(calcTracks, order);

    for(auto i{0}; const int index : validIndexes) {
        sortedTracks[index] = sortedSubTracks.at(i++);
    }

    return sortedTracks;
}

ParsedScript TrackSorter::parseScript(const QString& sort)
{
    const std::scoped_lock lock{m_parserGuard};
    return m_parser.parse(sort);
}
} // namespace Fooyin
