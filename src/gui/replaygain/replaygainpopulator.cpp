/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <LukeT1@proton.me>
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

#include "replaygainpopulator.h"

#include "replaygainmodel.h"

#include <utils/enum.h>

namespace Fooyin {
class ReplayGainPopulatorPrivate
{
public:
    void reset();
    void addNodeIfNew(const QString& key, const QString& name, ReplayGainModel::ItemParent parent,
                      ReplayGainItem::ItemType type, const Track& track = {});
    void checkAddParentNode(ReplayGainModel::ItemParent parent);
    void checkAddEntryNode(const QString& key, const QString& name, ReplayGainModel::ItemParent parent,
                           const Track& track = {});

    RGInfoData m_data;
};

void ReplayGainPopulatorPrivate::reset()
{
    m_data.clear();
}

void ReplayGainPopulatorPrivate::addNodeIfNew(const QString& key, const QString& name,
                                              ReplayGainModel::ItemParent parent, ReplayGainItem::ItemType type,
                                              const Track& track)
{
    if(key.isEmpty() || name.isEmpty()) {
        return;
    }

    if(m_data.nodes.contains(key)) {
        return;
    }

    m_data.nodes.emplace(key, ReplayGainItem{type, name, track, nullptr});
    m_data.parents[Utils::Enum::toString(parent)].emplace_back(key);
}

void ReplayGainPopulatorPrivate::checkAddParentNode(ReplayGainModel::ItemParent parent)
{
    using ItemParent = ReplayGainModel::ItemParent;
    if(parent == ItemParent::Summary) {
        addNodeIfNew(QStringLiteral("Summary"), ReplayGainPopulator::tr("Summary"), ItemParent::Root,
                     ReplayGainItem::Header);
    }
    else if(parent == ItemParent::Details) {
        addNodeIfNew(QStringLiteral("Details"), ReplayGainPopulator::tr("Details"), ItemParent::Root,
                     ReplayGainItem::Header);
    }
}

void ReplayGainPopulatorPrivate::checkAddEntryNode(const QString& key, const QString& name,
                                                   ReplayGainModel::ItemParent parent, const Track& track)
{
    checkAddParentNode(parent);
    addNodeIfNew(key, name, parent, ReplayGainItem::Entry, track);
}

ReplayGainPopulator::ReplayGainPopulator(QObject* parent)
    : Worker{parent}
    , p{std::make_unique<ReplayGainPopulatorPrivate>()}
{ }

ReplayGainPopulator::~ReplayGainPopulator() = default;

void ReplayGainPopulator::run(const TrackList& tracks)
{
    setState(Running);

    p->reset();

    for(const Track& track : tracks) {
        if(!mayRun()) {
            return;
        }

        const auto key = QStringLiteral("%1|%2").arg(track.id()).arg(track.effectiveTitle());
        p->checkAddEntryNode(key, track.effectiveTitle(), ReplayGainModel::ItemParent::Details, track);
    }

    if(mayRun()) {
        emit populated(p->m_data);
        emit finished();
    }

    p->reset();

    setState(Idle);
}
} // namespace Fooyin
