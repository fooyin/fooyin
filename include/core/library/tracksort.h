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

#include <core/scripting/scriptparser.h>
#include <core/track.h>

#include <QCollator>
#include <QString>

#include <mutex>
#include <ranges>

namespace Fooyin {
class LibraryManager;
struct ParsedScript;
class TrackSorterPrivate;

class FYCORE_EXPORT TrackSorter
{
public:
    TrackSorter();
    explicit TrackSorter(LibraryManager* libraryManager);
    ~TrackSorter();

    /*!
     * Calculates the sort fields @p tracks using the @p sort script
     * @param sort the sort script as a string
     * @param tracks the tracks to calculate
     * @returns a new TrackList with the calculated sortFields
     */
    TrackList calcSortFields(const QString& sort, const TrackList& tracks);

    /*!
     * Calculates the sort fields of @p tracks using the parsed @p sort script
     * @param sortScript the parsed sort script
     * @param tracks the tracks to calculate
     * @returns a new TrackList with the calculated sortFields
     */
    [[nodiscard]] TrackList calcSortFields(const ParsedScript& sortScript, const TrackList& tracks);

    /*!
     * Sorts @p tracks using their current sort fields
     * @param tracks the tracks to sort
     * @param order the order in which to sort the tracks
     * @returns a new sorted TrackList
     */
    static TrackList sortTracks(const TrackList& tracks, Qt::SortOrder order = Qt::AscendingOrder);

    /*!
     * Calculates the sort fields and then sorts @p tracks
     * @param sort the sort script as a string
     * @param tracks the tracks to sort
     * @param order the order in which to sort the tracks
     * @returns a new sorted TrackList
     */
    TrackList calcSortTracks(const QString& sort, const TrackList& tracks, Qt::SortOrder order = Qt::AscendingOrder);

    /*!
     * Calculates the sort fields and then sorts @p tracks in the given @p indexes.
     * @param sort the sort script as a string
     * @param tracks the tracks to sort
     * @param indexes the indexes to sort
     * @param order the order in which to sort the tracks
     * @returns a new sorted TrackList
     */
    TrackList calcSortTracks(const QString& sort, const TrackList& tracks, const std::vector<int>& indexes,
                             Qt::SortOrder order = Qt::AscendingOrder);

    /*!
     * Calculates the sort fields and then sorts @p tracks
     * @param sortScript the parsed sort script
     * @param tracks the tracks to sort
     * @param order the order in which to sort the tracks
     * @returns a new sorted TrackList
     */
    TrackList calcSortTracks(const ParsedScript& sortScript, const TrackList& tracks,
                             Qt::SortOrder order = Qt::AscendingOrder);

    /*!
     * Calculates the sort fields and then sorts @p tracks in the given @p indexes.
     * Tracks not under an index in @p indexes retain their position.
     * @param sortScript the parsed sort script
     * @param tracks the tracks to sort
     * @param indexes the indexes to sort
     * @param order the order in which to sort the tracks
     * @returns a new sorted TrackList
     */
    TrackList calcSortTracks(const ParsedScript& sortScript, const TrackList& tracks, const std::vector<int>& indexes,
                             Qt::SortOrder order = Qt::AscendingOrder);

    template <typename Container, typename Extractor>
    Container calcSortFields(const QString& sort, const Container& items, Extractor extractor)
    {
        Container calculatedTracks;
        calculatedTracks.reserve(items.size());

        const std::scoped_lock lock{m_parserGuard};

        for(const auto& item : items) {
            auto evalItem{item};
            Track& track = extractor(evalItem);
            track.setSort(m_parser.evaluate(sort, track));
            calculatedTracks.push_back(evalItem);
        }

        return calculatedTracks;
    }

    template <typename Container, typename SortExtractor, typename Extractor>
    Container calcSortTracks(const QString& sort, const Container& items, SortExtractor sortExtractor,
                             Extractor extractor, Qt::SortOrder order = Qt::AscendingOrder)
    {
        Container sortedTracks = calcSortFields(sort, items, sortExtractor);

        QCollator collator;
        collator.setNumericMode(true);

        std::ranges::stable_sort(sortedTracks, [order, collator, extractor](const auto& lhs, const auto& rhs) {
            const Track& leftTrack  = extractor(lhs);
            const Track& rightTrack = extractor(rhs);

            const auto cmp = collator.compare(leftTrack.sort(), rightTrack.sort());

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

private:
    ParsedScript parseScript(const QString& sort);

    ScriptParser m_parser;
    std::mutex m_parserGuard;
};
} // namespace Fooyin
