/*
 * Fooyin
 * Copyright 2022-2023, Luke Taylor <LukeT1@proton.me>
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

namespace Fy::Filters {
struct FilterPopulator::Private
{
    FilterPopulator* populator;

    Core::Scripting::Registry registry;
    Core::Scripting::Parser parser;

    QString currentField;
    QString currentSort;
    Core::Scripting::ParsedScript fieldScript;
    Core::Scripting::ParsedScript sortScript;

    FilterItem root;
    PendingTreeData data;

    explicit Private(FilterPopulator* populator)
        : populator{populator}
        , parser{&registry}
        , data{}
    { }

    FilterItem* getOrInsertItem(const QString& title, const QString& sortTitle)
    {
        if(!data.items.contains(title)) {
            data.items.emplace(title, FilterItem{title, sortTitle, &root});
        }
        return &data.items.at(title);
    }

    std::vector<FilterItem*> getOrInsertItems(const QStringList& titles, const QString& sortTitle)
    {
        std::vector<FilterItem*> items;
        for(const QString& title : titles) {
            auto* filterItem = getOrInsertItem(title, sortTitle);
            items.emplace_back(filterItem);
        }
        return items;
    }

    void addTrackToNode(const Core::Track& track, FilterItem* node)
    {
        node->addTrack(track);
        data.trackParents[track.id()].push_back(node->title());
    }

    void iterateTrack(const Core::Track& track)
    {
        const QString field = parser.evaluate(fieldScript, track);
        const QString sort  = parser.evaluate(sortScript, track);

        if(field.isNull()) {
            return;
        }

        if(field.contains(Core::Constants::Separator)) {
            const QStringList values = field.split(Core::Constants::Separator);
            const auto nodes         = getOrInsertItems(values, sort);
            for(FilterItem* node : nodes) {
                addTrackToNode(track, node);
            }
        }
        else {
            FilterItem* node = getOrInsertItem(field, sort);
            addTrackToNode(track, node);
        }
    }

    void runBatch(const Core::TrackList& tracks)
    {
        for(const Core::Track& track : tracks) {
            if(!populator->mayRun()) {
                return;
            }
            if(!track.enabled()) {
                continue;
            }
            iterateTrack(track);
        }

        if(!populator->mayRun()) {
            return;
        }

        QMetaObject::invokeMethod(populator, "populated", Q_ARG(PendingTreeData, data));

        data.clear();
    }
};

FilterPopulator::FilterPopulator(QObject* parent)
    : Utils::Worker{parent}
    , p{std::make_unique<Private>(this)}
{ }

FilterPopulator::~FilterPopulator() = default;

void FilterPopulator::run(const QString& field, const QString& sort, const Core::TrackList& tracks)
{
    setState(Running);

    p->data.clear();

    if(std::exchange(p->currentField, field) != field) {
        p->fieldScript = p->parser.parse(p->currentField);
    }
    if(std::exchange(p->currentSort, sort) != sort) {
        p->fieldScript = p->parser.parse(p->currentSort);
    }

    p->runBatch(tracks);

    if(Worker::mayRun()) {
        emit finished();
    }

    setState(Idle);
}
} // namespace Fy::Filters

#include "moc_filterpopulator.cpp"
