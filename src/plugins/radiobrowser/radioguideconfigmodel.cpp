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

#include "radioguideconfigmodel.h"

#include <QDataStream>
#include <QIODevice>
#include <QMimeData>

#include <unordered_set>

using namespace Qt::StringLiterals;

constexpr auto RadioGuideConfigMime = "application/x-fooyin-radioguide-config-item"_L1;

namespace {
QByteArray dragDataForIndex(const QModelIndex& index)
{
    QByteArray data;
    QDataStream stream{&data, QIODevice::WriteOnly};
    stream.setVersion(QDataStream::Qt_6_0);

    stream << (index.parent().isValid() ? index.parent().row() : -1);
    stream << index.row();

    return data;
}
} // namespace

namespace Fooyin::RadioBrowser {
QVariant RadioGuideConfigModel::data(const QModelIndex& index, int role) const
{
    if(!index.isValid()) {
        return {};
    }

    const auto* item = itemForIndex(index);
    if(!item) {
        return {};
    }

    switch(role) {
        case ItemKindRole:
            return static_cast<int>(item->kind);
        case Qt::DisplayRole:
        case Qt::EditRole:
            if(index.column() == 0) {
                return item->name;
            }
            if(item->kind == RadioGuideConfigItemKind::Preset && index.column() == 1) {
                return item->tag;
            }
            break;
        default:
            break;
    }

    return {};
}

QVariant RadioGuideConfigModel::headerData(int section, Qt::Orientation orientation, int role) const
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

    return section == 0 ? tr("Name") : tr("Tag");
}

Qt::ItemFlags RadioGuideConfigModel::flags(const QModelIndex& index) const
{
    auto defaultFlags = TreeModel::flags(index);
    defaultFlags |= Qt::ItemIsDragEnabled;

    if(!index.isValid()) {
        defaultFlags |= Qt::ItemIsDropEnabled;
    }

    const auto* item = itemForIndex(index);

    if(item && item->kind == RadioGuideConfigItemKind::Section) {
        if(index.column() == 0) {
            defaultFlags |= Qt::ItemIsEditable;
        }
        defaultFlags |= Qt::ItemIsDropEnabled;
    }
    else if(item && item->kind == RadioGuideConfigItemKind::Preset) {
        defaultFlags |= Qt::ItemIsEditable;
    }

    return defaultFlags;
}

int RadioGuideConfigModel::columnCount(const QModelIndex& /*parent*/) const
{
    return 2;
}

bool RadioGuideConfigModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if(!index.isValid() || role != Qt::EditRole) {
        return false;
    }

    auto* item = itemForIndex(index);
    if(!item) {
        return false;
    }

    const QString text = value.toString().trimmed();
    if(index.column() == 0) {
        item->name = text;
    }
    else if(index.column() == 1 && item->kind == RadioGuideConfigItemKind::Preset) {
        item->tag = text;
    }
    else {
        return false;
    }

    Q_EMIT dataChanged(index, index, {Qt::DisplayRole, Qt::EditRole});
    return true;
}

QStringList RadioGuideConfigModel::mimeTypes() const
{
    return {RadioGuideConfigMime};
}

QMimeData* RadioGuideConfigModel::mimeData(const QModelIndexList& indexes) const
{
    for(const QModelIndex& index : indexes) {
        if(index.isValid()) {
            auto* mimeData = new QMimeData();
            mimeData->setData(RadioGuideConfigMime, dragDataForIndex(index.siblingAtColumn(0)));
            return mimeData;
        }
    }
    return nullptr;
}

bool RadioGuideConfigModel::canDropMimeData(const QMimeData* data, const Qt::DropAction action, int /*row*/, int column,
                                            const QModelIndex& parent) const
{
    if(action != Qt::MoveAction || column >= columnCount({})) {
        return false;
    }

    const auto dragItem = dragItemFromData(data);
    if(!dragItem) {
        return false;
    }

    const QModelIndex source = indexForDragItem(*dragItem);
    if(!source.isValid()) {
        return false;
    }

    const auto* item = itemForIndex(source);
    if(!item) {
        return false;
    }

    if(item->kind == RadioGuideConfigItemKind::Section) {
        return !parent.isValid();
    }

    const auto* parentItem = itemForIndex(parent);
    return parent.isValid() && parentItem && parentItem->kind == RadioGuideConfigItemKind::Section;
}

bool RadioGuideConfigModel::dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column,
                                         const QModelIndex& parent)
{
    if(!canDropMimeData(data, action, row, column, parent)) {
        return false;
    }

    const auto dragItem = dragItemFromData(data);
    if(!dragItem) {
        return false;
    }

    const QModelIndex source = indexForDragItem(*dragItem);
    if(!source.isValid()) {
        return false;
    }

    const QModelIndex sourceParent = source.parent();
    auto* sourceParentItem         = itemForIndex(sourceParent);
    auto* targetParentItem         = itemForIndex(parent);
    if(!sourceParentItem || !targetParentItem) {
        return false;
    }

    if(row < 0) {
        row = targetParentItem->childCount();
    }

    if(sourceParent == parent) {
        if(row == source.row() || row == source.row() + 1) {
            return false;
        }

        if(!beginMoveRows(sourceParent, source.row(), source.row(), parent, row)) {
            return false;
        }
        sourceParentItem->moveChild(source.row(), row);
        endMoveRows();

        return true;
    }

    row = std::min(row, targetParentItem->childCount());

    auto* item = itemForIndex(source);
    beginMoveRows(sourceParent, source.row(), source.row(), parent, row);
    sourceParentItem->removeChild(source.row());
    targetParentItem->insertChild(row, item);
    endMoveRows();

    return true;
}

Qt::DropActions RadioGuideConfigModel::supportedDragActions() const
{
    return Qt::MoveAction;
}

Qt::DropActions RadioGuideConfigModel::supportedDropActions() const
{
    return Qt::MoveAction;
}

void RadioGuideConfigModel::setSections(const RadioGuideTagSections& sections)
{
    beginResetModel();

    resetRoot();
    m_nodes.clear();

    for(const RadioGuideTagSection& section : sections) {
        const QString sectionName = section.name.trimmed();
        if(sectionName.isEmpty()) {
            continue;
        }

        auto* sectionItem = createNode(rootItem());
        sectionItem->kind = RadioGuideConfigItemKind::Section;
        sectionItem->name = sectionName;

        for(const RadioGuideTag& tag : section.tags) {
            const QString name  = tag.name.trimmed();
            const QString value = tag.tag.trimmed();
            if(name.isEmpty() || value.isEmpty()) {
                continue;
            }

            auto* preset = createNode(sectionItem);
            preset->kind = RadioGuideConfigItemKind::Preset;
            preset->name = name;
            preset->tag  = value;
        }
    }

    endResetModel();
}

RadioGuideTagSections RadioGuideConfigModel::sections() const
{
    RadioGuideTagSections result;

    const auto sections = rootItem()->children();
    for(const auto* section : sections) {
        RadioGuideTagSection entry{.name = section->name, .tags = {}};
        if(entry.name.isEmpty()) {
            continue;
        }

        const auto presets = section->children();
        for(const auto* preset : presets) {
            if(!preset->name.isEmpty() && !preset->tag.isEmpty()) {
                entry.tags.emplace_back(preset->name, preset->tag);
            }
        }

        result.emplace_back(std::move(entry));
    }

    return result;
}

QModelIndex RadioGuideConfigModel::addSection(const QString& name, int row)
{
    if(row < 0 || row > rootItem()->childCount()) {
        row = rootItem()->childCount();
    }

    beginInsertRows({}, row, row);

    auto* section = createNode();
    section->kind = RadioGuideConfigItemKind::Section;
    section->name = name.trimmed();
    rootItem()->insertChild(row, section);

    endInsertRows();

    return indexOfItem(section);
}

QModelIndex RadioGuideConfigModel::addPreset(const QModelIndex& sectionIndex, const RadioGuideTag& tag, int row)
{
    RadioGuideConfigItem* section = sectionForIndex(sectionIndex);
    if(!section) {
        return {};
    }

    const QModelIndex parentIndex = indexOfItem(section);
    if(row < 0 || row > section->childCount()) {
        row = section->childCount();
    }

    beginInsertRows(parentIndex, row, row);

    auto* preset = createNode();
    preset->kind = RadioGuideConfigItemKind::Preset;
    preset->name = tag.name.trimmed();
    preset->tag  = tag.tag.trimmed();
    section->insertChild(row, preset);

    endInsertRows();

    return indexOfItem(preset);
}

bool RadioGuideConfigModel::removeIndex(const QModelIndex& index)
{
    if(!index.isValid()) {
        return false;
    }

    auto* item   = itemForIndex(index);
    auto* parent = item ? item->parent() : nullptr;
    if(!item || !parent) {
        return false;
    }

    beginRemoveRows(index.parent(), index.row(), index.row());

    std::vector<RadioGuideConfigItem*> nodesToRemove;
    nodesToRemove.emplace_back(item);
    collectDescendants(item, nodesToRemove);
    const std::unordered_set<RadioGuideConfigItem*> nodeSet{nodesToRemove.cbegin(), nodesToRemove.cend()};

    parent->removeChild(index.row());
    std::erase_if(m_nodes, [&nodeSet](const auto& node) { return nodeSet.contains(node.get()); });

    endRemoveRows();

    return true;
}

QModelIndex RadioGuideConfigModel::moveIndex(const QModelIndex& index, int offset)
{
    if(!index.isValid()) {
        return {};
    }

    const int targetRow = index.row() + offset;
    if(targetRow < 0 || targetRow >= rowCount(index.parent())) {
        return {};
    }

    auto* parent = itemForIndex(index.parent());
    if(!parent) {
        return {};
    }

    const int dest = offset > 0 ? targetRow + 1 : targetRow;
    if(!beginMoveRows(index.parent(), index.row(), index.row(), index.parent(), dest)) {
        return {};
    }
    parent->moveChild(index.row(), dest);
    endMoveRows();

    return this->index(targetRow, index.column(), index.parent());
}

RadioGuideConfigItem* RadioGuideConfigModel::createNode(RadioGuideConfigItem* parent)
{
    auto* item = m_nodes.emplace_back(std::make_unique<RadioGuideConfigItem>()).get();
    if(parent) {
        parent->appendChild(item);
    }
    return item;
}

void RadioGuideConfigModel::clearChildren(RadioGuideConfigItem* parent)
{
    std::vector<RadioGuideConfigItem*> descendants;
    collectDescendants(parent, descendants);
    const std::unordered_set<RadioGuideConfigItem*> descendantSet{descendants.cbegin(), descendants.cend()};

    parent->clearChildren();

    std::erase_if(m_nodes, [&descendantSet](const auto& node) { return descendantSet.contains(node.get()); });
}

void RadioGuideConfigModel::collectDescendants(RadioGuideConfigItem* parent,
                                               std::vector<RadioGuideConfigItem*>& descendants) const
{
    if(!parent) {
        return;
    }

    for(RadioGuideConfigItem* child : parent->children()) {
        descendants.push_back(child);
        collectDescendants(child, descendants);
    }
}

RadioGuideConfigItem* RadioGuideConfigModel::sectionForIndex(const QModelIndex& index) const
{
    if(!index.isValid()) {
        return nullptr;
    }

    auto* item = itemForIndex(index);
    if(!item) {
        return nullptr;
    }

    if(item->kind == RadioGuideConfigItemKind::Section) {
        return item;
    }

    auto* parent = item->parent();
    return parent && parent != rootItem() ? parent : nullptr;
}

QModelIndex RadioGuideConfigModel::indexForDragItem(const DragItem& item) const
{
    if(item.parentRow < 0) {
        return index(item.row, 0, {});
    }

    const QModelIndex parent = index(item.parentRow, 0, {});
    return index(item.row, 0, parent);
}

std::optional<RadioGuideConfigModel::DragItem> RadioGuideConfigModel::dragItemFromData(const QMimeData* data)
{
    if(!data || !data->hasFormat(RadioGuideConfigMime)) {
        return {};
    }

    QByteArray mime = data->data(RadioGuideConfigMime);
    QDataStream stream{&mime, QIODevice::ReadOnly};
    stream.setVersion(QDataStream::Qt_6_0);

    DragItem item;
    stream >> item.parentRow >> item.row;
    return item;
}
} // namespace Fooyin::RadioBrowser
