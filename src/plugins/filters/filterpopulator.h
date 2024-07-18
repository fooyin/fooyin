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

#include <core/scripting/scriptparser.h>
#include <core/scripting/scriptregistry.h>
#include <core/track.h>
#include <utils/worker.h>

namespace Fooyin::Filters {
using ItemKeyMap     = std::map<Md5Hash, FilterItem>;
using TrackIdNodeMap = std::unordered_map<int, std::vector<Md5Hash>>;

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
    explicit FilterPopulator(LibraryManager* libraryManager, QObject* parent = nullptr);

    void run(const QStringList& columns, const TrackList& tracks);

signals:
    void populated(Fooyin::Filters::PendingTreeData data);

private:
    FilterItem* getOrInsertItem(const QStringList& columns);
    std::vector<FilterItem*> getOrInsertItems(const QList<QStringList>& columnSet);
    void addTrackToNode(const Track& track, FilterItem* node);
    void iterateTrack(const Track& track);
    bool runBatch(const TrackList& tracks);

    ScriptRegistry m_registry;
    ScriptParser m_parser;

    QString m_currentColumns;
    ParsedScript m_script;

    FilterItem m_root;
    PendingTreeData m_data;
};
} // namespace Fooyin::Filters
