/*
 * Fooyin
 * Copyright 2022, Luke Taylor <LukeT1@proton.me>
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

#include "filtermodel.h"

#include "core/library/musiclibrary.h"
#include "filteritem.h"

#include <QSize>

namespace Library {
FilterModel::FilterModel(Filters::FilterType type, int index, MusicLibrary* library, QObject* parent)
    : QAbstractListModel(parent)
    , m_root(new FilterItem())
    , m_type(type)
    , m_index(index)
    , m_library(library)
{ }

FilterModel::~FilterModel() = default;

void FilterModel::setType(Filters::FilterType type)
{
    m_type = type;
    // reset(-1);
}

int FilterModel::index() const
{
    return m_index;
}

void FilterModel::setIndex(int index)
{
    m_index = index;
}

QVariant FilterModel::data(const QModelIndex& index, int role) const
{
    if(!index.isValid() && !checkIndex(index))
    {
        return {};
    }

    auto* item = static_cast<FilterItem*>(index.internalPointer());

    switch(role)
    {
        case(Qt::DisplayRole): {
            return item->data(FilterRole::Name).toString();
        }
        case(FilterRole::Id): {
            return item->data(FilterRole::Id).toInt();
        }
        default: {
            return {};
        }
    }
}

Qt::ItemFlags FilterModel::flags(const QModelIndex& index) const
{
    if(!index.isValid())
    {
        return Qt::NoItemFlags;
    }

    return QAbstractListModel::flags(index);
}

QVariant FilterModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    Q_UNUSED(section)
    if(orientation == Qt::Orientation::Vertical)
    {
        return {};
    }

    if(role == Qt::TextAlignmentRole)
    {
        return (Qt::AlignHCenter);
    }

    if(role != Qt::DisplayRole)
    {
        return {};
    }

    switch(m_type)
    {
        case(Filters::FilterType::Genre):
            return "Genre";
        case(Filters::FilterType::Year):
            return "Year";
        case(Filters::FilterType::AlbumArtist):
            return "Album Artist";
        case(Filters::FilterType::Artist):
            return "Artist";
        case(Filters::FilterType::Album):
            return "Album";
        default:
            return {};
    }
}

QModelIndex FilterModel::index(int row, int column, const QModelIndex& parent) const
{
    if(!hasIndex(row, column, parent))
    {
        return {};
    }

    FilterItem* parentItem = m_root.get();

    FilterItem* childItem = parentItem->child(row);
    if(childItem)
    {
        return createIndex(row, column, childItem);
    }
    return {};
}

QModelIndex FilterModel::parent(const QModelIndex& index) const
{
    Q_UNUSED(index)
    return {};
}

int FilterModel::rowCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent)
    return m_root->childCount();
}

int FilterModel::columnCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent)
    return m_root->columnCount();
}

Library::MusicLibrary* FilterModel::library()
{
    return m_library;
}

QHash<int, QByteArray> FilterModel::roleNames() const
{
    auto roles = QAbstractItemModel::roleNames();

    roles.insert(+FilterRole::Id, "ID");

    return roles;
}

QModelIndexList FilterModel::match(const QModelIndex& start, int role, const QVariant& value, int hits,
                                   Qt::MatchFlags flags) const
{
    if(role != FilterRole::Id)
    {
        return QAbstractItemModel::match(start, role, value, hits, flags);
    }

    QModelIndexList indexes{};
    for(int i = 0; i < rowCount(start); ++i)
    {
        const auto child = index(i, 0, start);
        const auto data = child.data(role);
        if(data.toInt() == value.toInt())
        {
            indexes.append(child);
        }
    }
    return indexes;
}

void FilterModel::reload(const FilterList& result)
{
    beginResetModel();
    beginReset();
    setupModelData(result);
    endResetModel();
}

void FilterModel::beginReset()
{
    m_root.reset();
    m_root = std::make_unique<FilterItem>();
}

void FilterModel::setupModelData(const FilterList& items)
{
    m_root->appendChild(new FilterItem(-1, QString("All (%1)").arg(items.size()), m_root.get()));
    if(!items.isEmpty())
    {
        for(const auto& item : items)
        {
            auto* filterItem = new FilterItem(item.id, item.name, m_root.get());
            m_root->appendChild(filterItem);
        }
    }
}
} // namespace Library
