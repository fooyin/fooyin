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

using namespace Qt::StringLiterals;

namespace Fooyin {
class ReplayGainPopulatorPrivate
{
public:
    void reset();
    ReplayGainItem* addNodeIfNew(const QString& key, const QString& name, float value,
                                 ReplayGainModel::ItemParent parent, ReplayGainItem::ItemType type, const Track& track);
    ReplayGainItem* addNodeIfNew(const QString& key, const QString& name, ReplayGainModel::ItemParent parent,
                                 ReplayGainItem::ItemType type, const Track& track = {});
    ReplayGainItem* addSummaryNode(const QString& key, const QString& name, float value, ReplayGainItem::ItemType type);
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

ReplayGainItem* ReplayGainPopulatorPrivate::addSummaryNode(const QString& key, const QString& name, float value,
                                                           ReplayGainItem::ItemType type)
{
    return addNodeIfNew(key, name, value, ReplayGainModel::ItemParent::Summary, type, {});
}

void ReplayGainPopulatorPrivate::checkAddParentNode(ReplayGainModel::ItemParent parent)
{
    using ItemParent = ReplayGainModel::ItemParent;
    if(parent == ItemParent::Summary) {
        addNodeIfNew(u"Summary"_s, ReplayGainPopulator::tr("Summary"), ItemParent::Root, ReplayGainItem::Header);
    }
    else if(parent == ItemParent::Details) {
        addNodeIfNew(u"Details"_s, ReplayGainPopulator::tr("Details"), ItemParent::Root, ReplayGainItem::Header);
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
        p->addNodeIfNew(u"TrackGain"_s, tr("Track Gain"), track.rgTrackGain(), ReplayGainModel::ItemParent::Root,
                        ReplayGainItem::TrackGain, track);
        p->addNodeIfNew(u"TrackPeak"_s, tr("Track Peak"), track.rgTrackPeak(), ReplayGainModel::ItemParent::Root,
                        ReplayGainItem::TrackPeak, track);
        p->addNodeIfNew(u"AlbumGain"_s, tr("Album Gain"), track.rgAlbumGain(), ReplayGainModel::ItemParent::Root,
                        ReplayGainItem::AlbumGain, track);
        p->addNodeIfNew(u"AlbumPeak"_s, tr("Album Peak"), track.rgAlbumPeak(), ReplayGainModel::ItemParent::Root,
                        ReplayGainItem::AlbumPeak, track);
    }
    else {
        if(auto* gain
           = p->addSummaryNode(u"TrackGain"_s, tr("Track Gain"), Constants::InvalidGain, ReplayGainItem::TrackGain)) {
            gain->setSummaryFunc([](ReplayGainItem* parent, const ReplayGainItem::RGValues& values) {
                if(values.contains(ReplayGainItem::TrackGain)) {
                    auto gains = values.at(ReplayGainItem::TrackGain);
                    parent->setTrackGain(gains.empty() ? Constants::InvalidGain : *gains.cbegin());
                    parent->setMultipleValues(gains.size() > 1);
                }
            });
        }

        if(auto* gain
           = p->addSummaryNode(u"AlbumGain"_s, tr("Album Gain"), Constants::InvalidGain, ReplayGainItem::AlbumGain)) {
            gain->setSummaryFunc([](ReplayGainItem* parent, const ReplayGainItem::RGValues& values) {
                if(values.contains(ReplayGainItem::AlbumGain)) {
                    const auto& gains = values.at(ReplayGainItem::AlbumGain);
                    parent->setAlbumGain(gains.empty() ? Constants::InvalidGain : *gains.cbegin());
                    parent->setMultipleValues(gains.size() > 1);
                }
            });
        }

        if(auto* maxPeak
           = p->addSummaryNode(u"TotalPeak"_s, u"Total Peak"_s, Constants::InvalidPeak, ReplayGainItem::TrackPeak)) {
            maxPeak->setIsEditable(false);
            maxPeak->setSummaryFunc([](ReplayGainItem* parent, const ReplayGainItem::RGValues& values) {
                if(values.contains(ReplayGainItem::TrackPeak)) {
                    auto peaks = values.at(ReplayGainItem::TrackPeak);
                    peaks.erase(Constants::InvalidPeak);
                    parent->setTrackPeak(peaks.empty() ? Constants::InvalidPeak : *std::ranges::max_element(peaks));
                }
            });
        }

        if(auto* lowestGain = p->addSummaryNode(u"LowestGain"_s, tr("Lowest Gain (Loudest track)"),
                                                Constants::InvalidGain, ReplayGainItem::TrackGain)) {
            lowestGain->setIsEditable(false);
            lowestGain->setSummaryFunc([](ReplayGainItem* parent, const ReplayGainItem::RGValues& values) {
                if(values.contains(ReplayGainItem::TrackGain)) {
                    auto gains = values.at(ReplayGainItem::TrackGain);
                    gains.erase(Constants::InvalidGain);
                    parent->setTrackGain(gains.empty() ? Constants::InvalidGain : *std::ranges::max_element(gains));
                }
            });
        }

        if(auto* highestGain = p->addSummaryNode(u"HighestGain"_s, tr("Highest Gain (Quietest track)"),
                                                 Constants::InvalidGain, ReplayGainItem::TrackGain)) {
            highestGain->setIsEditable(false);
            highestGain->setSummaryFunc([](ReplayGainItem* parent, const ReplayGainItem::RGValues& values) {
                if(values.contains(ReplayGainItem::TrackGain)) {
                    auto gains = values.at(ReplayGainItem::TrackGain);
                    gains.erase(Constants::InvalidGain);
                    parent->setTrackGain(gains.empty() ? Constants::InvalidGain : *std::ranges::min_element(gains));
                }
            });
        }

        for(const Track& track : tracks) {
            if(!mayRun()) {
                return;
            }

            const auto key = u"%1|%2"_s.arg(track.id()).arg(track.effectiveTitle());
            p->addNodeIfNew(key, track.effectiveTitle(), ReplayGainModel::ItemParent::Details, ReplayGainItem::Entry,
                            track);
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
