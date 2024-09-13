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

#include <core/constants.h>
#include <utils/enum.h>

#include <set>

namespace Fooyin {
class ReplayGainPopulatorPrivate
{
public:
    void reset();
    ReplayGainItem* addNodeIfNew(const QString& key, const QString& name, float value,
                                 ReplayGainModel::ItemParent parent, ReplayGainItem::ItemType type, const Track& track);
    ReplayGainItem* addNodeIfNew(const QString& key, const QString& name, ReplayGainModel::ItemParent parent,
                                 ReplayGainItem::ItemType type, const Track& track = {});
    void checkAddParentNode(ReplayGainModel::ItemParent parent);

    RGInfoData m_data;
};

void ReplayGainPopulatorPrivate::reset()
{
    m_data.clear();
}

ReplayGainItem* ReplayGainPopulatorPrivate::addNodeIfNew(const QString& key, const QString& name, float value,
                                                         ReplayGainModel::ItemParent parent,
                                                         ReplayGainItem::ItemType type, const Track& track)
{
    if(key.isEmpty() || name.isEmpty()) {
        return nullptr;
    }

    if(m_data.nodes.contains(key)) {
        return &m_data.nodes.at(key);
    }

    checkAddParentNode(parent);

    m_data.nodes.emplace(key, ReplayGainItem{type, name, value, track, nullptr});
    m_data.parents[Utils::Enum::toString(parent)].emplace_back(key);

    return &m_data.nodes.at(key);
}

ReplayGainItem* ReplayGainPopulatorPrivate::addNodeIfNew(const QString& key, const QString& name,
                                                         ReplayGainModel::ItemParent parent,
                                                         ReplayGainItem::ItemType type, const Track& track)
{
    if(key.isEmpty() || name.isEmpty()) {
        return nullptr;
    }

    if(m_data.nodes.contains(key)) {
        return nullptr;
    }

    checkAddParentNode(parent);

    m_data.nodes.emplace(key, ReplayGainItem{type, name, track, nullptr});
    m_data.parents[Utils::Enum::toString(parent)].emplace_back(key);

    return &m_data.nodes.at(key);
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

ReplayGainPopulator::ReplayGainPopulator(QObject* parent)
    : Worker{parent}
    , p{std::make_unique<ReplayGainPopulatorPrivate>()}
{ }

ReplayGainPopulator::~ReplayGainPopulator() = default;

void ReplayGainPopulator::run(const TrackList& tracks)
{
    setState(Running);

    p->reset();

    if(tracks.size() == 1) {
        const Track& track = tracks.front();
        p->addNodeIfNew(QStringLiteral("TrackGain"), tr("Track Gain"), track.rgTrackGain(),
                        ReplayGainModel::ItemParent::Root, ReplayGainItem::TrackGain, track);
        p->addNodeIfNew(QStringLiteral("TrackPeak"), tr("Track Peak"), track.rgTrackPeak(),
                        ReplayGainModel::ItemParent::Root, ReplayGainItem::TrackPeak, track);
        p->addNodeIfNew(QStringLiteral("AlbumGain"), tr("Album Gain"), track.rgAlbumGain(),
                        ReplayGainModel::ItemParent::Root, ReplayGainItem::AlbumGain, track);
        p->addNodeIfNew(QStringLiteral("AlbumPeak"), tr("Album Peak"), track.rgAlbumPeak(),
                        ReplayGainModel::ItemParent::Root, ReplayGainItem::AlbumPeak, track);
    }
    else {
        std::set<float> trackGain;
        std::set<float> albumGain;
        float totalPeak{Constants::InvalidPeak};

        for(const Track& track : tracks) {
            if(!mayRun()) {
                return;
            }

            if(track.hasTrackGain()) {
                trackGain.emplace(track.rgTrackGain());
            }
            if(track.hasAlbumGain()) {
                albumGain.emplace(track.rgAlbumGain());
            }
            if(track.hasTrackPeak()) {
                totalPeak = std::max(totalPeak, track.rgTrackPeak());
            }

            const auto key = QStringLiteral("%1|%2").arg(track.id()).arg(track.effectiveTitle());
            p->addNodeIfNew(key, track.effectiveTitle(), ReplayGainModel::ItemParent::Details, ReplayGainItem::Entry,
                            track);
        }

        if(auto* gain = p->addNodeIfNew(QStringLiteral("TrackGain"), tr("Track Gain"),
                                        trackGain.empty() ? Constants::InvalidGain : *trackGain.cbegin(),
                                        ReplayGainModel::ItemParent::Summary, ReplayGainItem::TrackGain, {})) {
            gain->setMultipleValues(trackGain.size() > 1);
        }
        if(auto* gain = p->addNodeIfNew(QStringLiteral("AlbumGain"), tr("Album Gain"),
                                        albumGain.empty() ? Constants::InvalidGain : *albumGain.cbegin(),
                                        ReplayGainModel::ItemParent::Summary, ReplayGainItem::AlbumGain, {})) {
            gain->setMultipleValues(albumGain.size() > 1);
        }
        if(auto* maxPeak = p->addNodeIfNew(QStringLiteral("TotalPeak"), QStringLiteral("Total Peak"), totalPeak,
                                           ReplayGainModel::ItemParent::Summary, ReplayGainItem::TrackPeak, {})) {
            maxPeak->setIsEditable(false);
        }
        if(auto* lowestGain = p->addNodeIfNew(QStringLiteral("LowestGain"), tr("Lowest Gain (Loudest track)"),
                                              *std::ranges::max_element(trackGain),
                                              ReplayGainModel::ItemParent::Summary, ReplayGainItem::TrackGain, {})) {
            lowestGain->setIsEditable(false);
        }
        if(auto* highestGain = p->addNodeIfNew(QStringLiteral("HighestGain"), tr("Highest Gain (Quietest track)"),
                                               *std::ranges::min_element(trackGain),
                                               ReplayGainModel::ItemParent::Summary, ReplayGainItem::TrackGain, {})) {
            highestGain->setIsEditable(false);
        }
    }

    if(mayRun()) {
        emit populated(p->m_data);
        emit finished();
    }

    p->reset();

    setState(Idle);
}
} // namespace Fooyin
