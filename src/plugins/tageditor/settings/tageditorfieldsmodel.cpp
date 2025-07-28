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

#include "tageditorfieldsmodel.h"

#include "settings/tageditorfieldregistry.h"

#include <core/track.h>

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(TAGEDITOR_MODEL, "fy.tageditor")

using namespace Qt::StringLiterals;

namespace Fooyin::TagEditor {
TagEditorFieldItem::TagEditorFieldItem()
    : TagEditorFieldItem{{}, nullptr}
{ }

TagEditorFieldItem::TagEditorFieldItem(TagEditorField field, TagEditorFieldItem* parent)
    : TreeStatusItem{parent}
    , m_field{std::move(field)}
{ }

TagEditorField TagEditorFieldItem::field() const
{
    return m_field;
}

void TagEditorFieldItem::changeField(const TagEditorField& field)
{
    m_field = field;
}

TagEditorFieldsModel::TagEditorFieldsModel(TagEditorFieldRegistry* registry, QObject* parent)
    : ExtendableTableModel{parent}
    , m_fieldRegistry{registry}
{ }

void TagEditorFieldsModel::populate()
{
    beginResetModel();
    m_root = {};
    m_nodes.clear();

    const auto fields = m_fieldRegistry->items();

    for(const auto& field : fields) {
        if(field.name.isEmpty()) {
            continue;
        }
        auto* child = m_nodes.emplace_back(std::make_unique<TagEditorFieldItem>(field, &m_root)).get();
        m_root.insertChild(field.index, child);
    }

    endResetModel();
}

void TagEditorFieldsModel::processQueue()
{
    std::vector<int> rowsToRemove;

    for(auto& node : m_nodes) {
        const auto status    = node->status();
        TagEditorField field = node->field();

        switch(status) {
            case(TagEditorFieldItem::Added): {
                if(field.scriptField.isEmpty() || hasField(field.scriptField, field.id)) {
                    break;
                }

                field.multivalue = Track::isMultiValueTag(field.scriptField);

                const TagEditorField addedField = m_fieldRegistry->addItem(field);
                if(addedField.isValid()) {
                    node->changeField(addedField);
                    node->setStatus(TagEditorFieldItem::None);
                }
                else {
                    qCWarning(TAGEDITOR_MODEL) << "Field could not be added:" << field.name;
                }
                break;
            }
            case(TagEditorFieldItem::Removed): {
                if(m_fieldRegistry->removeById(field.id)) {
                    beginRemoveRows({}, node->row(), node->row());
                    m_root.removeChild(node->row());
                    endRemoveRows();
                    rowsToRemove.push_back(field.index);
                }
                else {
                    qCWarning(TAGEDITOR_MODEL) << "Field could not be removed:" << field.name;
                }
                break;
            }
            case(TagEditorFieldItem::Changed): {
                if(const auto existingItem = m_fieldRegistry->itemById(field.id)) {
                    if(field == existingItem.value()) {
                        node->setStatus(TagEditorFieldItem::None);
                        break;
                    }
                    if(existingItem->scriptField != field.scriptField && hasField(field.scriptField, field.id)) {
                        break;
                    }
                }

                field.multivalue = Track::isMultiValueTag(field.scriptField);

                if(m_fieldRegistry->changeItem(field)) {
                    if(const auto item = m_fieldRegistry->itemById(field.id)) {
                        node->changeField(item.value());
                        node->setStatus(TagEditorFieldItem::None);
                    }
                }
                else {
                    qCWarning(TAGEDITOR_MODEL) << "Field could not be changed:" << field.name;
                }
                break;
            }
            case(TagEditorFieldItem::None):
                break;
        }
    }

    for(const auto& index : rowsToRemove) {
        std::erase_if(m_nodes, [index](const auto& node) { return node->field().index == index; });
    }

    invalidateData();
}

void TagEditorFieldsModel::invalidateData()
{
    beginResetModel();
    endResetModel();
}

Qt::ItemFlags TagEditorFieldsModel::flags(const QModelIndex& index) const
{
    if(!index.isValid()) {
        return Qt::NoItemFlags;
    }

    auto flags = ExtendableTableModel::flags(index) |= Qt::ItemIsEditable;

    if(index.column() >= 3) {
        flags |= Qt::ItemIsUserCheckable;
    }

    return flags;
}

QVariant TagEditorFieldsModel::headerData(int section, Qt::Orientation orientation, int role) const
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
            return tr("Field");
        case(3):
            return tr("Multiline");
        default:
            break;
    }

    return {};
}

QVariant TagEditorFieldsModel::data(const QModelIndex& index, int role) const
{
    if(!checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return {};
    }

    auto* item       = static_cast<TagEditorFieldItem*>(index.internalPointer());
    const int column = index.column();

    if(role == Qt::TextAlignmentRole) {
        switch(column) {
            case(0):
            case(1):
            case(2):
                return (Qt::AlignVCenter | Qt::AlignLeft).toInt();
            case(3):
            case(4):
                return Qt::AlignCenter;
            default:
                return {};
        }
    }

    if(role == Qt::FontRole) {
        return item->font();
    }

    if(role == Qt::UserRole) {
        return QVariant::fromValue(item->field());
    }

    if(role == Qt::CheckStateRole) {
        if(column == 3) {
            if(item->field().multiline) {
                return Qt::Checked;
            }
            return Qt::Unchecked;
        }
        if(column == 4) {
            if(item->field().multivalue) {
                return Qt::Checked;
            }
            return Qt::Unchecked;
        }
    }

    if(role == Qt::DisplayRole || role == Qt::EditRole) {
        switch(column) {
            case(0):
                return item->field().index;
            case(1): {
                const QString& name = item->field().name;
                return !name.isEmpty() ? name : u"<enter name here>"_s;
            }
            case(2): {
                const QString& field = item->field().scriptField;
                return !field.isEmpty() ? field : u"<enter field here>"_s;
            }
            default:
                break;
        }
    }

    return {};
}

bool TagEditorFieldsModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if(role != Qt::EditRole && role != Qt::CheckStateRole) {
        return false;
    }

    auto* item           = static_cast<TagEditorFieldItem*>(index.internalPointer());
    const int column     = index.column();
    TagEditorField field = item->field();

    if(role == Qt::CheckStateRole) {
        const bool checked = value.value<Qt::CheckState>() == Qt::Checked;
        if(column == 3) {
            field.multiline = checked;
        }
    }

    switch(index.column()) {
        case(1): {
            if(value.toString() == u"<enter name here>"_s || field.name == value.toString()) {
                if(item->status() == TagEditorFieldItem::Added) {
                    emit pendingRowCancelled();
                }
                return false;
            }
            field.name = value.toString();
            break;
        }
        case(2): {
            QString setValue = value.toString().trimmed();
            setValue.remove(u'%');

            if(field.scriptField == setValue) {
                return false;
            }

            field.scriptField = setValue;
            break;
        }
        default:
            break;
    }

    if(item->status() == TagEditorFieldItem::None) {
        item->setStatus(TagEditorFieldItem::Changed);
    }

    item->changeField(field);
    emit dataChanged(index, index.siblingAtColumn(columnCount({}) - 1),
                     {Qt::FontRole, Qt::DisplayRole, Qt::CheckStateRole});

    return true;
}

QModelIndex TagEditorFieldsModel::index(int row, int column, const QModelIndex& parent) const
{
    if(!hasIndex(row, column, parent)) {
        return {};
    }

    TagEditorFieldItem* item = m_root.child(row);

    return createIndex(row, column, item);
}

int TagEditorFieldsModel::rowCount(const QModelIndex& /*parent*/) const
{
    return m_root.childCount();
}

int TagEditorFieldsModel::columnCount(const QModelIndex& /*parent*/) const
{
    return 4;
}

bool TagEditorFieldsModel::removeRows(int row, int count, const QModelIndex& /*parent*/)
{
    for(int i{row}; i < row + count; ++i) {
        const QModelIndex& index = this->index(i, 0, {});

        if(!index.isValid()) {
            return false;
        }

        auto* item = static_cast<TagEditorFieldItem*>(index.internalPointer());
        if(item) {
            if(item->status() == TagEditorFieldItem::Added) {
                beginRemoveRows({}, i, i);
                m_root.removeChild(i);
                endRemoveRows();
                m_nodes.erase(m_nodes.begin() + i);
            }
            else if(!item->field().isDefault) {
                item->setStatus(TagEditorFieldItem::Removed);
            }
        }
    }

    invalidateData();

    return true;
}

void TagEditorFieldsModel::addPendingRow()
{
    const int index = static_cast<int>(m_nodes.size());

    TagEditorField field;
    field.index = index;

    auto* item = m_nodes.emplace_back(std::make_unique<TagEditorFieldItem>(field, &m_root)).get();

    item->setStatus(TagEditorFieldItem::Added);

    const int row = m_root.childCount();
    beginInsertRows({}, row, row);
    m_root.appendChild(item);
    endInsertRows();
}

void TagEditorFieldsModel::removePendingRow()
{
    const int row = rowCount({}) - 1;
    beginRemoveRows({}, row, row);
    m_root.removeChild(row);
    endRemoveRows();
}

void TagEditorFieldsModel::moveRowsUp(const QModelIndexList& indexes)
{
    const int row = indexes.front().row() - 1;
    if(row < 0) {
        return;
    }

    const auto children = std::views::transform(
        indexes, [](const QModelIndex& index) { return static_cast<TagEditorFieldItem*>(index.internalPointer()); });

    int currRow{row};
    const int firstRow = children.front()->row();
    const int lastRow  = static_cast<int>(firstRow + children.size()) - 1;

    beginMoveRows({}, firstRow, lastRow, {}, row);

    for(auto* childItem : children) {
        childItem->resetRow();
        const int oldRow = childItem->row();
        m_root.moveChild(oldRow, currRow);
        Utils::move(m_nodes, oldRow, currRow);
        ++currRow;
    }

    m_root.resetChildren();
    adjustIndicies();

    endMoveRows();
}

void TagEditorFieldsModel::moveRowsDown(const QModelIndexList& indexes)
{
    const int row = indexes.back().row() + 2;
    if(row > rowCount({})) {
        return;
    }

    const auto children = std::views::transform(
        indexes, [](const QModelIndex& index) { return static_cast<TagEditorFieldItem*>(index.internalPointer()); });

    const int firstRow = children.front()->row();
    const int lastRow  = static_cast<int>(firstRow + children.size()) - 1;

    beginMoveRows({}, firstRow, lastRow, {}, row);

    for(auto* childItem : children) {
        childItem->resetRow();
        const int oldRow = childItem->row();
        m_root.moveChild(oldRow, row);
        Utils::move(m_nodes, oldRow, row - 1);
    }

    m_root.resetChildren();
    adjustIndicies();

    endMoveRows();
}

bool TagEditorFieldsModel::hasField(const QString& field, int id) const
{
    return std::ranges::any_of(m_nodes, [&field, id](const auto& node) {
        const auto modelField = node->field();
        return modelField.scriptField == field && modelField.id != id;
    });
}

void TagEditorFieldsModel::adjustIndicies()
{
    for(int i{0}; const auto& item : m_nodes) {
        auto field = item->field();
        if(std::exchange(field.index, i++) != field.index) {
            item->changeField(field);
            item->setStatus(TagEditorFieldItem::Changed);
        }
    }
}
} // namespace Fooyin::TagEditor
