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

#include "replaygainmodel.h"

#include <core/constants.h>
#include <core/library/libraryutils.h>

#include <ranges>

using namespace Qt::StringLiterals;

constexpr auto HeaderFontDelta = 2;

namespace Fooyin {
ReplayGainModel::ReplayGainModel(bool readOnly, QObject* parent)
    : TreeModel{parent}
    , m_readOnly{readOnly}
{
    m_populator.moveToThread(&m_populatorThread);

    m_headerFont.setPointSize(m_headerFont.pointSize() + HeaderFontDelta);
    m_headerFont.setBold(true);

    QObject::connect(&m_populator, &ReplayGainPopulator::populated, this, &ReplayGainModel::populate);
}

ReplayGainModel::~ReplayGainModel()
{
    m_populatorThread.quit();
    m_populatorThread.wait();
}

void ReplayGainModel::resetModel(const TrackList& tracks)
{
    if(m_populatorThread.isRunning()) {
        m_populator.stopThread();
    }
    else {
        m_populatorThread.start();
    }

    m_tracks = tracks;

    QMetaObject::invokeMethod(&m_populator, [this, tracks] { m_populator.run(tracks); });
}

void ReplayGainModel::updateTracks(const TrackList& tracks)
{
    Utils::updateCommonTracks(m_tracks, tracks, Utils::CommonOperation::Update);

    for(auto& item : m_nodes | std::views::values) {
        const auto trackIt = std::ranges::find_if(tracks, [&item](const Track& track) {
            return item.track().isInDatabase() && item.track().id() == track.id();
        });
        if(trackIt != tracks.cend()) {
            item.setTrack(*trackIt);
        }
    }
}

TrackList ReplayGainModel::applyChanges()
{
    TrackList tracks;

    for(auto& item : m_nodes | std::views::values) {
        if(item.applyChanges()) {
            const Track changedTrack = item.track();
            auto trackIt             = std::ranges::find_if(
                tracks, [&changedTrack](const Track& track) { return track.sameIdentityAs(changedTrack); });

            if(trackIt == tracks.end()) {
                tracks.emplace_back(changedTrack);
                trackIt = std::prev(tracks.end());
            }

            switch(item.type()) {
                case ReplayGainItem::TrackGain:
                    trackIt->setRGTrackGain(changedTrack.rgTrackGain());
                    break;
                case ReplayGainItem::TrackPeak:
                    trackIt->setRGTrackPeak(changedTrack.rgTrackPeak());
                    break;
                case ReplayGainItem::AlbumGain:
                    trackIt->setRGAlbumGain(changedTrack.rgAlbumGain());
                    break;
                case ReplayGainItem::AlbumPeak:
                    trackIt->setRGAlbumPeak(changedTrack.rgAlbumPeak());
                    break;
                case ReplayGainItem::Header:
                case ReplayGainItem::Entry:
                    *trackIt = changedTrack;
                    break;
            }
        }
    }

    if(tracks.empty()) {
        return {};
    }

    beginResetModel();
    endResetModel();

    return tracks;
}

Qt::ItemFlags ReplayGainModel::flags(const QModelIndex& index) const
{
    if(!index.isValid()) {
        return Qt::NoItemFlags;
    }

    auto flags = TreeModel::flags(index);

    if(index.data(ReplayGainItem::Type).toInt() != ReplayGainItem::Header) {
        flags |= Qt::ItemNeverHasChildren;

        const auto* item                 = itemForIndex(index);
        const bool editableSummaryColumn = !item->isSummary() || index.column() == 1;
        if(!m_readOnly && index.column() > 0 && item->isEditable() && editableSummaryColumn) {
            flags |= Qt::ItemIsEditable;
        }
    }

    return flags;
}

QVariant ReplayGainModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(role == Qt::TextAlignmentRole) {
        return Qt::AlignCenter;
    }

    if(role != Qt::DisplayRole || orientation == Qt::Orientation::Vertical) {
        return {};
    }

    switch(section) {
        case(0):
            return tr("Name");
        case(1):
            return m_tracks.size() == 1 ? tr("Value") : tr("Track Gain");
        case(2):
            return tr("Track Peak");
        case(3):
            return tr("Album Gain");
        case(4):
            return tr("Album Peak");
        default:
            break;
    }

    return {};
}

int ReplayGainModel::columnCount(const QModelIndex& /*parent*/) const
{
    return m_tracks.size() == 1 ? 2 : 5;
}

QVariant ReplayGainModel::data(const QModelIndex& index, int role) const
{
    if(!checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return {};
    }

    const auto* item = itemForIndex(index);
    const auto type  = item->type();

    if(role == ReplayGainItem::Type) {
        return type;
    }
    if(role == ReplayGainItem::IsSummary) {
        return item->isSummary();
    }
    if(role == Qt::FontRole) {
        return (type == ReplayGainItem::Header) ? m_headerFont : item->font();
    }

    const bool isEdit = (role == Qt::EditRole);
    if(role != Qt::DisplayRole && !isEdit) {
        return {};
    }

    auto formatGain = [](float gain, int precision) {
        return (gain != Constants::InvalidGain) ? QString::number(gain, 'f', precision) : QString{};
    };

    auto formatPeak = [](float peak, int precision) {
        return (peak != Constants::InvalidPeak) ? QString::number(peak, 'f', precision) : QString{};
    };

    const int column = index.column();

    if(column == 0) {
        return item->name();
    }

    if(item->isSummary()) {
        if(column == 1) {
            if(item->multipleValues()) {
                return u"<<multiple>>"_s;
            }

            QString value;
            bool isPeak{false};

            switch(type) {
                case ReplayGainItem::TrackPeak:
                    value  = formatPeak(item->trackPeak(), 6);
                    isPeak = true;
                    break;
                case ReplayGainItem::AlbumGain:
                    value = formatGain(item->albumGain(), 2);
                    break;
                case ReplayGainItem::AlbumPeak:
                    value  = formatPeak(item->albumPeak(), 6);
                    isPeak = true;
                    break;
                case ReplayGainItem::TrackGain:
                default:
                    value = formatGain(item->trackGain(), 2);
                    break;
            }

            if(value.isEmpty()) {
                return {};
            }

            return (isEdit || isPeak) ? value : u"%1 dB"_s.arg(value);
        }
        return {};
    }

    if(column == 1) {
        if(item->multipleValues()) {
            return u"<<multiple values>>"_s;
        }

        if(m_tracks.size() == 1) {
            float numericValue{Constants::InvalidGain};
            QString value;
            bool isPeak{false};

            switch(type) {
                case ReplayGainItem::TrackPeak:
                    numericValue = item->trackPeak();
                    value        = formatPeak(numericValue, 6);
                    isPeak       = true;
                    break;
                case ReplayGainItem::AlbumGain:
                    numericValue = item->albumGain();
                    value        = formatGain(numericValue, 2);
                    break;
                case ReplayGainItem::AlbumPeak:
                    numericValue = item->albumPeak();
                    value        = formatPeak(numericValue, 6);
                    isPeak       = true;
                    break;
                case ReplayGainItem::TrackGain:
                default:
                    numericValue = item->trackGain();
                    value        = formatGain(numericValue, 2);
                    break;
            }

            if(value.isEmpty()) {
                return {};
            }

            return (isEdit || isPeak) ? value : u"%1 dB"_s.arg(value).prepend(numericValue > 0 ? "+"_L1 : ""_L1);
        }

        const float trackGain = item->trackGain();
        const QString value   = formatGain(trackGain, 2);
        if(value.isEmpty()) {
            return {};
        }

        return isEdit ? value : u"%1 dB"_s.arg(value).prepend(trackGain > 0 ? "+"_L1 : ""_L1);
    }

    if(column == 2) {
        return formatPeak(item->trackPeak(), 6);
    }

    if(column == 3) {
        const float albumGain = item->albumGain();
        const QString value   = formatGain(albumGain, 2);
        if(value.isEmpty()) {
            return {};
        }
        return isEdit ? value : u"%1 dB"_s.arg(value).prepend(albumGain > 0 ? "+"_L1 : ""_L1);
    }

    if(column == 4) {
        return formatPeak(item->albumPeak(), 6);
    }

    return {};
}

bool ReplayGainModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if(m_tracks.empty() || role != Qt::EditRole) {
        return false;
    }

    const int column = index.column();
    if(column == 0) {
        return false;
    }

    auto* item              = itemForIndex(index);
    const auto type         = item->type();
    const QString textValue = value.toString().trimmed();

    if(item->isSummary() && item->multipleValues() && column == 1 && textValue == u"<<multiple>>"_s) {
        return false;
    }

    bool ok{false};
    const float setValue = textValue.toFloat(&ok);

    if(!ok && !textValue.isEmpty()) {
        return false;
    }

    const auto setGainOrPeak = [this, &index, item](auto setFunc, float validValue) {
        bool changed{false};

        if(item->isSummary()) {
            for(auto& node : m_nodes | std::views::values) {
                if(node.type() == ReplayGainItem::Header || node.isSummary()) {
                    continue;
                }
                changed |= (node.*setFunc)(validValue);
            }
        }
        else {
            changed = (item->*setFunc)(validValue);
        }

        if(!changed) {
            return false;
        }

        emit dataChanged(index, index);
        updateSummary();
        return true;
    };

    switch(column) {
        case(1):
            switch(type) {
                case ReplayGainItem::TrackPeak: {
                    const float validValue = ok ? setValue : Constants::InvalidPeak;
                    return setGainOrPeak(&ReplayGainItem::setTrackPeak, validValue);
                }
                case ReplayGainItem::AlbumGain: {
                    const float validValue = ok ? setValue : Constants::InvalidGain;
                    return setGainOrPeak(&ReplayGainItem::setAlbumGain, validValue);
                }
                case ReplayGainItem::AlbumPeak: {
                    const float validValue = ok ? setValue : Constants::InvalidPeak;
                    return setGainOrPeak(&ReplayGainItem::setAlbumPeak, validValue);
                }
                case ReplayGainItem::TrackGain:
                default: {
                    const float validValue = ok ? setValue : Constants::InvalidGain;
                    return setGainOrPeak(&ReplayGainItem::setTrackGain, validValue);
                }
            }
            break;
        case(2): {
            const float validValue = ok ? setValue : Constants::InvalidPeak;
            return setGainOrPeak(&ReplayGainItem::setTrackPeak, validValue);
        }
        case(3): {
            const float validValue = ok ? setValue : Constants::InvalidGain;
            return setGainOrPeak(&ReplayGainItem::setAlbumGain, validValue);
        }
        case(4): {
            const float validValue = ok ? setValue : Constants::InvalidPeak;
            return setGainOrPeak(&ReplayGainItem::setAlbumPeak, validValue);
        }
        default:
            break;
    }

    return false;
}

void ReplayGainModel::populate(const RGInfoData& data)
{
    beginResetModel();
    resetRoot();
    m_nodes.clear();

    m_nodes = data.nodes;

    for(const auto& [parentKey, children] : data.parents) {
        ReplayGainItem* parent{nullptr};

        if(parentKey == "Root"_L1) {
            parent = rootItem();
        }
        else if(m_nodes.contains(parentKey)) {
            parent = &m_nodes.at(parentKey);
        }

        if(parent) {
            for(const auto& child : children) {
                if(m_nodes.contains(child)) {
                    parent->appendChild(&m_nodes.at(child));
                }
            }
        }
    }

    rootItem()->sortChildren();

    updateSummary();

    endResetModel();
}

void ReplayGainModel::updateSummary()
{
    if(!m_nodes.contains(u"Summary"_s)) {
        return;
    }
    if(!m_nodes.contains(u"Details"_s)) {
        return;
    }

    auto& summary           = m_nodes.at(u"Summary"_s);
    auto& parent            = m_nodes.at(u"Details"_s);
    const auto summaryItems = summary.children();
    const auto children     = parent.children();

    ReplayGainItem::RGValues values;
    for(const auto& child : children) {
        values[ReplayGainItem::TrackGain].emplace(child->trackGain());
        values[ReplayGainItem::TrackPeak].emplace(child->trackPeak());
        values[ReplayGainItem::AlbumGain].emplace(child->albumGain());
        values[ReplayGainItem::AlbumPeak].emplace(child->albumPeak());
    }

    for(auto* summaryRow : summaryItems) {
        if(summaryRow->summaryFunc()) {
            summaryRow->summaryFunc()(summaryRow, values);
        }
    }
}
} // namespace Fooyin
