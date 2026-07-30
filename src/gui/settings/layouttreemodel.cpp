/*
 * Fooyin
 * Copyright © 2025, Luke Taylor <luket@pm.me>
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

#include "layouttreemodel.h"

#include <gui/widgetprovider.h>

using namespace Qt::StringLiterals;

namespace Fooyin {
namespace {
bool isSplitter(const LayoutItem* item)
{
    return item && (item->key() == u"SplitterVertical"_s || item->key() == u"SplitterHorizontal"_s);
}

QJsonArray splitterLocks(const LayoutItem* item)
{
    return isSplitter(item) ? item->data().value("Locked"_L1).toArray() : QJsonArray{};
}

void setSplitterLocks(LayoutItem* item, const QJsonArray& locks)
{
    if(!isSplitter(item)) {
        return;
    }

    QJsonObject data  = item->data();
    data["Locked"_L1] = locks;
    item->setData(data);
    item->setStatus(LayoutItem::Changed);
}

QJsonObject saveItemData(const LayoutItem* item);

QJsonArray saveChildren(const LayoutItem* parent)
{
    QJsonArray children;
    for(const auto* child : parent->children()) {
        if(!child || child->status() == LayoutItem::Removed) {
            continue;
        }

        QJsonObject childObject;
        childObject[child->key()] = saveItemData(child);
        children.append(childObject);
    }

    return children;
}

QJsonObject saveItemData(const LayoutItem* item)
{
    QJsonObject data = item->data();

    if(isSplitter(item) && data.contains("Locked"_L1)) {
        const QJsonArray locks = splitterLocks(item);
        QJsonArray activeLocks;
        bool hasLockedChild{false};

        for(const LayoutItem* child : item->children()) {
            if(!child || child->status() == LayoutItem::Removed) {
                continue;
            }

            const bool locked = child->row() < locks.size() && locks.at(child->row()).toBool();
            activeLocks.append(locked);
            hasLockedChild |= locked;
        }

        if(hasLockedChild) {
            data["Locked"_L1] = activeLocks;
        }
        else {
            data.remove("Locked"_L1);
        }
    }

    if(item->childCount() > 0 || data.contains("Widgets"_L1)) {
        data["Widgets"_L1] = saveChildren(item);
    }

    return data;
}

QJsonObject saveItem(const LayoutItem* item)
{
    if(!item || item->status() == LayoutItem::Removed) {
        return {};
    }

    QJsonObject itemObject;
    itemObject[item->key()] = saveItemData(item);
    return itemObject;
}

bool isValidItemObject(const QJsonObject& item)
{
    if(item.empty()) {
        return false;
    }

    const auto it = item.constBegin();
    return it->isObject();
}

int minimumChildCount(const LayoutItem* item)
{
    return isSplitter(item) ? 2 : 1;
}
} // namespace

LayoutItem::LayoutItem()
    : LayoutItem{{}, {}, nullptr}
{ }

LayoutItem::LayoutItem(QString key, QJsonObject data, LayoutItem* parent)
    : TreeStatusItem{parent}
    , m_key{std::move(key)}
    , m_data{std::move(data)}
{ }

QString LayoutItem::name() const
{
    return m_key;
}

QString LayoutItem::key() const
{
    return m_key;
}

QJsonObject LayoutItem::data() const
{
    return m_data;
}

void LayoutItem::setData(QJsonObject data)
{
    m_data = std::move(data);
}

bool LayoutItem::isContainer() const
{
    return m_data.value("Widgets"_L1).isArray();
}

LayoutTreeModel::LayoutTreeModel(WidgetProvider* widgetProvider, QObject* parent)
    : TreeModel{parent}
    , m_widgetProvider{widgetProvider}
{ }

void LayoutTreeModel::populate(const FyLayout& layout)
{
    beginResetModel();

    resetRoot();
    m_nodes.clear();
    m_layoutName = layout.name();
    m_layoutJson = layout.json();

    if(layout.isValid()) {
        populateChildren(rootItem(), m_layoutJson.value("Widgets"_L1).toArray());
    }

    endResetModel();
}

void LayoutTreeModel::remove(const QModelIndex& index)
{
    if(!canRemove(index)) {
        return;
    }

    auto* item = itemForIndex(index);

    const auto markRemovedRecursive = [](auto&& self, LayoutItem* current) -> void {
        if(!current) {
            return;
        }

        current->setStatus(LayoutItem::Removed);

        if(current->isContainer()) {
            for(auto* child : current->children()) {
                self(self, child);
            }
        }
    };

    markRemovedRecursive(markRemovedRecursive, item);

    invalidateData();
}

void LayoutTreeModel::moveUp(const QModelIndex& index)
{
    if(!canMoveUp(index)) {
        return;
    }

    auto* item                    = itemForIndex(index);
    auto* parent                  = item->parent();
    const QModelIndex parentIndex = this->parent(index);
    const int row                 = item->row();

    if(!beginMoveRows(parentIndex, row, row, parentIndex, row - 1)) {
        return;
    }

    QJsonArray locks = splitterLocks(parent);
    if(!locks.empty()) {
        while(locks.size() < parent->childCount()) {
            locks.append(false);
        }
        const QJsonValue movedLock = locks.at(row);
        locks.removeAt(row);
        locks.insert(row - 1, movedLock);
        setSplitterLocks(parent, locks);
    }

    parent->moveChild(row, row - 1);
    item->setStatus(LayoutItem::Changed);
    endMoveRows();

    Q_EMIT dataChanged(indexOfItem(item), indexOfItem(item), {Qt::FontRole});
}

void LayoutTreeModel::moveDown(const QModelIndex& index)
{
    if(!canMoveDown(index)) {
        return;
    }

    auto* item                    = itemForIndex(index);
    auto* parent                  = item->parent();
    const QModelIndex parentIndex = this->parent(index);
    const int row                 = item->row();

    if(!beginMoveRows(parentIndex, row, row, parentIndex, row + 2)) {
        return;
    }

    QJsonArray locks = splitterLocks(parent);
    if(!locks.empty()) {
        while(locks.size() < parent->childCount()) {
            locks.append(false);
        }
        const QJsonValue movedLock = locks.at(row);
        locks.removeAt(row);
        locks.insert(row + 1, movedLock);
        setSplitterLocks(parent, locks);
    }

    parent->moveChild(row, row + 2);
    item->setStatus(LayoutItem::Changed);
    endMoveRows();

    Q_EMIT dataChanged(indexOfItem(item), indexOfItem(item), {Qt::FontRole});
}

bool LayoutTreeModel::canMoveUp(const QModelIndex& index) const
{
    if(!index.isValid() || !checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return false;
    }

    const auto* item = itemForIndex(index);
    return item && item->parent() && item->parent() != rootItem() && item->row() > 0;
}

bool LayoutTreeModel::canMoveDown(const QModelIndex& index) const
{
    if(!index.isValid() || !checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return false;
    }

    const auto* item = itemForIndex(index);
    return item && item->parent() && item->parent() != rootItem() && item->row() < item->parent()->childCount() - 1;
}

bool LayoutTreeModel::canRemove(const QModelIndex& index) const
{
    if(!index.isValid() || !checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return false;
    }

    const auto* item = itemForIndex(index);
    if(!item || !item->parent() || item->parent() == rootItem()) {
        return false;
    }

    if(item->key() == u"Dummy"_s) {
        return item->parent()->childCount() > minimumChildCount(item->parent());
    }

    return true;
}

bool LayoutTreeModel::canCut(const QModelIndex& index) const
{
    return canRemove(index) && !isDummy(index);
}

bool LayoutTreeModel::canCopy(const QModelIndex& index) const
{
    if(!index.isValid() || !checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return false;
    }

    return !isDummy(index);
}

bool LayoutTreeModel::canPasteAfter(const QModelIndex& index) const
{
    if(!index.isValid() || !checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return false;
    }

    const auto* item = itemForIndex(index);
    return item && item->parent() && item->parent() != rootItem() && item->key() != u"Dummy"_s;
}

bool LayoutTreeModel::isDummy(const QModelIndex& index) const
{
    if(!index.isValid() || !checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return false;
    }

    const auto* item = itemForIndex(index);
    return item && item->key() == u"Dummy"_s;
}

QJsonObject LayoutTreeModel::copyItem(const QModelIndex& index) const
{
    if(!index.isValid() || !checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return {};
    }

    return saveItem(itemForIndex(index));
}

void LayoutTreeModel::pasteAfter(const QModelIndex& index, const QJsonObject& item)
{
    if(!canPasteAfter(index) || !isValidItemObject(item)) {
        return;
    }

    auto* target        = itemForIndex(index);
    auto* parent        = target->parent();
    const int pastedRow = target->row() + 1;

    QJsonArray locks = splitterLocks(parent);
    if(!locks.empty()) {
        while(locks.size() < parent->childCount()) {
            locks.append(false);
        }
        locks.insert(pastedRow, false);
        setSplitterLocks(parent, locks);
    }

    const auto it = item.constBegin();
    auto* pasted  = insertItem(parent, pastedRow, it.key(), it->toObject());
    populateChildren(pasted, pasted->data().value("Widgets"_L1).toArray(), LayoutItem::Added);
    pasted->setStatus(LayoutItem::Added);

    invalidateData();
}

FyLayout LayoutTreeModel::layout() const
{
    if(m_layoutName.isEmpty()) {
        return {};
    }

    QJsonObject json   = m_layoutJson;
    json["Name"_L1]    = m_layoutName;
    json["Widgets"_L1] = saveChildren(rootItem());

    return FyLayout{m_layoutName, json};
}

bool LayoutTreeModel::hasConfigurableMargins(const QModelIndex& index) const
{
    if(!index.isValid() || !checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return false;
    }

    auto* item = itemForIndex(index);
    return item && m_widgetProvider && m_widgetProvider->widgetExists(item->key());
}

bool LayoutTreeModel::hasCustomMargins(const QModelIndex& index) const
{
    if(!index.isValid() || !checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return false;
    }

    auto* item = itemForIndex(index);
    return item && item->data().value("Margins"_L1).isObject();
}

QMargins LayoutTreeModel::margins(const QModelIndex& index) const
{
    if(!index.isValid() || !checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return {};
    }

    auto* item = itemForIndex(index);
    if(!item) {
        return {};
    }

    const QJsonObject marginData = item->data().value("Margins"_L1).toObject();
    return {marginData.value("Left"_L1).toInt(), marginData.value("Top"_L1).toInt(),
            marginData.value("Right"_L1).toInt(), marginData.value("Bottom"_L1).toInt()};
}

void LayoutTreeModel::setMargins(const QModelIndex& index, const QMargins& margins)
{
    if(!index.isValid() || !checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return;
    }

    auto* item = itemForIndex(index);
    if(!item || !hasConfigurableMargins(index)) {
        return;
    }

    QJsonObject marginData;
    marginData["Left"_L1]   = margins.left();
    marginData["Top"_L1]    = margins.top();
    marginData["Right"_L1]  = margins.right();
    marginData["Bottom"_L1] = margins.bottom();

    QJsonObject data   = item->data();
    data["Margins"_L1] = marginData;
    item->setData(data);
    item->setStatus(LayoutItem::Changed);
    Q_EMIT dataChanged(index, index, {Qt::FontRole});
}

void LayoutTreeModel::clearMargins(const QModelIndex& index)
{
    if(!index.isValid() || !checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return;
    }

    auto* item = itemForIndex(index);
    if(!item || !hasConfigurableMargins(index)) {
        return;
    }

    QJsonObject data = item->data();
    data.remove("Margins"_L1);
    item->setData(data);
    item->setStatus(LayoutItem::Changed);
    Q_EMIT dataChanged(index, index, {Qt::FontRole});
}

bool LayoutTreeModel::hasConfigurableSplitterSpacing(const QModelIndex& index) const
{
    if(!index.isValid() || !checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return false;
    }

    return isSplitter(itemForIndex(index));
}

bool LayoutTreeModel::hasCustomSplitterSpacing(const QModelIndex& index) const
{
    if(!index.isValid() || !checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return false;
    }

    auto* item = itemForIndex(index);
    return item && item->data().contains("Spacing"_L1);
}

int LayoutTreeModel::splitterSpacing(const QModelIndex& index) const
{
    if(!index.isValid() || !checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return 0;
    }

    auto* item = itemForIndex(index);
    if(!item) {
        return 0;
    }

    return item->data().value("Spacing"_L1).toInt();
}

void LayoutTreeModel::setSplitterSpacing(const QModelIndex& index, int spacing)
{
    if(!index.isValid() || !checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return;
    }

    auto* item = itemForIndex(index);
    if(!item || !hasConfigurableSplitterSpacing(index)) {
        return;
    }

    QJsonObject data   = item->data();
    data["Spacing"_L1] = spacing;
    item->setData(data);
    item->setStatus(LayoutItem::Changed);
    Q_EMIT dataChanged(index, index, {Qt::FontRole});
}

void LayoutTreeModel::clearSplitterSpacing(const QModelIndex& index)
{
    if(!index.isValid() || !checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return;
    }

    auto* item = itemForIndex(index);
    if(!item || !hasConfigurableSplitterSpacing(index)) {
        return;
    }

    QJsonObject data = item->data();
    data.remove("Spacing"_L1);
    item->setData(data);
    item->setStatus(LayoutItem::Changed);
    Q_EMIT dataChanged(index, index, {Qt::FontRole});
}

bool LayoutTreeModel::hasConfigurableSplitterLock(const QModelIndex& index) const
{
    if(!index.isValid() || !checkIndex(index, CheckIndexOption::IndexIsValid) || isDummy(index)) {
        return false;
    }

    const auto* item = itemForIndex(index);
    return item && isSplitter(item->parent());
}

Qt::Orientation LayoutTreeModel::splitterLockOrientation(const QModelIndex& index) const
{
    if(!hasConfigurableSplitterLock(index)) {
        return Qt::Horizontal;
    }

    return itemForIndex(index)->parent()->key() == u"SplitterVertical"_s ? Qt::Vertical : Qt::Horizontal;
}

bool LayoutTreeModel::isSplitterItemLocked(const QModelIndex& index) const
{
    if(!hasConfigurableSplitterLock(index)) {
        return false;
    }

    const auto* item        = itemForIndex(index);
    const QJsonArray locked = splitterLocks(item->parent());
    const int splitterIndex = item->row();
    return splitterIndex < locked.size() && locked.at(splitterIndex).toBool();
}

bool LayoutTreeModel::canLockSplitterItem(const QModelIndex& index) const
{
    if(!hasConfigurableSplitterLock(index)) {
        return false;
    }
    if(isSplitterItemLocked(index)) {
        return true;
    }

    const auto* item        = itemForIndex(index);
    const auto* parent      = item->parent();
    const QJsonArray locked = splitterLocks(parent);

    for(const LayoutItem* sibling : parent->children()) {
        if(sibling == item || sibling->status() == LayoutItem::Removed) {
            continue;
        }
        if(sibling->row() >= locked.size() || !locked.at(sibling->row()).toBool()) {
            return true;
        }
    }
    return false;
}

bool LayoutTreeModel::setSplitterItemLocked(const QModelIndex& index, bool locked)
{
    if(!hasConfigurableSplitterLock(index) || (locked && !canLockSplitterItem(index))) {
        return false;
    }

    auto* item       = itemForIndex(index);
    auto* parent     = item->parent();
    QJsonArray locks = splitterLocks(parent);
    while(locks.size() < parent->childCount()) {
        locks.append(false);
    }

    locks[item->row()] = locked;

    QJsonObject data  = parent->data();
    data["Locked"_L1] = locks;
    parent->setData(data);
    parent->setStatus(LayoutItem::Changed);

    const QModelIndex parentIndex = indexOfItem(parent);
    Q_EMIT dataChanged(parentIndex, parentIndex, {Qt::FontRole});
    return true;
}

QVariant LayoutTreeModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(role == Qt::TextAlignmentRole) {
        return Qt::AlignHCenter;
    }

    if(role != Qt::DisplayRole || orientation == Qt::Orientation::Vertical) {
        return {};
    }

    switch(section) {
        case 0:
            return tr("Name");
        default:
            break;
    }

    return {};
}

QVariant LayoutTreeModel::data(const QModelIndex& index, int role) const
{
    if(role != Qt::DisplayRole && role != Qt::FontRole && role != LayoutItem::WidgetKey
       && role != LayoutItem::IsContainer) {
        return {};
    }

    if(!checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return {};
    }

    auto* item = itemForIndex(index);

    if(role == Qt::FontRole) {
        return item->font();
    }

    if(role == LayoutItem::WidgetKey) {
        return item->key();
    }

    if(role == LayoutItem::IsContainer) {
        return item->isContainer();
    }

    switch(index.column()) {
        case 0:
            return m_widgetProvider->displayName(item->key());
        default:
            break;
    }

    return {};
}

int LayoutTreeModel::columnCount(const QModelIndex& /*parent*/) const
{
    return 1;
}

LayoutItem* LayoutTreeModel::appendItem(LayoutItem* parent, const QString& key, const QJsonObject& data)
{
    auto node  = std::make_unique<LayoutItem>(key, data, parent);
    auto* item = m_nodes.emplace_back(std::move(node)).get();
    parent->appendChild(item);
    return item;
}

LayoutItem* LayoutTreeModel::insertItem(LayoutItem* parent, int row, const QString& key, const QJsonObject& data)
{
    auto node  = std::make_unique<LayoutItem>(key, data, parent);
    auto* item = m_nodes.emplace_back(std::move(node)).get();
    parent->insertChild(row, item);
    return item;
}

void LayoutTreeModel::populateChildren(LayoutItem* parent, const QJsonArray& children)
{
    populateChildren(parent, children, LayoutItem::None);
}

void LayoutTreeModel::populateChildren(LayoutItem* parent, const QJsonArray& children, LayoutItem::ItemStatus status)
{
    for(const auto& child : children) {
        if(!child.isObject()) {
            continue;
        }

        const QJsonObject childObject = child.toObject();
        if(childObject.empty()) {
            continue;
        }

        const auto it = childObject.constBegin();
        if(!it->isObject()) {
            continue;
        }

        const QString key      = it.key();
        const QJsonObject data = it->toObject();
        auto* item             = appendItem(parent, key, data);
        item->setStatus(status);

        populateChildren(item, data.value("Widgets"_L1).toArray(), status);
    }
}
} // namespace Fooyin
