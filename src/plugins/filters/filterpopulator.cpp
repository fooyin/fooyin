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

#include "filterpopulator.h"

#include <core/constants.h>
#include <core/scripting/scriptregistry.h>
#include <utils/crypto.h>

namespace Fooyin::Filters {
FilterPopulator::FilterPopulator(LibraryManager* libraryManager, QObject* parent)
    : Worker{parent}
    , m_parser{new ScriptRegistry(libraryManager)}
{ }

void FilterPopulator::run(const QStringList& columns, const TrackList& tracks)
{
    setState(Running);

    m_data.clear();

    const QString newColumns = columns.join(u"\036");
    if(std::exchange(m_currentColumns, newColumns) != newColumns) {
        m_script = m_parser.parse(m_currentColumns);
    }

    const bool success = runBatch(tracks);

    setState(Idle);

    if(success) {
        emit finished();
    }
}

FilterItem* FilterPopulator::getOrInsertItem(const QStringList& columns)
{
    const auto key = Utils::generateMd5Hash(columns.join(QString{}));
    if(!m_data.items.contains(key)) {
        m_data.items.emplace(key, FilterItem{key, columns, &m_root});
    }
    return &m_data.items.at(key);
}

std::vector<FilterItem*> FilterPopulator::getOrInsertItems(const QList<QStringList>& columnSet)
{
    std::vector<FilterItem*> items;
    for(const QStringList& columns : columnSet) {
        auto* filterItem = getOrInsertItem(columns);
        items.emplace_back(filterItem);
    }
    return items;
}

void FilterPopulator::addTrackToNode(const Track& track, FilterItem* node)
{
    node->addTrack(track);
    m_data.trackParents[track.id()].push_back(node->key());
}

void FilterPopulator::iterateTrack(const Track& track)
{
    const QString columns = m_parser.evaluate(m_script, track);

    if(columns.contains(QLatin1String{Constants::UnitSeparator})) {
        const QStringList values = columns.split(QLatin1String{Constants::UnitSeparator});
        QList<QStringList> colValues;
        std::ranges::transform(values, std::back_inserter(colValues),
                               [](const QString& col) { return col.split(QLatin1String{Constants::RecordSeparator}); });
        const auto nodes = getOrInsertItems(colValues);
        for(FilterItem* node : nodes) {
            addTrackToNode(track, node);
        }
    }
    else {
        FilterItem* node = getOrInsertItem(columns.split(QLatin1String{Constants::RecordSeparator}));
        addTrackToNode(track, node);
    }
}

bool FilterPopulator::runBatch(const TrackList& tracks)
{
    for(const Track& track : tracks) {
        if(!mayRun()) {
            return false;
        }

        if(track.isInLibrary()) {
            iterateTrack(track);
        }
    }

    if(!mayRun()) {
        return false;
    }

    emit populated(m_data);
    m_data.clear();

    return true;
}
} // namespace Fooyin::Filters

#include "moc_filterpopulator.cpp"
