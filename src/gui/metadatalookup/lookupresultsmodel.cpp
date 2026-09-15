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

#include "lookupresultsmodel.h"

using namespace Qt::StringLiterals;

namespace Fooyin {
QVariant LookupResultsModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(orientation == Qt::Orientation::Vertical) {
        return {};
    }

    if(role == Qt::TextAlignmentRole) {
        return Qt::AlignCenter;
    }

    if(role != Qt::DisplayRole) {
        return {};
    }

    switch(section) {
        case 0:
            return tr("Release");
        case 1:
            return tr("Artist");
        case 2:
            return tr("Date/Country");
        case 3:
            return tr("Format");
        case 4:
            return tr("Discs");
        default:
            return {};
    }
}

QVariant LookupResultsModel::data(const QModelIndex& index, int role) const
{
    if(!index.isValid() || role != Qt::DisplayRole || std::cmp_greater_equal(index.row(), m_results.size())) {
        return {};
    }

    const ReleaseSummary& release = m_results.at(index.row());

    switch(index.column()) {
        case 0:
            return release.title;
        case 1:
            return artistCreditText(release.artistCredit);
        case 2:
            if(release.date.isEmpty()) {
                return release.country;
            }
            return release.country.isEmpty() ? release.date : u"%1/%2"_s.arg(release.date, release.country);
        case 3:
            return release.formats.join(u", "_s);
        case 4:
            return release.discCount > 0 ? QVariant{release.discCount} : QVariant{};
        default:
            return {};
    }
}

int LookupResultsModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_results.size());
}

int LookupResultsModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : 5;
}

void LookupResultsModel::setResults(std::vector<ReleaseSummary> results)
{
    beginResetModel();
    m_results = std::move(results);
    endResetModel();
}

const ReleaseSummary* LookupResultsModel::releaseAt(int row) const
{
    return row >= 0 && std::cmp_less(row, m_results.size()) ? &m_results.at(row) : nullptr;
}
} // namespace Fooyin
