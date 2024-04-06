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

#pragma once

#include <utils/tablemodel.h>
#include <utils/treeitem.h>

namespace Fooyin {
class PluginInfo;
class PluginManager;

class PluginItem : public TreeItem<PluginItem>
{
public:
    explicit PluginItem(PluginInfo* info = nullptr, PluginItem* parent = nullptr);

    [[nodiscard]] PluginInfo* info() const;

private:
    PluginInfo* m_info;
};

class PluginsModel : public TableModel<PluginItem>
{
    Q_OBJECT

public:
    explicit PluginsModel(PluginManager* pluginManager, QObject* parent = nullptr);

    void setupModelData();

    [[nodiscard]] Qt::ItemFlags flags(const QModelIndex& index) const override;
    [[nodiscard]] int rowCount(const QModelIndex& parent) const override;
    [[nodiscard]] int columnCount(const QModelIndex& parent) const override;
    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role) override;

    [[nodiscard]] QStringList enabledPlugins() const;
    [[nodiscard]] QStringList disabledPlugins() const;

private:
    using PluginNameMap = std::unordered_map<QString, PluginItem>;

    PluginManager* m_pluginManager;

    PluginNameMap m_nodes;
    QStringList m_enabledPlugins;
    QStringList m_disabledPlugins;
};
} // namespace Fooyin
