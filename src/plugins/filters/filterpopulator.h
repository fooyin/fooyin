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

#include "filteritem.h"

#include <core/track.h>
#include <utils/worker.h>

namespace Fooyin::Filters {
using ItemKeyMap     = std::map<QString, FilterItem>;
using TrackIdNodeMap = std::unordered_map<int, std::vector<QString>>;

struct PendingTreeData
{
    ItemKeyMap items;
    TrackIdNodeMap trackParents;

    void clear()
    {
        items.clear();
        trackParents.clear();
    }
};

class FilterPopulator : public Worker
{
    Q_OBJECT

public:
    FilterPopulator(QObject* parent = nullptr);
    ~FilterPopulator() override;

    void run(const QStringList& columns, const TrackList& tracks);

signals:
    void populated(PendingTreeData data);

private:
    struct Private;
    std::unique_ptr<Private> p;
};
} // namespace Fooyin::Filters
