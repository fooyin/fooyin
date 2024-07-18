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

#include "librarytreeitem.h"

#include <utils/crypto.h>
#include <utils/worker.h>

namespace Fooyin {
class LibraryManager;
class LibraryTreePopulatorPrivate;

using ItemKeyMap     = std::unordered_map<Md5Hash, LibraryTreeItem>;
using NodeKeyMap     = std::unordered_map<Md5Hash, std::vector<Md5Hash>>;
using TrackIdNodeMap = std::unordered_map<int, std::vector<Md5Hash>>;

struct PendingTreeData
{
    ItemKeyMap items;
    NodeKeyMap nodes;
    TrackIdNodeMap trackParents;

    void clear()
    {
        items.clear();
        nodes.clear();
        trackParents.clear();
    }
};

class LibraryTreePopulator : public Worker
{
    Q_OBJECT

public:
    explicit LibraryTreePopulator(LibraryManager* libraryManager, QObject* parent = nullptr);
    ~LibraryTreePopulator() override;

    void run(const QString& grouping, const TrackList& tracks);

signals:
    void populated(Fooyin::PendingTreeData data);

private:
    std::unique_ptr<LibraryTreePopulatorPrivate> p;
};
} // namespace Fooyin
