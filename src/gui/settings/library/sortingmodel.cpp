/*
 * Fooyin
 * Copyright 2022-2023, Luke Taylor <LukeT1@proton.me>
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

#include <core/library/sortingregistry.h>

namespace Fy::Gui::Settings {
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

SortingModel::SortingModel(Core::Library::SortingRegistry* sortRegistry, QObject* parent)
    : TableModel{parent}
    , m_sortRegistry{sortRegistry}
{ }

void SortingModel::populate()
{
    beginResetModel();
    reset();

    const auto& sortScripts = m_sortRegistry->items();

    for(const auto& [index, sortScript] : sortScripts) {
        if(!sortScript.isValid()) {
            continue;
        }
        SortingItem* parent = rootItem();
        SortingItem* child  = &m_nodes.emplace(index, SortingItem{sortScript, parent}).first->second;
        parent->appendChild(child);
    }

    endResetModel();
}

void SortingModel::addNewSortScript()
{
    const int index = static_cast<int>(m_nodes.size());

    SortScript sortScript;
    sortScript.index = index;

    auto* parent = rootItem();

    SortingItem* item = &m_nodes.emplace(index, SortingItem{sortScript, parent}).first->second;

    item->setStatus(SortingItem::Added);

    const int row = parent->childCount();
    beginInsertRows({}, row, row);
    parent->appendChild(item);
    endInsertRows();
}

void SortingModel::markForRemoval(const SortScript& sortScript)
{
    SortingItem* item = &m_nodes.at(sortScript.index);

    if(item->status() == SortingItem::Added) {
        beginRemoveRows({}, item->row(), item->row());
        rootItem()->removeChild(item->row());
        endRemoveRows();

        removeSortScript(sortScript.index);
    }
    else {
        item->setStatus(SortingItem::Removed);
        emit dataChanged({}, {}, {Qt::FontRole});
    }
}

void SortingModel::markForChange(const SortScript& sortScript)
{
    SortingItem* item = &m_nodes.at(sortScript.index);
    item->changeSort(sortScript);
    emit dataChanged({}, {}, {Qt::FontRole});

    if(item->status() == SortingItem::None) {
        item->setStatus(SortingItem::Changed);
    }
}

void SortingModel::processQueue()
{
    std::vector<SortingItem> sortScriptsToRemove;

    for(auto& [index, node] : m_nodes) {
        const SortingItem::ItemStatus status = node.status();
        const SortScript sortScript          = node.sortScript();

        switch(status) {
            case(SortingItem::Added): {
                const auto addedSort = m_sortRegistry->addItem(sortScript);
                if(addedSort.isValid()) {
                    node.changeSort(addedSort);
                    node.setStatus(SortingItem::None);
                }
                else {
                    qWarning() << QString{"Sorting %1 could not be added"}.arg(sortScript.name);
                }
                break;
            }
            case(SortingItem::Removed): {
                if(m_sortRegistry->removeByIndex(sortScript.index)) {
                    beginRemoveRows({}, node.row(), node.row());
                    rootItem()->removeChild(node.row());
                    endRemoveRows();
                    sortScriptsToRemove.push_back(node);
                }
                else {
                    qWarning() << QString{"Sorting (%1) could not be removed"}.arg(sortScript.name);
                }
                break;
            }
            case(SortingItem::Changed): {
                if(m_sortRegistry->changeItem(sortScript)) {
                    node.setStatus(SortingItem::None);
                }
                else {
                    qWarning() << QString{"Sorting (%1) could not be changed"}.arg(sortScript.name);
                }
                break;
            }
            case(SortingItem::None):
                break;
        }
    }
    for(const auto& item : sortScriptsToRemove) {
        removeSortScript(item.sortScript().index);
    }
}

Qt::ItemFlags SortingModel::flags(const QModelIndex& index) const
{
    if(!index.isValid()) {
        return Qt::NoItemFlags;
    }

    auto flags = TableModel::flags(index);
    flags |= Qt::ItemIsEditable;

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
            return "Index";
        case(1):
            return "Name";
        case(2):
            return "Sort Script";
    }
    return {};
}

QVariant SortingModel::data(const QModelIndex& index, int role) const
{
    if(role != Qt::DisplayRole && role != Qt::EditRole && role != Qt::FontRole && role != Qt::UserRole) {
        return {};
    }

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

    switch(index.column()) {
        case(0):
            return item->sortScript().index;
        case(1): {
            const QString& name = item->sortScript().name;
            return !name.isEmpty() ? name : "<enter name here>";
        }
        case(2): {
            const QString& field = item->sortScript().script;
            return !field.isEmpty() ? field : "<enter sort script here>";
        }
    }

    return {};
}

bool SortingModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if(role != Qt::EditRole) {
        return false;
    }

    const auto* item = static_cast<SortingItem*>(index.internalPointer());
    auto group       = item->sortScript();

    switch(index.column()) {
        case(1): {
            if(group.name == value.toString()) {
                return false;
            }
            group.name = value.toString();
            break;
        }
        case(2): {
            if(group.script == value.toString()) {
                return false;
            }
            group.script = value.toString();
            break;
        }
        case(0):
            break;
    }

    markForChange(group);

    return true;
}

int SortingModel::columnCount(const QModelIndex& /*parent*/) const
{
    return 3;
}

void SortingModel::reset()
{
    resetRoot();
    m_nodes.clear();
}

void SortingModel::removeSortScript(int index)
{
    if(!m_nodes.contains(index)) {
        return;
    }
    m_nodes.erase(index);
}
} // namespace Fy::Gui::Settings
