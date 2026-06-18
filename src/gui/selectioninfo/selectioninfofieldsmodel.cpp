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

#include "selectioninfofieldsmodel.h"

#include "selectioninfofieldregistry.h"

#include <utils/helpers.h>

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(SELECTION_INFO_FIELDS, "fy.selectioninfo.fields")

using namespace Qt::StringLiterals;

namespace Fooyin {
SelectionInfoFieldItem::SelectionInfoFieldItem()
    : SelectionInfoFieldItem{{}, nullptr}
{ }

SelectionInfoFieldItem::SelectionInfoFieldItem(SelectionInfoField field, SelectionInfoFieldItem* parent)
    : TreeStatusItem{parent}
    , m_field{std::move(field)}
{ }

SelectionInfoFieldsModel::SelectionInfoFieldsModel(SelectionInfoFieldRegistry* registry, QObject* parent)
    : ExtendableTableModel{parent}
    , m_registry{registry}
{ }

void SelectionInfoFieldsModel::populate()
{
    beginResetModel();
    m_root = {};
    m_nodes.clear();

    for(const auto& field : m_registry->items()) {
        if(field.name.isEmpty()) {
            continue;
        }
        auto* child = m_nodes.emplace_back(std::make_unique<SelectionInfoFieldItem>(field, &m_root)).get();
        m_root.insertChild(field.index, child);
    }

    endResetModel();
}

void SelectionInfoFieldsModel::processQueue()
{
    std::vector<int> idsToRemove;

    for(auto& node : m_nodes) {
        const auto status        = node->status();
        SelectionInfoField field = node->m_field;

        switch(status) {
            case SelectionInfoFieldItem::Added: {
                if(field.name.isEmpty() || field.scriptField.isEmpty() || hasField(field.scriptField, field.id)) {
                    break;
                }
                const auto addedField = m_registry->addItem(field);
                if(addedField.isValid()) {
                    node->m_field = addedField;
                    node->setStatus(SelectionInfoFieldItem::None);
                }
                else {
                    qCWarning(SELECTION_INFO_FIELDS) << "Field could not be added:" << field.name;
                }
                break;
            }
            case SelectionInfoFieldItem::Removed:
                if(m_registry->removeById(field.id)) {
                    beginRemoveRows({}, node->row(), node->row());
                    m_root.removeChild(node->row());
                    endRemoveRows();
                    idsToRemove.push_back(field.id);
                }
                break;
            case SelectionInfoFieldItem::Changed: {
                if(field.name.isEmpty() || field.scriptField.isEmpty() || hasField(field.scriptField, field.id)) {
                    break;
                }
                if(m_registry->changeItem(field)) {
                    if(const auto item = m_registry->itemById(field.id)) {
                        node->m_field = *item;
                        node->setStatus(SelectionInfoFieldItem::None);
                    }
                }
                break;
            }
            case SelectionInfoFieldItem::None:
                break;
        }
    }

    for(const int id : idsToRemove) {
        std::erase_if(m_nodes, [id](const auto& node) { return node->m_field.id == id; });
    }

    invalidateData();
}

Qt::ItemFlags SelectionInfoFieldsModel::flags(const QModelIndex& index) const
{
    if(!index.isValid()) {
        return Qt::NoItemFlags;
    }

    auto flags = ExtendableTableModel::flags(index);
    flags |= Qt::ItemIsEditable;
    if(index.column() == 1) {
        flags |= Qt::ItemIsUserCheckable;
    }
    return flags;
}

QVariant SelectionInfoFieldsModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(role == Qt::TextAlignmentRole) {
        return Qt::AlignCenter;
    }
    if(role != Qt::DisplayRole || orientation == Qt::Vertical) {
        return {};
    }

    switch(section) {
        case 0:
            return tr("Index");
        case 1:
            return tr("Enabled");
        case 2:
            return tr("Name");
        case 3:
            return tr("Field");
        default:
            return {};
    }
}

QVariant SelectionInfoFieldsModel::data(const QModelIndex& index, int role) const
{
    if(!checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return {};
    }

    const auto* item  = static_cast<SelectionInfoFieldItem*>(index.internalPointer());
    const int column  = index.column();
    const auto& field = item->m_field;

    if(role == Qt::TextAlignmentRole) {
        return column < 2 ? QVariant{Qt::AlignCenter} : QVariant{Qt::AlignVCenter | Qt::AlignLeft};
    }
    if(role == Qt::FontRole) {
        return item->font();
    }
    if(role == Qt::UserRole) {
        return QVariant::fromValue(field);
    }
    if(role == Qt::CheckStateRole && column == 1) {
        return field.enabled ? Qt::Checked : Qt::Unchecked;
    }
    if(role == Qt::DisplayRole || role == Qt::EditRole) {
        switch(column) {
            case 0:
                return field.index;
            case 2:
                return field.name.isEmpty() ? u"<enter name here>"_s : field.name;
            case 3:
                return field.scriptField.isEmpty() ? u"<enter field here>"_s : field.scriptField;
            default:
                break;
        }
    }
    return {};
}

bool SelectionInfoFieldsModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if(role != Qt::EditRole && role != Qt::CheckStateRole) {
        return false;
    }

    auto* item = static_cast<SelectionInfoFieldItem*>(index.internalPointer());
    SelectionInfoField field{item->m_field};

    if(role == Qt::CheckStateRole && index.column() == 1) {
        field.enabled = value.value<Qt::CheckState>() == Qt::Checked;
    }
    else if(index.column() == 2) {
        const QString name = value.toString().trimmed();
        if(name.isEmpty() || name == u"<enter name here>"_s || field.name == name) {
            if(item->status() == SelectionInfoFieldItem::Added) {
                Q_EMIT pendingRowCancelled();
            }
            return false;
        }
        field.name = name;
    }
    else if(index.column() == 3) {
        QString scriptField = value.toString().trimmed();
        scriptField.remove(u'%');
        if(scriptField.isEmpty() || scriptField == u"<enter field here>"_s || field.scriptField == scriptField) {
            return false;
        }
        field.scriptField = scriptField;
    }
    else {
        return false;
    }

    if(item->status() == SelectionInfoFieldItem::None) {
        item->setStatus(SelectionInfoFieldItem::Changed);
    }

    item->m_field = field;

    Q_EMIT dataChanged(index.siblingAtColumn(0), index.siblingAtColumn(columnCount({}) - 1),
                       {Qt::FontRole, Qt::DisplayRole, Qt::CheckStateRole});

    return true;
}

QModelIndex SelectionInfoFieldsModel::index(int row, int column, const QModelIndex& parent) const
{
    if(!hasIndex(row, column, parent)) {
        return {};
    }
    return createIndex(row, column, m_root.child(row));
}

int SelectionInfoFieldsModel::rowCount(const QModelIndex& /*parent*/) const
{
    return m_root.childCount();
}

int SelectionInfoFieldsModel::columnCount(const QModelIndex& /*parent*/) const
{
    return 4;
}

bool SelectionInfoFieldsModel::removeRows(int row, int count, const QModelIndex& /*parent*/)
{
    for(int i{row}; i < row + count; ++i) {
        auto* item = m_root.child(i);
        if(!item) {
            return false;
        }
        if(item->status() == SelectionInfoFieldItem::Added) {
            beginRemoveRows({}, i, i);
            m_root.removeChild(i);
            m_nodes.erase(m_nodes.begin() + i);
            endRemoveRows();
        }
        else {
            item->setStatus(SelectionInfoFieldItem::Removed);
        }
    }
    invalidateData();
    return true;
}

void SelectionInfoFieldsModel::addPendingRow()
{
    SelectionInfoField field;
    field.index = static_cast<int>(m_nodes.size());

    auto* item = m_nodes.emplace_back(std::make_unique<SelectionInfoFieldItem>(field, &m_root)).get();
    item->setStatus(SelectionInfoFieldItem::Added);

    const int row = m_root.childCount();
    beginInsertRows({}, row, row);
    m_root.appendChild(item);
    endInsertRows();
}

void SelectionInfoFieldsModel::removePendingRow()
{
    const int row = rowCount({}) - 1;
    beginRemoveRows({}, row, row);
    m_root.removeChild(row);
    m_nodes.pop_back();
    endRemoveRows();
}

void SelectionInfoFieldsModel::moveRowsUp(const QModelIndexList& indexes)
{
    const int destination = indexes.front().row() - 1;
    if(destination < 0) {
        return;
    }

    const int first = indexes.front().row();
    const int last  = indexes.back().row();
    beginMoveRows({}, first, last, {}, destination);

    int current = destination;
    for(const auto& index : indexes) {
        auto* item = static_cast<SelectionInfoFieldItem*>(index.internalPointer());
        item->resetRow();
        const int oldRow = item->row();
        m_root.moveChild(oldRow, current);
        Utils::move(m_nodes, oldRow, current);
        ++current;
    }

    m_root.resetChildren();
    adjustIndices();
    endMoveRows();
}

void SelectionInfoFieldsModel::moveRowsDown(const QModelIndexList& indexes)
{
    const int destination = indexes.back().row() + 2;
    if(destination > rowCount({})) {
        return;
    }

    const int first = indexes.front().row();
    const int last  = indexes.back().row();
    beginMoveRows({}, first, last, {}, destination);

    for(const auto& index : indexes) {
        auto* item = static_cast<SelectionInfoFieldItem*>(index.internalPointer());
        item->resetRow();
        const int oldRow = item->row();
        m_root.moveChild(oldRow, destination);
        Utils::move(m_nodes, oldRow, destination - 1);
    }

    m_root.resetChildren();
    adjustIndices();
    endMoveRows();
}

bool SelectionInfoFieldsModel::hasField(const QString& field, int id) const
{
    return std::ranges::any_of(m_nodes, [&field, id](const auto& node) {
        const auto modelField = node->m_field;
        return modelField.id != id && modelField.scriptField.compare(field, Qt::CaseInsensitive) == 0;
    });
}

void SelectionInfoFieldsModel::adjustIndices()
{
    for(int i{0}; const auto& item : m_nodes) {
        auto field = item->m_field;
        if(std::exchange(field.index, i++) != field.index) {
            item->m_field = field;
            item->setStatus(SelectionInfoFieldItem::Changed);
        }
    }
}
} // namespace Fooyin
