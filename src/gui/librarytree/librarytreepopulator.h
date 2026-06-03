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

#include <utils/crypto.h>
#include <utils/worker.h>

#include <QFont>

#include <memory>

namespace Fooyin {
class LibraryManager;
class LibraryTreePopulatorPrivate;
class SettingsManager;

using ItemKeyMap     = std::unordered_map<Md5Hash, LibraryTreeItem>;
using NodeKeyMap     = std::unordered_map<Md5Hash, std::vector<Md5Hash>>;
using TrackIdNodeMap = std::unordered_map<int, std::vector<Md5Hash>>;

struct PendingTreeData
{
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
    explicit LibraryTreePopulator(LibraryManager* libraryManager, SettingsManager* settings, QObject* parent = nullptr);
    ~LibraryTreePopulator() override;

    void setFont(const QFont& font);

    void run(const LibraryTreeGrouping& grouping, const TrackList& tracks, bool useVarious);
    void updateItems(ItemKeyMap items, bool useVarious);

Q_SIGNALS:
    void populated(Fooyin::PendingTreeDataPtr data);

private:
    std::unique_ptr<LibraryTreePopulatorPrivate> p;
};
} // namespace Fooyin

Q_DECLARE_METATYPE(Fooyin::PendingTreeDataPtr)
