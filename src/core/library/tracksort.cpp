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
    : m_scriptEnvironment{libraryManager}
{ }

TrackSorter::~TrackSorter() = default;

TrackList TrackSorter::calcSortTracks(const QString& sort, TrackList tracks, Qt::SortOrder order)
{
    return calcSortTracks(parseScript(sort), std::move(tracks), order);
}

TrackList TrackSorter::calcSortTracks(const QString& sort, TrackList tracks, const std::vector<int>& indexes,
                                      Qt::SortOrder order)
{
    return calcSortTracks(parseScript(sort), std::move(tracks), indexes, order);
}

TrackList TrackSorter::calcSortTracks(const ParsedScript& sortScript, TrackList tracks, Qt::SortOrder order)
{
    auto sortEntries = calcOwnedSortEntries(sortScript, std::move(tracks), std::identity{});
    sortSortEntries(sortEntries, order);
    return stripSortEntries<TrackList>(std::move(sortEntries));
}

TrackList TrackSorter::calcSortTracks(const ParsedScript& sortScript, TrackList tracks, const std::vector<int>& indexes,
                                      Qt::SortOrder order)
{
    TrackList sortedTracks{std::move(tracks)};
    TrackList tracksToSort;
    tracksToSort.reserve(indexes.size());

    auto validIndexes = indexes | std::views::filter([&sortedTracks](int index) {
                            return (index >= 0 && index < static_cast<int>(sortedTracks.size()));
                        });

    for(const int index : validIndexes) {
        tracksToSort.push_back(sortedTracks.at(index));
    }

    auto sortEntries = calcOwnedSortEntries(sortScript, std::move(tracksToSort), std::identity{});
    sortSortEntries(sortEntries, order);
    auto sortedSubTracks = stripSortEntries<TrackList>(std::move(sortEntries));

    for(auto i{0}; const int index : validIndexes) {
        sortedTracks[index] = std::move(sortedSubTracks.at(i++));
    }

    return sortedTracks;
}

ParsedScript TrackSorter::parseScript(const QString& sort)
{
    const std::scoped_lock lock{m_parserGuard};
    return m_parser.parse(sort);
}
} // namespace Fooyin
