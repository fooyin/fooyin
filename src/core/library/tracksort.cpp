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

#include <core/scripting/scriptparser.h>
#include <core/track.h>

#include <mutex>
#include <ranges>

#include <QCollator>

namespace Fooyin {
class TrackSorterPrivate
{
public:
    explicit TrackSorterPrivate(LibraryManager* libraryManager)
        : m_registry{libraryManager}
        , m_parser{&m_registry}
    { }

    ParsedScript parseScript(const QString& sort);

    ScriptRegistry m_registry;
    ScriptParser m_parser;
    std::mutex m_parserGuard;
};

ParsedScript TrackSorterPrivate::parseScript(const QString& sort)
{
    const std::scoped_lock lock{m_parserGuard};
    return m_parser.parse(sort);
}

TrackSorter::TrackSorter()
    : TrackSorter{nullptr}
{ }

TrackSorter::TrackSorter(LibraryManager* libraryManager)
    : p{std::make_unique<TrackSorterPrivate>(libraryManager)}
{ }

TrackSorter::~TrackSorter() = default;

TrackList TrackSorter::calcSortFields(const QString& sort, const TrackList& tracks)
{
    return calcSortFields(p->parseScript(sort), tracks);
}

TrackList TrackSorter::calcSortFields(const ParsedScript& sortScript, const TrackList& tracks) const
{
    const std::scoped_lock lock{p->m_parserGuard};

    TrackList calcTracks{tracks};
    for(Track& track : calcTracks) {
        track.setSort(p->m_parser.evaluate(sortScript, track));
    }
    return calcTracks;
}

TrackList TrackSorter::sortTracks(const TrackList& tracks, Qt::SortOrder order)
{
    TrackList sortedTracks{tracks};

    QCollator collator;
    collator.setNumericMode(true);

    std::ranges::stable_sort(sortedTracks, [order, collator](const Track& lhs, const Track& rhs) {
        const auto cmp = collator.compare(lhs.sort(), rhs.sort());

        if(cmp == 0) {
            return false;
        }
        if(order == Qt::AscendingOrder) {
            return cmp < 0;
        }
        return cmp > 0;
    });
    return sortedTracks;
}

TrackList TrackSorter::calcSortTracks(const QString& sort, const TrackList& tracks, Qt::SortOrder order)
{
    return calcSortTracks(p->parseScript(sort), tracks, order);
}

TrackList TrackSorter::calcSortTracks(const QString& sort, const TrackList& tracks, const std::vector<int>& indexes,
                                      Qt::SortOrder order)
{
    return calcSortTracks(p->parseScript(sort), tracks, indexes, order);
}

TrackList TrackSorter::calcSortTracks(const ParsedScript& sortScript, const TrackList& tracks, Qt::SortOrder order) const
{
    const TrackList calcTracks = calcSortFields(sortScript, tracks);
    return sortTracks(calcTracks, order);
}

TrackList TrackSorter::calcSortTracks(const ParsedScript& sortScript, const TrackList& tracks,
                                      const std::vector<int>& indexes, Qt::SortOrder order) const
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
} // namespace Fooyin
