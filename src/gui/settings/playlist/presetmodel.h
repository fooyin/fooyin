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

#pragma once

#include "gui/playlist/playlistpreset.h"

#include <utils/tablemodel.h>
#include <utils/treestatusitem.h>

namespace Fy::Gui {
namespace Widgets::Playlist {
class PresetRegistry;
}
namespace Settings {
class PresetItem : public Utils::TreeStatusItem<PresetItem>
{
public:
    PresetItem();
    explicit PresetItem(Widgets::Playlist::PlaylistPreset preset , PresetItem* parent);

    [[nodiscard]] Widgets::Playlist::PlaylistPreset preset() const;
    void changePreset(const Widgets::Playlist::PlaylistPreset& preset);

private:
    Widgets::Playlist::PlaylistPreset m_preset;
};

class PresetModel : public Utils::TableModel<PresetItem>
{
public:
    explicit PresetModel(Widgets::Playlist::PresetRegistry* presetRegistry, QObject* parent = nullptr);

    void setupModelData();
    void addNewPreset(const Widgets::Playlist::PlaylistPreset& preset = {});
    void markForRemoval(const Widgets::Playlist::PlaylistPreset& preset);
    void markForChange(const Widgets::Playlist::PlaylistPreset& preset);
    void processQueue();

    [[nodiscard]] Qt::ItemFlags flags(const QModelIndex& index) const override;
    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role) override;
    [[nodiscard]] int columnCount(const QModelIndex& parent) const override;

private:
    void removePreset(int index);

    using PresetItemList = std::vector<std::unique_ptr<PresetItem>>;

    Widgets::Playlist::PresetRegistry* m_presetRegistry;

    PresetItemList m_nodes;
};
} // namespace Settings
} // namespace Fy::Gui
