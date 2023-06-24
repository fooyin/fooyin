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

#include "pluginsmodel.h"

#include "pluginitem.h"

#include <core/plugins/pluginmanager.h>

#include <utils/enumhelper.h>

namespace Fy::Gui::Settings {
Fy::Gui::Settings::PluginsModel::PluginsModel(Plugins::PluginManager* pluginManager, QObject* parent)
    : TableModel{parent}
    , m_pluginManager{pluginManager}
{
    setupModelData();
}

void PluginsModel::setupModelData()
{
    const auto& plugins = m_pluginManager->allPluginInfo();

    for(const auto& [name, info] : plugins) {
        PluginItem* parent = rootItem();

        if(!m_nodes.contains(name)) {
            m_nodes.emplace(name, std::make_unique<PluginItem>(info.get(), parent));
        }
        PluginItem* child = m_nodes.at(name).get();
        parent->appendChild(child);
    }
}

int PluginsModel::rowCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent)
    return rootItem()->childCount();
}

int PluginsModel::columnCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent)
    return 5;
}

QVariant PluginsModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(role == Qt::TextAlignmentRole) {
        return (Qt::AlignHCenter);
    }

    if(role != Qt::DisplayRole || orientation == Qt::Orientation::Vertical) {
        return {};
    }

    switch(section) {
        case(0):
            return "Name";
        case(1):
            return "Version";
        case(2):
            return "Category";
        case(3):
            return "Author";
        case(4):
            return "Status";
    }
    return {};
}

QVariant PluginsModel::data(const QModelIndex& index, int role) const
{
    if(role != Qt::DisplayRole && role != Qt::ToolTipRole) {
        return {};
    }

    if(!index.isValid()) {
        return {};
    }

    const int column = index.column();
    auto* item       = static_cast<PluginItem*>(index.internalPointer());

    if(role == Qt::DisplayRole) {
        switch(column) {
            case(0):
                return item->info()->name();
            case(1):
                return item->info()->version();
            case(2):
                return item->info()->category();
            case(3):
                return item->info()->vendor();
            case(4):
                return Utils::EnumHelper::toString(item->info()->status());
        }
    }

    if(role == Qt::ToolTipRole) {
        return item->info()->description();
    }

    return {};
}
} // namespace Fy::Gui::Settings
