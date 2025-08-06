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

#include "artworkmodel.h"

#include "sources/artworksource.h"

#include <gui/guiconstants.h>
#include <utils/utils.h>

namespace Fooyin {
void ArtworkModel::addPendingCover(const SearchResult& result)
{
    const int row = rowCount({});
    beginInsertRows({}, row, row);
    m_items.emplace_back(result);
    endInsertRows();
}

void ArtworkModel::loadCover(const QUrl& url, const ArtworkResult& result)
{
    auto itemIt = std::ranges::find(m_items, url, &ArtworkItem::url);
    if(itemIt == m_items.cend()) {
        return;
    }

    itemIt->load(result);
    invalidateData();
}

void ArtworkModel::updateCoverProgress(const QUrl& url, int progress)
{
    auto itemIt = std::ranges::find(m_items, url, &ArtworkItem::url);
    if(itemIt == m_items.cend()) {
        return;
    }

    itemIt->setProgress(progress);
    invalidateData();
}

void ArtworkModel::removeCover(const QUrl& url)
{
    auto itemIt = std::ranges::find(m_items, url, &ArtworkItem::url);
    if(itemIt == m_items.cend()) {
        return;
    }

    const int row = static_cast<int>(std::ranges::distance(m_items.cbegin(), itemIt));

    beginRemoveRows({}, row, row);
    m_items.erase(itemIt);
    endRemoveRows();
}

void ArtworkModel::clear()
{
    beginResetModel();
    m_items.clear();
    endResetModel();
}

Qt::ItemFlags ArtworkModel::flags(const QModelIndex& index) const
{
    Qt::ItemFlags defaultFlags = QAbstractListModel::flags(index);

    if(index.data(ArtworkItem::IsLoaded).toBool()) {
        defaultFlags |= Qt::ItemIsSelectable;
    }
    else {
        defaultFlags &= ~Qt::ItemIsSelectable;
    }

    return defaultFlags;
}

int ArtworkModel::rowCount(const QModelIndex& /*parent*/) const
{
    return static_cast<int>(m_items.size());
}

QVariant ArtworkModel::data(const QModelIndex& index, int role) const
{
    if(!checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return {};
    }

    const auto& item = m_items.at(index.row());

    if(role == Qt::DisplayRole) {
        return item.title();
    }
    if(role == Qt::DecorationRole) {
        return item.thumbnail();
    }
    if(role == ArtworkItem::Result) {
        return QVariant::fromValue(item.result());
    }
    if(role == ArtworkItem::Caption) {
        return item.isLoaded() ? QVariant::fromValue(item.size()) : item.progress();
    }
    if(role == ArtworkItem::IsLoaded) {
        return item.isLoaded();
    }

    return {};
}

void ArtworkModel::invalidateData()
{
    beginResetModel();
    endResetModel();
}
} // namespace Fooyin
