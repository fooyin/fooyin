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

#include "presetmodel.h"

#include "gui/playlist/presetregistry.h"

namespace Fy::Gui::Settings {
using PlaylistPreset = Widgets::Playlist::PlaylistPreset;
using PresetRegistry = Widgets::Playlist::PresetRegistry;

PresetItem::PresetItem()
    : PresetItem{{}, nullptr}
{ }

PresetItem::PresetItem(PlaylistPreset preset, PresetItem* parent)
    : TreeStatusItem{parent}
    , m_preset{std::move(preset)}
{ }

Widgets::Playlist::PlaylistPreset PresetItem::preset() const
{
    return m_preset;
}

void PresetItem::changePreset(const PlaylistPreset& preset)
{
    m_preset = preset;
}

PresetModel::PresetModel(PresetRegistry* presetRegistry, QObject* parent)
    : TableModel{parent}
    , m_presetRegistry{presetRegistry}
{
    setupModelData();
}

void PresetModel::setupModelData()
{
    const auto& presets = m_presetRegistry->presets();

    for(const auto& [index, preset] : presets) {
        if(preset.name.isEmpty()) {
            continue;
        }
        PresetItem* child = m_nodes.emplace_back(std::make_unique<PresetItem>(preset, rootItem())).get();
        rootItem()->appendChild(child);
    }
}

void PresetModel::addNewPreset(const PlaylistPreset& preset)
{
    const int index = static_cast<int>(m_nodes.size());

    PlaylistPreset newPreset{preset};
    newPreset.index = index;

    auto* parent = rootItem();

    auto* item = m_nodes.emplace_back(std::make_unique<PresetItem>(newPreset, parent)).get();

    item->setStatus(PresetItem::Added);

    const int row = parent->childCount();
    beginInsertRows({}, row, row);
    parent->appendChild(item);
    endInsertRows();
}

void PresetModel::markForRemoval(const PlaylistPreset& preset)
{
    PresetItem* item = m_nodes.at(preset.index).get();

    if(item->status() == PresetItem::Added) {
        beginRemoveRows({}, item->row(), item->row());
        rootItem()->removeChild(item->row());
        endRemoveRows();

        removePreset(preset.index);
    }
    else {
        item->setStatus(PresetItem::Removed);
        const QModelIndex index = createIndex(item->row(), 1, item);
        emit dataChanged(index, index, {Qt::FontRole});
    }
}

void PresetModel::markForChange(const PlaylistPreset& preset)
{
    PresetItem* item = m_nodes.at(preset.index).get();
    item->changePreset(preset);
    const QModelIndex index = createIndex(item->row(), 1, item);
    emit dataChanged(index, index, {Qt::DisplayRole});

    if(item->status() == PresetItem::None) {
        item->setStatus(PresetItem::Changed);
    }
}

void PresetModel::processQueue()
{
    std::vector<PresetItem*> presetsToRemove;

    for(auto& node : m_nodes) {
        PresetItem* item                    = node.get();
        const PresetItem::ItemStatus status = item->status();
        const PlaylistPreset preset         = item->preset();

        switch(status) {
            case(PresetItem::Added): {
                const PlaylistPreset addedPreset = m_presetRegistry->addPreset(preset);
                if(addedPreset.isValid()) {
                    item->changePreset(addedPreset);
                    item->setStatus(PresetItem::None);
                }
                else {
                    qWarning() << QString{"Field %1 could not be added"}.arg(preset.name);
                }
                break;
            }
            case(PresetItem::Removed): {
                if(m_presetRegistry->removeByIndex(preset.index)) {
                    beginRemoveRows({}, item->row(), item->row());
                    rootItem()->removeChild(item->row());
                    endRemoveRows();
                    presetsToRemove.push_back(item);
                }
                else {
                    qWarning() << QString{"Field (%1) could not be removed"}.arg(preset.name);
                }
                break;
            }
            case(PresetItem::Changed): {
                if(m_presetRegistry->changePreset(preset)) {
                    item->setStatus(PresetItem::None);
                }
                else {
                    qWarning() << QString{"Field (%1) could not be changed"}.arg(preset.name);
                }
                break;
            }
            case(PresetItem::None):
                break;
        }
    }
    for(const auto& item : presetsToRemove) {
        removePreset(item->preset().index);
    }
}

Qt::ItemFlags PresetModel::flags(const QModelIndex& index) const
{
    if(!index.isValid()) {
        return Qt::NoItemFlags;
    }

    auto flags = TableModel::flags(index);
    flags |= Qt::ItemIsEditable;

    return flags;
}

QVariant PresetModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(role == Qt::TextAlignmentRole) {
        return (Qt::AlignHCenter);
    }

    if(role != Qt::DisplayRole || orientation == Qt::Orientation::Vertical) {
        return {};
    }

    switch(section) {
        case(0):
            return "Preset Index";
        case(1):
            return "Preset Name";
        default:
            return {};
    }
}

QVariant PresetModel::data(const QModelIndex& index, int role) const
{
    if(role != Qt::DisplayRole && role != Qt::EditRole && role != Qt::FontRole) {
        return {};
    }

    if(!checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return {};
    }

    auto* item = static_cast<PresetItem*>(index.internalPointer());

    if(role == Qt::FontRole) {
        QFont font;
        switch(item->status()) {
            case(PresetItem::Added):
                font.setItalic(true);
                break;
            case(PresetItem::Removed):
                font.setStrikeOut(true);
                break;
            case(PresetItem::Changed):
                font.setBold(true);
                break;
            case(PresetItem::None):
                break;
        }
        return font;
    }

    switch(index.column()) {
        case(0):
            return item->preset().index;
        case(1): {
            const QString& name = item->preset().name;
            return !name.isEmpty() ? name : "<enter name here>";
        }
    }
    return {};
}

bool PresetModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if(role != Qt::EditRole) {
        return false;
    }

    auto* item            = static_cast<PresetItem*>(index.internalPointer());
    const int col         = index.column();
    PlaylistPreset preset = item->preset();

    switch(col) {
        case(1): {
            if(preset.name == value.toString()) {
                return false;
            }
            preset.name = value.toString();
            break;
        }
        case(0):
        default:
            break;
    }

    markForChange(preset);

    return true;
}

int PresetModel::columnCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent)
    return 2;
}

void PresetModel::removePreset(int index)
{
    if(index < 0 || index >= static_cast<int>(m_nodes.size())) {
        return;
    }
    m_nodes.erase(m_nodes.begin() + index);
}
} // namespace Fy::Gui::Settings
