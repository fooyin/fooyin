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

constexpr auto ColumnSeparator = "\036";

namespace Fooyin::Filters {
struct FilterPopulator::Private
{
    FilterPopulator* self;

    ScriptRegistry registry;
    ScriptParser parser;

    QString currentColumns;
    ParsedScript script;

    FilterItem root;
    PendingTreeData data;

    explicit Private(FilterPopulator* self_)
        : self{self_}
        , parser{&registry}
        , data{}
    { }

    FilterItem* getOrInsertItem(const QStringList& columns)
    {
        const QString key = Utils::generateHash(columns.join(""));
        if(!data.items.contains(key)) {
            data.items.emplace(key, FilterItem{key, columns, &root});
        }
        return &data.items.at(key);
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
        data.trackParents[track.id()].push_back(node->key());
    }

    void iterateTrack(const Track& track)
    {
        const QString columns = parser.evaluate(script, track);

        if(columns.isNull()) {
            return;
        }

        if(columns.contains(Constants::Separator)) {
            const QStringList values = columns.split(Constants::Separator);
            QList<QStringList> colValues;
            std::ranges::transform(values, std::back_inserter(colValues),
                                   [](const QString& col) { return col.split(ColumnSeparator); });
            const auto nodes = getOrInsertItems(colValues);
            for(FilterItem* node : nodes) {
                addTrackToNode(track, node);
            }
        }
        else {
            FilterItem* node = getOrInsertItem(columns.split(ColumnSeparator));
            addTrackToNode(track, node);
        }
    }

    void runBatch(const TrackList& tracks)
    {
        for(const Track& track : tracks) {
            if(!self->mayRun()) {
                return;
            }

            if(track.enabled() && track.libraryId() >= 0) {
                iterateTrack(track);
            }
        }

        if(!self->mayRun()) {
            return;
        }

        QMetaObject::invokeMethod(self, "populated", Q_ARG(PendingTreeData, data));

        data.clear();
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

    p->data.clear();

    const QString newColumns = columns.join(ColumnSeparator);
    if(std::exchange(p->currentColumns, newColumns) != newColumns) {
        p->script = p->parser.parse(p->currentColumns);
    }

    p->runBatch(tracks);

    if(Worker::mayRun()) {
        emit finished();
    }

    setState(Idle);
}
} // namespace Fooyin::Filters

#include "moc_filterpopulator.cpp"
