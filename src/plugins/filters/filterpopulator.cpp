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
#include <core/scripting/scriptparser.h>
#include <core/scripting/scriptregistry.h>
#include <core/track.h>

#include <utils/crypto.h>

namespace Fooyin::Filters {
struct FilterPopulator::Private
{
    FilterPopulator* m_self;

    ScriptRegistry m_registry;
    ScriptParser m_parser;

    QString m_currentColumns;
    ParsedScript m_script;

    FilterItem m_root;
    PendingTreeData m_data;

    explicit Private(FilterPopulator* self)
        : m_self{self}
        , m_parser{&m_registry}
    { }

    FilterItem* getOrInsertItem(const QStringList& columns)
    {
        const QString key = Utils::generateHash(columns.join(QStringLiteral("")));
        if(!m_data.items.contains(key)) {
            m_data.items.emplace(key, FilterItem{key, columns, &m_root});
        }
        return &m_data.items.at(key);
    }

    std::vector<FilterItem*> getOrInsertItems(const QList<QStringList>& columnSet)
    {
        std::vector<FilterItem*> items;
        for(const QStringList& columns : columnSet) {
            auto* filterItem = getOrInsertItem(columns);
            items.emplace_back(filterItem);
        }
        return items;
    }

    void addTrackToNode(const Track& track, FilterItem* node)
    {
        node->addTrack(track);
        m_data.trackParents[track.id()].push_back(node->key());
    }

    void iterateTrack(const Track& track)
    {
        const QString columns = m_parser.evaluate(m_script, track);

        if(columns.contains(u"\037")) {
            const QStringList values = columns.split(QStringLiteral("\037"));
            QList<QStringList> colValues;
            std::ranges::transform(values, std::back_inserter(colValues),
                                   [](const QString& col) { return col.split(QStringLiteral("\036")); });
            const auto nodes = getOrInsertItems(colValues);
            for(FilterItem* node : nodes) {
                addTrackToNode(track, node);
            }
        }
        else {
            FilterItem* node = getOrInsertItem(columns.split(QStringLiteral("\036")));
            addTrackToNode(track, node);
        }
    }

    void runBatch(const TrackList& tracks)
    {
        for(const Track& track : tracks) {
            if(!m_self->mayRun()) {
                return;
            }

            if(track.isInLibrary()) {
                iterateTrack(track);
            }
        }

        if(!m_self->mayRun()) {
            return;
        }

        emit m_self->populated(m_data);
        m_data.clear();
    }
};

FilterPopulator::FilterPopulator(QObject* parent)
    : Worker{parent}
    , p{std::make_unique<Private>(this)}
{ }

FilterPopulator::~FilterPopulator() = default;

void FilterPopulator::run(const QStringList& columns, const TrackList& tracks)
{
    setState(Running);

    p->m_data.clear();

    const QString newColumns = columns.join(u"\036");
    if(std::exchange(p->m_currentColumns, newColumns) != newColumns) {
        p->m_script = p->m_parser.parse(p->m_currentColumns);
    }

    p->runBatch(tracks);

    if(Worker::mayRun()) {
        emit finished();
    }

    setState(Idle);
}
} // namespace Fooyin::Filters

#include "moc_filterpopulator.cpp"
