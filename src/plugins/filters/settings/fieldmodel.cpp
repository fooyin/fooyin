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

#include "fieldmodel.h"

#include "fieldregistry.h"

#include <QFont>

using namespace Qt::Literals::StringLiterals;

namespace Fooyin::Filters {
class FieldItem : public TreeStatusItem<FieldItem>
{
public:
    FieldItem()
        : FieldItem{{}, nullptr}
    { }

    explicit FieldItem(FilterField field, FieldItem* parent)
        : TreeStatusItem{parent}
        , m_field{std::move(field)}
    { }

    [[nodiscard]] FilterField field() const
    {
        return m_field;
    }

    void changeField(const FilterField& field)
    {
        m_field = field;
    }

private:
    FilterField m_field;
};

struct FieldModel::Private
{
    FieldRegistry* fieldsRegistry;
    FieldItem root;
    std::map<int, FieldItem> nodes;

    explicit Private(FieldRegistry* fieldsRegistry)
        : fieldsRegistry{fieldsRegistry}
    { }
};

FieldModel::FieldModel(FieldRegistry* fieldsRegistry, QObject* parent)
    : ExtendableTableModel{parent}
    , p{std::make_unique<Private>(fieldsRegistry)}
{ }

FieldModel::~FieldModel() = default;

void FieldModel::populate()
{
    beginResetModel();
    p->root = {};
    p->nodes.clear();

    const auto& fields = p->fieldsRegistry->items();

    for(const auto& [index, field] : fields) {
        if(field.name.isEmpty()) {
            continue;
        }
        FieldItem* child = &p->nodes.emplace(index, FieldItem{field, &p->root}).first->second;
        p->root.insertChild(index, child);
    }

    endResetModel();
}

void FieldModel::processQueue()
{
    std::vector<int> fieldsToRemove;

    for(auto& [index, node] : p->nodes) {
        const FieldItem::ItemStatus status = node.status();
        const FilterField field            = node.field();

        switch(status) {
            case(FieldItem::Added): {
                if(field.field.isEmpty()) {
                    break;
                }

                const FilterField addedField = p->fieldsRegistry->addItem(field);
                if(addedField.isValid()) {
                    node.changeField(addedField);
                    node.setStatus(FieldItem::None);

                    emit dataChanged({}, {});
                }
                else {
                    qWarning() << "Field " + field.name + " could not be added";
                }
                break;
            }
            case(FieldItem::Removed): {
                if(p->fieldsRegistry->removeByIndex(field.index)) {
                    beginRemoveRows({}, node.row(), node.row());
                    p->root.removeChild(node.row());
                    endRemoveRows();
                    fieldsToRemove.push_back(index);
                }
                else {
                    qWarning() << "Field " + field.name + " could not be removed";
                }
                break;
            }
            case(FieldItem::Changed): {
                if(p->fieldsRegistry->changeItem(field)) {
                    node.changeField(p->fieldsRegistry->itemById(field.id));
                    node.setStatus(FieldItem::None);

                    emit dataChanged({}, {});
                }
                else {
                    qWarning() << "Field " + field.name + " could not be changed";
                }
                break;
            }
            case(FieldItem::None):
                break;
        }
    }
    for(const auto& index : fieldsToRemove) {
        p->nodes.erase(index);
    }
}

Qt::ItemFlags FieldModel::flags(const QModelIndex& index) const
{
    if(!index.isValid()) {
        return Qt::NoItemFlags;
    }

    auto flags = ExtendableTableModel::flags(index);
    flags |= Qt::ItemIsEditable;

    return flags;
}

QVariant FieldModel::headerData(int section, Qt::Orientation orientation, int role) const
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
            return "Field";
        case(3):
            return "Sort Field";
    }
    return {};
}

QVariant FieldModel::data(const QModelIndex& index, int role) const
{
    if(role != Qt::DisplayRole && role != Qt::EditRole && role != Qt::FontRole) {
        return {};
    }

    if(!checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return {};
    }

    auto* item = static_cast<FieldItem*>(index.internalPointer());

    if(role == Qt::FontRole) {
        return item->font();
    }

    switch(index.column()) {
        case(0):
            return item->field().index;
        case(1): {
            const QString& name = item->field().name;
            return !name.isEmpty() ? name : QStringLiteral("<enter name here>");
        }
        case(2): {
            const QString& field = item->field().field;
            return !field.isEmpty() ? field : QStringLiteral("<enter field here>");
        }
        case(3): {
            const QString& sortField = item->field().sortField;
            return !sortField.isEmpty() || item->field().isValid() ? sortField
                                                                   : QStringLiteral("<enter sort field here>");
        }
    }
    return {};
}

bool FieldModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if(role != Qt::EditRole) {
        return false;
    }

    auto* item        = static_cast<FieldItem*>(index.internalPointer());
    FilterField field = item->field();

    switch(index.column()) {
        case(1): {
            if(value.toString() == "<enter name here>"_L1 || field.name == value.toString()) {
                if(item->status() == FieldItem::Added) {
                    emit pendingRowCancelled();
                }
                return false;
            }
            field.name = value.toString();

            emit pendingRowAdded();
            break;
        }
        case(2): {
            if(field.field == value.toString()) {
                return false;
            }
            field.field = value.toString();
            break;
        }
        case(3): {
            if(field.sortField == value.toString()) {
                return false;
            }
            field.sortField = value.toString();
            break;
        }
        case(0):
            break;
    }

    if(item->status() == FieldItem::None) {
        item->setStatus(FieldItem::Changed);
    }

    item->changeField(field);
    emit dataChanged(index, index, {Qt::FontRole, Qt::DisplayRole});

    return true;
}

QModelIndex FieldModel::index(int row, int column, const QModelIndex& parent) const
{
    if(!hasIndex(row, column, parent)) {
        return {};
    }

    FieldItem* item = p->root.child(row);

    return createIndex(row, column, item);
}

int FieldModel::rowCount(const QModelIndex& /*parent*/) const
{
    return p->root.childCount();
}

int FieldModel::columnCount(const QModelIndex& /*parent*/) const
{
    return 4;
}

bool FieldModel::removeRows(int row, int count, const QModelIndex& /*parent*/)
{
    for(int i{row}; i < row + count; ++i) {
        const QModelIndex& index = this->index(i, 0, {});

        if(!index.isValid()) {
            return false;
        }

        auto* item = static_cast<FieldItem*>(index.internalPointer());
        if(item) {
            if(item->status() == FieldItem::Added) {
                beginRemoveRows({}, i, i);
                p->root.removeChild(i);
                endRemoveRows();
                p->nodes.erase(item->field().index);
            }
            else {
                item->setStatus(FieldItem::Removed);
                emit dataChanged({}, {}, {Qt::FontRole});
            }
        }
    }
    return true;
}

void FieldModel::addPendingRow()
{
    const int index = static_cast<int>(p->nodes.size());

    FilterField field;
    field.index = index;

    FieldItem* item = &p->nodes.emplace(index, FieldItem{field, &p->root}).first->second;

    item->setStatus(FieldItem::Added);

    const int row = p->root.childCount();
    beginInsertRows({}, row, row);
    p->root.appendChild(item);
    endInsertRows();

    emit newPendingRow();
}

void FieldModel::removePendingRow()
{
    const int row = rowCount({}) - 1;
    beginRemoveRows({}, row, row);
    p->root.removeChild(row);
    endRemoveRows();
}
} // namespace Fooyin::Filters
