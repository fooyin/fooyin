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

#include <core/scripting/scriptenvironmenthelpers.h>
#include <core/scripting/scriptparser.h>
#include <core/track.h>
#include <utils/stringcollator.h>

#include <QString>

#include <mutex>
#include <ranges>
#include <vector>

namespace Fooyin {
class LibraryManager;
struct ParsedScript;
class TrackSorterPrivate;

class FYCORE_EXPORT TrackSorter
{
public:
    template <typename Item>
    struct SortEntry
    {
        Item item;
        QString sortKey;
    };

    TrackSorter();
    explicit TrackSorter(LibraryManager* libraryManager);
    ~TrackSorter();

    /*!
     * Calculates the sort fields and then sorts @p tracks
     * @param sort the sort script as a string
     * @param tracks the tracks to sort
     * @param order the order in which to sort the tracks
     * @returns a new sorted TrackList
     */
    TrackList calcSortTracks(const QString& sort, TrackList tracks, Qt::SortOrder order = Qt::AscendingOrder);

    /*!
     * Calculates the sort fields and then sorts @p tracks in the given @p indexes.
     * @param sort the sort script as a string
     * @param tracks the tracks to sort
     * @param indexes the indexes to sort
     * @param order the order in which to sort the tracks
     * @returns a new sorted TrackList
     */
    TrackList calcSortTracks(const QString& sort, TrackList tracks, const std::vector<int>& indexes,
                             Qt::SortOrder order = Qt::AscendingOrder);

    /*!
     * Calculates the sort fields and then sorts @p tracks
     * @param sortScript the parsed sort script
     * @param tracks the tracks to sort
     * @param order the order in which to sort the tracks
     * @returns a new sorted TrackList
     */
    TrackList calcSortTracks(const ParsedScript& sortScript, TrackList tracks,
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
    TrackList calcSortTracks(const ParsedScript& sortScript, TrackList tracks, const std::vector<int>& indexes,
                             Qt::SortOrder order = Qt::AscendingOrder);

    template <typename Container, typename SortScript, typename Extractor>
    auto calcSortEntries(const SortScript& sort, const Container& items, Extractor extractor)
        -> std::vector<SortEntry<typename Container::value_type>>
    {
        using Item = typename Container::value_type;
        std::vector<SortEntry<Item>> entries;
        entries.reserve(items.size());

        const std::scoped_lock lock{m_parserGuard};

        ScriptContext context;
        context.environment = &m_scriptEnvironment;

        for(const auto& item : items) {
            const Track& track = extractor(item);
            entries.push_back({item, m_parser.evaluate(sort, track, context)});
        }

        return entries;
    }

    template <typename Container, typename SortScript, typename Extractor>
    auto calcOwnedSortEntries(const SortScript& sort, Container items, Extractor extractor)
        -> std::vector<SortEntry<typename Container::value_type>>
    {
        using Item = typename Container::value_type;
        std::vector<SortEntry<Item>> entries;
        entries.reserve(items.size());

        const std::scoped_lock lock{m_parserGuard};

        ScriptContext context;
        context.environment = &m_scriptEnvironment;

        for(auto& item : items) {
            const Track& track = extractor(item);
            entries.push_back({std::move(item), m_parser.evaluate(sort, track, context)});
        }

        return entries;
    }

    template <typename Container, typename SortScript, typename Extractor>
    Container calcSortTracks(const SortScript& sort, const Container& items, Extractor extractor,
                             Qt::SortOrder order = Qt::AscendingOrder)
    {
        auto sortEntries = calcSortEntries(sort, items, extractor);
        sortSortEntries(sortEntries, order);
        return stripSortEntries<Container>(std::move(sortEntries));
    }

    template <typename Container, typename SortScript, typename Extractor>
    Container calcSortTracks(const SortScript& sortScript, const Container& items, const std::vector<int>& indexes,
                             Extractor extractor, Qt::SortOrder order = Qt::AscendingOrder)
    {
        Container sortedTracks{items};
        Container tracksToSort;

        auto validIndexes = indexes | std::views::filter([&items](int index) {
                                return (index >= 0 && index < static_cast<int>(items.size()));
                            });

        for(const int index : validIndexes) {
            tracksToSort.push_back(items.at(index));
        }

        auto sortEntries = calcSortEntries(sortScript, tracksToSort, extractor);
        sortSortEntries(sortEntries, order);
        Container sortedSubTracks = stripSortEntries<Container>(std::move(sortEntries));

        for(auto i{0}; const int index : validIndexes) {
            sortedTracks[index] = sortedSubTracks.at(i++);
        }

        return sortedTracks;
    }

private:
    ParsedScript parseScript(const QString& sort);

    template <typename Container>
    static Container stripSortEntries(std::vector<SortEntry<typename Container::value_type>> sortEntries)
    {
        Container items;
        items.reserve(sortEntries.size());

        for(auto& entry : sortEntries) {
            items.push_back(std::move(entry.item));
        }
        return items;
    }

    template <typename Item>
    static void sortSortEntries(std::vector<SortEntry<Item>>& sortEntries, Qt::SortOrder order = Qt::AscendingOrder)
    {
        StringCollator collator;
        std::ranges::stable_sort(sortEntries, [order, &collator](const auto& lhs, const auto& rhs) {
            const auto cmp = collator.compare(lhs.sortKey, rhs.sortKey);
            if(cmp == 0) {
                return false;
            }

            if(order == Qt::AscendingOrder) {
                return cmp < 0;
            }
            return cmp > 0;
        });
    }

    ScriptParser m_parser;
    LibraryScriptEnvironment m_scriptEnvironment;
    std::mutex m_parserGuard;
};
} // namespace Fooyin
