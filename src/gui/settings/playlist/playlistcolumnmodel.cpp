/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
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

#include "playlistcolumnmodel.h"

#include "playlist/playlistcolumn.h"
#include "playlist/playlistcolumnregistry.h"

#include <utils/treestatusitem.h>

#include <QApplication>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(PL_COLMOD, "fy.playlistcolmodel")

using namespace Qt::StringLiterals;

namespace {
QString normaliseWriteField(QString field)
{
    field = field.trimmed();

    if(field.isEmpty()) {
        return {};
    }

    if(field.size() >= 2 && field.startsWith(u'%') && field.endsWith(u'%')) {
        field = field.sliced(1, field.size() - 2).trimmed();
    }

    return field;
}
} // namespace

namespace Fooyin {
ColumnItem::ColumnItem()
    : ColumnItem{{}, nullptr}
{ }

ColumnItem::ColumnItem(PlaylistColumn column, ColumnItem* parent)
    : TreeStatusItem{parent}
    , m_column{std::move(column)}
{ }

const PlaylistColumn& ColumnItem::column() const
{
    return m_column;
}

void ColumnItem::changeColumn(const PlaylistColumn& column)
{
    m_column = column;
}

PlaylistColumnModel::PlaylistColumnModel(PlaylistColumnRegistry* columnsRegistry, QObject* parent)
    : ExtendableTableModel{parent}
    , m_columnsRegistry{columnsRegistry}
{ }

void PlaylistColumnModel::populate()
{
    beginResetModel();
    m_root = {};
    m_nodes.clear();

    const auto& columns = m_columnsRegistry->items();

    for(const auto& column : columns) {
        if(column.name.isEmpty()) {
            continue;
        }
        ColumnItem* child = &m_nodes.emplace(column.index, ColumnItem{column, &m_root}).first->second;
        m_root.insertChild(column.index, child);
    }

    endResetModel();
}

void PlaylistColumnModel::processQueue()
{
    std::vector<int> columnsToRemove;

    for(auto& [index, node] : m_nodes) {
        const ColumnItem::ItemStatus status = node.status();
        const PlaylistColumn column         = node.column();

        switch(status) {
            case ColumnItem::Added: {
                if(column.field.isEmpty()) {
                    break;
                }

                const PlaylistColumn addedColumn = m_columnsRegistry->addItem(column);
                if(addedColumn.isValid()) {
                    node.changeColumn(addedColumn);
                    node.setStatus(ColumnItem::None);
                }
                else {
                    qCWarning(PL_COLMOD) << "Column could not be added:" << column.name;
                }
                break;
            }
            case ColumnItem::Removed: {
                if(m_columnsRegistry->removeById(column.id)) {
                    beginRemoveRows({}, node.row(), node.row());
                    m_root.removeChild(node.row());
                    endRemoveRows();
                    columnsToRemove.push_back(index);
                }
                else {
                    qCWarning(PL_COLMOD) << "Column could not be removed:" << column.name;
                }
                break;
            }
            case ColumnItem::Changed: {
                if(m_columnsRegistry->changeItem(column)) {
                    if(const auto changedItem = m_columnsRegistry->itemById(column.id)) {
                        node.changeColumn(changedItem.value());
                        node.setStatus(ColumnItem::None);
                    }
                }
                else {
                    qCWarning(PL_COLMOD) << "Column could not be changed:" << column.name;
                }
                break;
            }
            case ColumnItem::None:
                break;
        }
    }

    for(const auto& index : columnsToRemove) {
        m_nodes.erase(index);
    }

    invalidateData();
}

Qt::ItemFlags PlaylistColumnModel::flags(const QModelIndex& index) const
{
    if(!index.isValid()) {
        return Qt::NoItemFlags;
    }

    Qt::ItemFlags flags = ExtendableTableModel::flags(index);

    switch(index.column()) {
        case 0:
            flags |= Qt::ItemIsUserCheckable;
            break;
        case 2:
        case 3:
        case 4:
            flags |= Qt::ItemIsEditable;
            break;
        default:
            break;
    }

    return flags;
}

QVariant PlaylistColumnModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(role == Qt::TextAlignmentRole) {
        return Qt::AlignCenter;
    }

    if(role != Qt::DisplayRole || orientation == Qt::Orientation::Vertical) {
        return {};
    }

    switch(section) {
        case 0:
            return tr("Enabled");
        case 1:
            return tr("Index");
        case 2:
            return tr("Name");
        case 3:
            return tr("Display Script");
        case 4:
            return tr("Write Field");
        default:
            break;
    }

    return {};
}

QVariant PlaylistColumnModel::data(const QModelIndex& index, int role) const
{
    if(!checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return {};
    }

    auto* item = static_cast<ColumnItem*>(index.internalPointer());

    if(role == Qt::FontRole) {
        return item->font();
    }

    if(role == Qt::TextAlignmentRole && index.column() == 0) {
        return Qt::AlignCenter;
    }

    if(role == Qt::UserRole) {
        return QVariant::fromValue(item->column());
    }

    const PlaylistColumn& columnData = item->column();
    const int column                 = index.column();
    const QString& writeField        = columnData.writeField;

    if(column == 0 && role == Qt::CheckStateRole) {
        return columnData.enabled ? Qt::Checked : Qt::Unchecked;
    }

    if(column == 4 && role == Qt::ForegroundRole && writeField.isEmpty()) {
        return QApplication::palette().color(QPalette::PlaceholderText);
    }

    if(role == Qt::DisplayRole || role == Qt::EditRole) {
        switch(column) {
            case 1: {
                return columnData.index;
            }
            case 2: {
                const QString& name = columnData.name;
                return !name.isEmpty() ? name : u"<enter name here>"_s;
            }
            case 3: {
                const QString& field = columnData.field;
                return !field.isEmpty() ? field : u"<enter field here>"_s;
            }
            case 4: {
                return !writeField.isEmpty() ? writeField : tr("No write field");
            }
            default:
                break;
        }
    }

    return {};
}

bool PlaylistColumnModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if(role != Qt::EditRole && role != Qt::CheckStateRole) {
        return false;
    }

    auto* item            = static_cast<ColumnItem*>(index.internalPointer());
    PlaylistColumn column = item->column();

    if(index.column() == 0 && role == Qt::CheckStateRole) {
        const bool enabled = value.value<Qt::CheckState>() == Qt::Checked;
        if(column.enabled == enabled) {
            return false;
        }
        column.enabled = enabled;
    }
    else if(role == Qt::EditRole) {
        switch(index.column()) {
            case 2: {
                if(value.toString() == u"<enter name here>"_s || column.name == value.toString()) {
                    if(item->status() == ColumnItem::Added) {
                        emit pendingRowCancelled();
                    }
                    return false;
                }
                column.name = value.toString();
                break;
            }
            case 3: {
                if(column.field == value.toString()) {
                    return false;
                }
                column.field = value.toString();
                break;
            }
            case 4: {
                const QString writeField = normaliseWriteField(value.toString());
                if(column.writeField == writeField || writeField == tr("No write field")) {
                    return false;
                }
                column.writeField = writeField;
                break;
            }
            default:
                return false;
        }
    }
    else {
        return false;
    }

    if(item->status() == ColumnItem::None) {
        item->setStatus(ColumnItem::Changed);
    }

    item->changeColumn(column);
    emit dataChanged(index, index, {Qt::FontRole, Qt::DisplayRole, Qt::CheckStateRole, Qt::UserRole});

    return true;
}

QModelIndex PlaylistColumnModel::index(int row, int column, const QModelIndex& parent) const
{
    if(!hasIndex(row, column, parent)) {
        return {};
    }

    const ColumnItem* item = m_root.child(row);
    return createIndex(row, column, item);
}

int PlaylistColumnModel::rowCount(const QModelIndex& /*parent*/) const
{
    return m_root.childCount();
}

int PlaylistColumnModel::columnCount(const QModelIndex& /*parent*/) const
{
    return 5;
}

bool PlaylistColumnModel::removeRows(int row, int count, const QModelIndex& /*parent*/)
{
    for(int i{row}; i < row + count; ++i) {
        const QModelIndex& index = this->index(i, 0, {});

        if(!index.isValid()) {
            return false;
        }

        auto* item = static_cast<ColumnItem*>(index.internalPointer());
        if(item) {
            if(item->status() == ColumnItem::Added) {
                beginRemoveRows({}, i, i);
                m_root.removeChild(i);
                endRemoveRows();
                m_nodes.erase(item->column().index);
            }
            else if(!item->column().isDefault) {
                item->setStatus(ColumnItem::Removed);
            }
        }
    }

    invalidateData();

    return true;
}

void PlaylistColumnModel::addPendingRow()
{
    const int index = static_cast<int>(m_nodes.size());

    PlaylistColumn column;
    column.index = index;

    ColumnItem* item = &m_nodes.emplace(index, ColumnItem{column, &m_root}).first->second;

    item->setStatus(ColumnItem::Added);

    const int row = m_root.childCount();
    beginInsertRows({}, row, row);
    m_root.appendChild(item);
    endInsertRows();
}

void PlaylistColumnModel::removePendingRow()
{
    const int row = rowCount({}) - 1;
    beginRemoveRows({}, row, row);
    m_root.removeChild(row);
    endRemoveRows();
}
} // namespace Fooyin
