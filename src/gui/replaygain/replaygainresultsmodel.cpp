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

#include "replaygainresultsmodel.h"

namespace Fooyin {
ReplayGainResultsModel::ReplayGainResultsModel(TrackList tracks, QObject* parent)
    : QAbstractTableModel{parent}
    , m_tracks{std::move(tracks)}
{ }

QVariant ReplayGainResultsModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(orientation == Qt::Orientation::Vertical) {
        return {};
    }

    if(role == Qt::TextAlignmentRole) {
        return (Qt::AlignHCenter);
    }

    if(role != Qt::DisplayRole) {
        return {};
    }

    switch(section) {
        case(0):
            return tr("Name");
        case(1):
            return tr("Track Gain");
        case(2):
            return tr("Album Gain");
        case(3):
            return tr("Track Peak");
        case(4):
            return tr("Album Peak");
        default:
            return {};
    }
}

QVariant ReplayGainResultsModel::data(const QModelIndex& index, int role) const
{
    if(!checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return {};
    }

    const int row = index.row();
    if(row < 0 || std::cmp_greater_equal(row, m_tracks.size())) {
        return {};
    }

    if(role == Qt::DisplayRole) {
        const Track& track = m_tracks.at(row);
        switch(index.column()) {
            case(0):
                return track.effectiveTitle();
            case(1):
                return track.hasTrackGain() ? QStringLiteral("%1 dB").arg(QString::number(track.rgTrackGain(), 'f', 2))
                                            : QString{};
            case(2):
                return track.hasAlbumGain() ? QStringLiteral("%1 dB").arg(QString::number(track.rgAlbumGain(), 'f', 2))
                                            : QString{};
            case(3):
                return track.hasTrackPeak() ? QString::number(track.rgTrackPeak(), 'f', 6) : QString{};
            case(4):
                return track.hasAlbumPeak() ? QString::number(track.rgAlbumPeak(), 'f', 6) : QString{};
            default:
                return {};
        }
    }

    return {};
}

int ReplayGainResultsModel::rowCount(const QModelIndex& /*parent*/) const
{
    return static_cast<int>(m_tracks.size());
}

int ReplayGainResultsModel::columnCount(const QModelIndex& /*parent*/) const
{
    return 5;
}
} // namespace Fooyin
