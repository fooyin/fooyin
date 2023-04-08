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

#include "pluginitem.h"

#include <utils/tablemodel.h>

namespace Fy {
namespace Plugins {
class PluginManager;
}

namespace Gui::Settings {
class PluginsModel : public Utils::TableModel<PluginItem>
{
public:
    explicit PluginsModel(Plugins::PluginManager* pluginManager, QObject* parent = nullptr);

    void setupModelData();

    [[nodiscard]] int rowCount(const QModelIndex& parent) const override;
    [[nodiscard]] int columnCount(const QModelIndex& parent) const override;
    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;

private:
    using PluginNameMap = std::unordered_map<QString, std::unique_ptr<PluginItem>>;

    Plugins::PluginManager* m_pluginManager;

    PluginNameMap m_nodes;
};
} // namespace Gui::Settings
} // namespace Fy
