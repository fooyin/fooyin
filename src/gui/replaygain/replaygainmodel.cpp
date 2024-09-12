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

constexpr auto HeaderFontDelta = 2;

namespace Fooyin {
ReplayGainModel::ReplayGainModel(QObject* parent)
    : TreeModel{parent}
{
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

TrackList ReplayGainModel::applyChanges()
{
    TrackList tracks;

    for(auto& [_, item] : m_nodes) {
        if(item.applyChanges()) {
            tracks.emplace_back(item.track());
        }
    }

    emit dataChanged({}, {}, {Qt::FontRole});

    return tracks;
}

Qt::ItemFlags ReplayGainModel::flags(const QModelIndex& index) const
{
    if(!index.isValid()) {
        return Qt::NoItemFlags;
    }

    auto flags = TreeModel::flags(index);

    if(index.data(ReplayGainItem::Type).toInt() == ReplayGainItem::Entry) {
        flags |= Qt::ItemNeverHasChildren;
        if(index.column() > 0) {
            flags |= Qt::ItemIsEditable;
        }
    }

    return flags;
}

QVariant ReplayGainModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(role == Qt::TextAlignmentRole) {
        return (Qt::AlignHCenter);
    }

    if(role != Qt::DisplayRole || orientation == Qt::Orientation::Vertical) {
        return {};
    }

    switch(section) {
        case(0):
            return tr("Name");
        case(1):
            return tr("Track Gain");
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
    return 5;
}

QVariant ReplayGainModel::data(const QModelIndex& index, int role) const
{
    if(!checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return {};
    }

    const auto* item                    = static_cast<ReplayGainItem*>(index.internalPointer());
    const ReplayGainItem::ItemType type = item->type();

    if(role == ReplayGainItem::Type) {
        return type;
    }

    if(role == Qt::FontRole) {
        return type == ReplayGainItem::Header ? m_headerFont : item->font();
    }

    if(role == Qt::EditRole) {
        switch(index.column()) {
            case(1):
                return QString::number(item->trackGain(), 'f', 2);
            case(2):
                return QString::number(item->trackPeak(), 'f', 6);
            case(3):
                return QString::number(item->albumGain(), 'f', 2);
            case(4):
                return QString::number(item->albumPeak(), 'f', 6);
            default:
                return {};
        }
    }

    if(role != Qt::DisplayRole) {
        return {};
    }

    switch(index.column()) {
        case(0):
            return item->name();
        case(1):
            return QStringLiteral("%1 dB").arg(QString::number(item->trackGain(), 'f', 2));
        case(2):
            return QString::number(item->trackPeak(), 'f', 6);
        case(3):
            return QStringLiteral("%1 dB").arg(QString::number(item->albumGain(), 'f', 2));
        case(4):
            return QString::number(item->albumPeak(), 'f', 6);
        default:
            break;
    }

    return {};
}

bool ReplayGainModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if(m_tracks.empty()) {
        return false;
    }

    if(role != Qt::EditRole) {
        return false;
    }

    const int column = index.column();
    auto* item       = itemForIndex(index);

    if(column == 0) {
        return false;
    }

    const auto setValue = value.toFloat();

    switch(column) {
        case(1):
            if(!item->setTrackGain(setValue)) {
                item->setStatus(ReplayGainItem::None);
                return false;
            }
            break;
        case(2):
            if(!item->setTrackPeak(setValue)) {
                item->setStatus(ReplayGainItem::None);
                return false;
            }
            break;
        case(3):
            if(!item->setAlbumGain(setValue)) {
                item->setStatus(ReplayGainItem::None);
                return false;
            }
            break;
        case(4):
            if(!item->setAlbumPeak(setValue)) {
                item->setStatus(ReplayGainItem::None);
                return false;
            }
            break;
        default:
            break;
    }

    emit dataChanged(index, index);

    return true;
}

void ReplayGainModel::populate(const RGInfoData& data)
{
    beginResetModel();
    resetRoot();
    m_nodes.clear();

    m_nodes = data.nodes;

    for(const auto& [parentKey, children] : data.parents) {
        ReplayGainItem* parent{nullptr};

        if(parentKey == u"Root") {
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

    endResetModel();
}
} // namespace Fooyin
