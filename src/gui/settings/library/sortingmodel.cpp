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

#include "sortingmodel.h"

#include "core/library/sortingregistry.h"

namespace Fooyin {
SortingItem::SortingItem()
    : SortingItem{{}, nullptr}
{ }

SortingItem::SortingItem(SortScript sortScript, SortingItem* parent)
    : TreeStatusItem{parent}
    , m_sortScript{std::move(sortScript)}
{ }

SortScript SortingItem::sortScript() const
{
    return m_sortScript;
}

void SortingItem::changeSort(SortScript sortScript)
{
    m_sortScript = std::move(sortScript);
}

SortingModel::SortingModel(SortingRegistry* sortRegistry, QObject* parent)
    : ExtendableTableModel{parent}
    , m_sortRegistry{sortRegistry}
{ }

void SortingModel::populate()
{
    beginResetModel();
    m_root = {};
    m_nodes.clear();

    const auto& sortScripts = m_sortRegistry->items();

    for(const auto& sortScript : sortScripts) {
        if(!sortScript.isValid()) {
            continue;
        }
        SortingItem* child = &m_nodes.emplace(sortScript.index, SortingItem{sortScript, &m_root}).first->second;
        m_root.appendChild(child);
    }

    endResetModel();
}

void SortingModel::processQueue()
{
    std::vector<SortingItem> sortScriptsToRemove;

    for(auto& [index, node] : m_nodes) {
        const SortingItem::ItemStatus status = node.status();
        const SortScript sortScript          = node.sortScript();

        switch(status) {
            case(SortingItem::Added): {
                if(sortScript.script.isEmpty()) {
                    break;
                }

                const auto addedSort = m_sortRegistry->addItem(sortScript);
                if(addedSort.isValid()) {
                    node.changeSort(addedSort);
                    node.setStatus(SortingItem::None);

                    emit dataChanged({}, {}, {Qt::DisplayRole, Qt::FontRole});
                }
                else {
                    qWarning() << QStringLiteral("Sorting %1 could not be added").arg(sortScript.name);
                }
                break;
            }
            case(SortingItem::Removed): {
                if(m_sortRegistry->removeByIndex(sortScript.index)) {
                    beginRemoveRows({}, node.row(), node.row());
                    m_root.removeChild(node.row());
                    endRemoveRows();
                    sortScriptsToRemove.push_back(node);
                }
                else {
                    qWarning() << QStringLiteral("Sorting %1 could not be removed").arg(sortScript.name);
                }
                break;
            }
            case(SortingItem::Changed): {
                if(m_sortRegistry->changeItem(sortScript)) {
                    if(const auto changedItem = m_sortRegistry->itemById(sortScript.id)) {
                        node.changeSort(changedItem.value());
                        node.setStatus(SortingItem::None);

                        emit dataChanged({}, {}, {Qt::DisplayRole, Qt::FontRole});
                    }
                }
                else {
                    qWarning() << QStringLiteral("Sorting %1 could not be changed").arg(sortScript.name);
                }
                break;
            }
            case(SortingItem::None):
                break;
        }
    }
    for(const auto& item : sortScriptsToRemove) {
        m_nodes.erase(item.sortScript().index);
    }
}

Qt::ItemFlags SortingModel::flags(const QModelIndex& index) const
{
    if(!index.isValid()) {
        return Qt::NoItemFlags;
    }

    auto flags = ExtendableTableModel::flags(index);

    auto* item = static_cast<SortingItem*>(index.internalPointer());
    if(item && !item->sortScript().isDefault) {
        flags |= Qt::ItemIsEditable;
    }

    return flags;
}

QVariant SortingModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(role == Qt::TextAlignmentRole) {
        return (Qt::AlignHCenter);
    }

    if(role != Qt::DisplayRole || orientation == Qt::Orientation::Vertical) {
        return {};
    }

    switch(section) {
        case(0):
            return tr("Index");
        case(1):
            return tr("Name");
        case(2):
            return tr("Sort Script");
        default:
            break;
    }

    return {};
}

QVariant SortingModel::data(const QModelIndex& index, int role) const
{
    if(!checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return {};
    }

    auto* item = static_cast<SortingItem*>(index.internalPointer());

    if(role == Qt::FontRole) {
        return item->font();
    }

    if(role == Qt::UserRole) {
        return QVariant::fromValue(item->sortScript());
    }

    if(role == Qt::DisplayRole || role == Qt::EditRole) {
        switch(index.column()) {
            case(0):
                return item->sortScript().index;
            case(1): {
                const QString& name = item->sortScript().name;
                return !name.isEmpty() ? name : QStringLiteral("<enter name here>");
            }
            case(2): {
                const QString& field = item->sortScript().script;
                return !field.isEmpty() ? field : QStringLiteral("<enter sort script here>");
            }
            default:
                break;
        }
    }

    return {};
}

bool SortingModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if(role != Qt::EditRole) {
        return false;
    }

    auto* item            = static_cast<SortingItem*>(index.internalPointer());
    SortScript sortScript = item->sortScript();

    switch(index.column()) {
        case(1): {
            if(value.toString() == QStringLiteral("<enter name here>") || sortScript.name == value.toString()) {
                if(item->status() == SortingItem::Added) {
                    emit pendingRowCancelled();
                }
                return false;
            }
            sortScript.name = value.toString();
            break;
        }
        case(2): {
            if(sortScript.script == value.toString()) {
                return false;
            }
            sortScript.script = value.toString();
            break;
        }
        default:
            break;
    }

    if(item->status() == SortingItem::None) {
        item->setStatus(SortingItem::Changed);
    }

    item->changeSort(sortScript);
    emit dataChanged({}, {}, {Qt::DisplayRole, Qt::FontRole});

    return true;
}

QModelIndex SortingModel::index(int row, int column, const QModelIndex& parent) const
{
    if(!hasIndex(row, column, parent)) {
        return {};
    }

    SortingItem* item = m_root.child(row);

    return createIndex(row, column, item);
}

int SortingModel::rowCount(const QModelIndex& /*parent*/) const
{
    return m_root.childCount();
}

int SortingModel::columnCount(const QModelIndex& /*parent*/) const
{
    return 3;
}

bool SortingModel::removeRows(int row, int count, const QModelIndex& /*parent*/)
{
    for(int i{row}; i < row + count; ++i) {
        const QModelIndex& index = this->index(i, 0, {});

        if(!index.isValid()) {
            return false;
        }

        auto* item = static_cast<SortingItem*>(index.internalPointer());
        if(item) {
            if(item->status() == SortingItem::Added) {
                beginRemoveRows({}, i, i);
                m_root.removeChild(i);
                endRemoveRows();
                m_nodes.erase(item->sortScript().index);
            }
            else if(!item->sortScript().isDefault) {
                item->setStatus(SortingItem::Removed);
                emit dataChanged({}, {}, {Qt::FontRole});
            }
        }
    }
    return true;
}

void SortingModel::addPendingRow()
{
    const int index = static_cast<int>(m_nodes.size());

    SortScript sortScript;
    sortScript.index = index;

    SortingItem* item = &m_nodes.emplace(index, SortingItem{sortScript, &m_root}).first->second;

    item->setStatus(SortingItem::Added);

    const int row = m_root.childCount();
    beginInsertRows({}, row, row);
    m_root.appendChild(item);
    endInsertRows();
}

void SortingModel::removePendingRow()
{
    const int row = rowCount({}) - 1;
    beginRemoveRows({}, row, row);
    m_root.removeChild(row);
    endRemoveRows();
}
} // namespace Fooyin
