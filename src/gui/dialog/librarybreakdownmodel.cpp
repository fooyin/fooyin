/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
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

#include "librarybreakdownmodel.h"

#include <utils/stringutils.h>

#include <QLocale>

#include <array>

using namespace Qt::StringLiterals;

namespace Fooyin {
LibraryBreakdownModel::LibraryBreakdownModel(QObject* parent)
    : QAbstractTableModel{parent}
    , m_breakdown{LibraryBreakdown::Codec}
{ }

void LibraryBreakdownModel::setGroups(std::vector<LibraryGroupStatistics> groups, LibraryBreakdown breakdown)
{
    beginResetModel();
    m_groups    = std::move(groups);
    m_breakdown = breakdown;
    endResetModel();
    Q_EMIT headerDataChanged(Qt::Horizontal, 0, 0);
}

int LibraryBreakdownModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_groups.size());
}

int LibraryBreakdownModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : 5;
}

QVariant LibraryBreakdownModel::data(const QModelIndex& index, int role) const
{
    if(!index.isValid() || !checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return {};
    }

    if(role == Qt::TextAlignmentRole) {
        return static_cast<int>((index.column() == 0 ? Qt::AlignLeft : Qt::AlignRight) | Qt::AlignVCenter);
    }

    const auto& group = m_groups.at(index.row());

    if(role == Qt::ToolTipRole) {
        return index.column() == 0 ? QVariant{groupName(group)} : QVariant{};
    }

    if(role != Qt::DisplayRole && role != SortRole) {
        return {};
    }

    if(index.column() == 0) {
        if(role == SortRole) {
            if(m_breakdown == LibraryBreakdown::BitDepth || m_breakdown == LibraryBreakdown::SampleRate) {
                return group.name.toInt();
            }
            if(m_breakdown == LibraryBreakdown::Rating) {
                return group.name.isEmpty() ? -1.0 : group.name.toDouble();
            }
        }
        return groupName(group);
    }

    const std::array values{group.trackCount, group.totalDurationMs, group.totalFileSize, group.totalPlayCount};
    const auto value = values.at(index.column() - 1);

    if(role == SortRole) {
        return QVariant::fromValue(value);
    }
    if(index.column() == 2) {
        return Utils::msToString(value);
    }
    if(index.column() == 3) {
        return Utils::formatFileSize(value);
    }

    return QLocale{}.toString(static_cast<qulonglong>(value));
}

QVariant LibraryBreakdownModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(orientation != Qt::Horizontal || role != Qt::DisplayRole || section < 0 || section >= columnCount({})) {
        return {};
    }

    if(section == 0) {
        switch(m_breakdown) {
            case LibraryBreakdown::Codec:
                return tr("Codec");
            case LibraryBreakdown::BitDepth:
                return tr("Bit depth");
            case LibraryBreakdown::SampleRate:
                return tr("Sample rate");
            case LibraryBreakdown::Folder:
                return tr("Library folder");
            case LibraryBreakdown::Rating:
                return tr("Rating");
            case LibraryBreakdown::Custom:
                return tr("Group");
        }
        return {};
    }

    const std::array headers{tr("Tracks"), tr("Duration"), tr("Storage"), tr("Play count")};
    return headers.at(section - 1);
}

QString LibraryBreakdownModel::groupName(const LibraryGroupStatistics& group) const
{
    switch(m_breakdown) {
        case LibraryBreakdown::BitDepth:
            return group.name.toInt() > 0 ? u"%1-bit"_s.arg(group.name.toInt()) : tr("Unknown");
        case LibraryBreakdown::SampleRate:
            return group.name.toInt() > 0 ? u"%1 Hz"_s.arg(group.name.toInt()) : tr("Unknown");
        case LibraryBreakdown::Rating:
            return group.name.isEmpty() ? tr("Unrated")
                                        : tr("%1 out of 5").arg(QString::number(group.name.toDouble(), 'g', 3));
        default:
            return group.name.isEmpty() ? tr("Unknown") : group.name;
    }
}
} // namespace Fooyin

#include "moc_librarybreakdownmodel.cpp"
