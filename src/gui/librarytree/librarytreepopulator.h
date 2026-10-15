/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <luket@pm.me>
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

#include "librarytreegroup.h"
#include "librarytreeitem.h"

#include <core/library/libraryinfo.h>
#include <core/ratingsymbols.h>
#include <utils/crypto.h>
#include <utils/worker.h>

#include <QFont>

#include <memory>
#include <stop_token>

namespace Fooyin {
class LibraryTreePopulatorPrivate;

using ItemKeyMap     = std::unordered_map<Md5Hash, LibraryTreeItem>;
using NodeKeyMap     = std::unordered_map<Md5Hash, std::vector<Md5Hash>>;
using TrackIdNodeMap = std::unordered_map<int, std::vector<Md5Hash>>;

struct LibraryTreePopulationContext
{
    LibraryInfoMap libraries;
    QFont font;
    RatingStarSymbols ratingSymbols;
    bool useVarious{false};
    uint64_t generation{0};
    uint64_t requestId{0};
    std::stop_token stopToken;
};

struct PendingTreeData
{
    uint64_t generation{0};
    ItemKeyMap items;
    ItemKeyMap updatedItems;
    NodeKeyMap nodes;
    TrackIdNodeMap trackParents;

    void clear()
    {
        items.clear();
        updatedItems.clear();
        nodes.clear();
        trackParents.clear();
    }
};

using PendingTreeDataPtr = std::shared_ptr<PendingTreeData>;

class LibraryTreePopulator : public Worker
{
    Q_OBJECT

public:
    enum class PopulationMode : uint8_t
    {
        Incremental = 0,
        Atomic
    };

    explicit LibraryTreePopulator(QObject* parent = nullptr);
    ~LibraryTreePopulator() override;

    void run(const LibraryTreeGrouping& grouping, const TrackList& tracks, LibraryTreePopulationContext context,
             PopulationMode mode = PopulationMode::Incremental);
    void updateItems(ItemKeyMap items, LibraryTreePopulationContext context);

Q_SIGNALS:
    void populated(Fooyin::PendingTreeDataPtr data);
    void populationFinished(uint64_t generation, uint64_t requestId);

private:
    std::unique_ptr<LibraryTreePopulatorPrivate> p;
};
} // namespace Fooyin

Q_DECLARE_METATYPE(Fooyin::PendingTreeDataPtr)
