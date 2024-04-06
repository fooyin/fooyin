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

#include "pluginsmodel.h"

#include "core/plugins/pluginmanager.h"

#include <utils/enum.h>

namespace Fooyin {
PluginItem::PluginItem(PluginInfo* info, PluginItem* parent)
    : TreeItem{parent}
    , m_info{info}
{ }

PluginInfo* PluginItem::info() const
{
    return m_info;
}

PluginsModel::PluginsModel(PluginManager* pluginManager, QObject* parent)
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
            m_nodes.emplace(name, PluginItem{info.get(), parent});
        }
        PluginItem* child = &m_nodes.at(name);
        parent->appendChild(child);
    }
}

Qt::ItemFlags PluginsModel::flags(const QModelIndex& index) const
{
    Qt::ItemFlags flags = QAbstractItemModel::flags(index);

    if(!index.isValid()) {
        return flags;
    }

    if(index.column() == 4) {
        flags |= Qt::ItemIsUserCheckable;
        flags |= Qt::ItemIsEditable;
    }

    return flags;
}

int PluginsModel::rowCount(const QModelIndex& /*parent*/) const
{
    return rootItem()->childCount();
}

int PluginsModel::columnCount(const QModelIndex& /*parent*/) const
{
    return 6;
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
            return QStringLiteral("Name");
        case(1):
            return QStringLiteral("Version");
        case(2):
            return QStringLiteral("Category");
        case(3):
            return QStringLiteral("Author");
        case(4):
            return QStringLiteral("Load");
        case(5):
            return QStringLiteral("Status");
        default:
            return {};
    }
    return {};
}

QVariant PluginsModel::data(const QModelIndex& index, int role) const
{
    if(!checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return {};
    }

    const int column = index.column();

    if(role == Qt::TextAlignmentRole) {
        switch(column) {
            case(0):;
                return (Qt::AlignVCenter | Qt::AlignLeft).toInt();
            case(1):
            case(2):
            case(3):
            case(5):
            case(4):
            default:
                return Qt::AlignCenter;
        }
    }

    const auto* item = static_cast<PluginItem*>(index.internalPointer());

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
                break;
            case(5):
                return Utils::Enum::toString(item->info()->status());
            default:
                break;
        }
    }

    if(role == Qt::CheckStateRole && column == 4) {
        if((item->info()->isDisabled() && !m_enabledPlugins.contains(item->info()->name()))
           || m_disabledPlugins.contains(item->info()->name())) {
            return Qt::Unchecked;
        }
        return Qt::Checked;
    }

    if(role == Qt::ToolTipRole) {
        return item->info()->isLoaded() ? item->info()->description() : item->info()->error();
    }

    return {};
}

bool PluginsModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if(!index.isValid() || role != Qt::CheckStateRole) {
        return false;
    }

    const auto* item   = static_cast<PluginItem*>(index.internalPointer());
    const bool checked = value.value<Qt::CheckState>() == Qt::Checked;
    const QString name = item->info()->name();

    if(item->info()->isDisabled()) {
        if(checked) {
            m_enabledPlugins.append(name);
        }
        else {
            m_enabledPlugins.removeOne(name);
        }
    }
    else {
        if(checked) {
            m_disabledPlugins.removeOne(name);
        }
        else {
            m_disabledPlugins.append(name);
        }
    }

    emit dataChanged(index, index, {role});

    return true;
}

QStringList PluginsModel::enabledPlugins() const
{
    return m_enabledPlugins;
}

QStringList PluginsModel::disabledPlugins() const
{
    return m_disabledPlugins;
}
} // namespace Fooyin
