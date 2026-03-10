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

/*!
 * Evaluates sort scripts against tracks and returns a stably sorted result.
 */
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
     * Sorts an owned track list using a sort script string.
     * @param sort Sort script to evaluate for each track.
     * @param tracks Track list to sort. Ownership is transferred into the call.
     * @param order Sort order for the evaluated sort keys.
     * @returns A new track list sorted by the evaluated sort keys.
     */
    TrackList calcSortTracks(const QString& sort, TrackList tracks, Qt::SortOrder order = Qt::AscendingOrder);

    /*!
     * Sorts only a subset of an owned track list using a sort script string.
     *
     * Tracks outside @p indexes retain their original position. Invalid indexes
     * are ignored.
     *
     * @param sort Sort script to evaluate for each selected track.
     * @param tracks Track list containing the subset to reorder.
     * @param indexes Indexes within @p tracks to sort.
     * @param order Sort order for the evaluated sort keys.
     * @returns A copy of @p tracks with only the selected indexes reordered.
     */
    TrackList calcSortTracks(const QString& sort, TrackList tracks, const std::vector<int>& indexes,
                             Qt::SortOrder order = Qt::AscendingOrder);

    /*!
     * Sorts an owned track list using a pre-parsed sort script.
     * @param sortScript Parsed script returned by `parseSortScript()`.
     * @param tracks Track list to sort. Ownership is transferred into the call.
     * @param order Sort order for the evaluated sort keys.
     * @returns A new track list sorted by the evaluated sort keys.
     */
    TrackList calcSortTracks(const ParsedScript& sortScript, TrackList tracks,
                             Qt::SortOrder order = Qt::AscendingOrder);

    /*!
     * Sorts only a subset of an owned track list using a pre-parsed sort script.
     *
     * Tracks outside @p indexes retain their original position. Invalid indexes
     * are ignored.
     *
     * @param sortScript Parsed script returned by `parseSortScript()`.
     * @param tracks Track list containing the subset to reorder.
     * @param indexes Indexes within @p tracks to sort.
     * @param order Sort order for the evaluated sort keys.
     * @returns A copy of @p tracks with only the selected indexes reordered.
     */
    TrackList calcSortTracks(const ParsedScript& sortScript, TrackList tracks, const std::vector<int>& indexes,
                             Qt::SortOrder order = Qt::AscendingOrder);

    /*!
     * Parses a sort script once for reuse across later sort operations.
     * @param sort Sort script string.
     * @returns A parsed script suitable for the ParsedScript overloads.
     */
    ParsedScript parseSortScript(const QString& sort);

    template <typename Container, typename SortScript, typename Extractor>
    /*!
     * Evaluates sort keys for a container without reordering it.
     *
     * `Extractor` must return the track to evaluate for each item in @p items.
     * The returned entries preserve the original item and its computed sort key.
     */
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
    /*!
     * Evaluates sort keys for an owned container without reordering it.
     *
     * Unlike `calcSortEntries()`, items are moved into the returned entries.
     */
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
    /*!
     * Sorts an arbitrary container by evaluating a sort key for each item.
     *
     * `Extractor` must return the track to evaluate for each item in @p items.
     * The input container is copied; the returned container holds the sorted
     * items.
     */
    Container calcSortTracks(const SortScript& sort, const Container& items, Extractor extractor,
                             Qt::SortOrder order = Qt::AscendingOrder)
    {
        auto sortEntries = calcSortEntries(sort, items, extractor);
        sortSortEntries(sortEntries, order);
        return stripSortEntries<Container>(std::move(sortEntries));
    }

    template <typename Container, typename SortScript, typename Extractor>
    /*!
     * Sorts only a subset of an arbitrary container.
     *
     * Items outside @p indexes retain their original position. Invalid indexes
     * are ignored.
     */
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
