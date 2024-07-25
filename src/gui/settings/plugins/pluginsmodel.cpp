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

#include <core/plugins/plugin.h>
#include <core/plugins/pluginmanager.h>
#include <utils/enum.h>

namespace Fooyin {
PluginItem::PluginItem(PluginInfo* info, PluginItem* parent)
    : TreeItem{parent}
    , m_info{info}
{ }

PluginItem::PluginItem(QString name, PluginItem* parent)
    : TreeItem{parent}
    , m_name{std::move(name)}
    , m_info{nullptr}
{ }

QString PluginItem::name() const
{
    return m_info ? m_info->name() : m_name;
}

PluginInfo* PluginItem::info() const
{
    return m_info;
}

PluginsModel::PluginsModel(PluginManager* pluginManager, QObject* parent)
    : TreeModel{parent}
    , m_pluginManager{pluginManager}
{ }

void PluginsModel::reset()
{
    beginResetModel();
    resetRoot();

    const auto& plugins = m_pluginManager->allPluginInfo();

    for(const auto& [name, info] : plugins) {
        PluginItem* parent = rootItem();

        const auto categories = info->category();

        for(const QString& category : categories) {
            if(!m_nodes.contains(category)) {
                m_nodes.emplace(category, PluginItem{category, parent});
                parent->appendChild(&m_nodes.at(category));
            }
            parent = &m_nodes.at(category);
        }

        if(!m_nodes.contains(name)) {
            m_nodes.emplace(name, PluginItem{info.get(), parent});
        }
        PluginItem* child = &m_nodes.at(name);
        parent->appendChild(child);
    }

    endResetModel();
}

Qt::ItemFlags PluginsModel::flags(const QModelIndex& index) const
{
    Qt::ItemFlags flags = QAbstractItemModel::flags(index);

    if(!index.isValid()) {
        return flags;
    }

    if(index.column() == 3) {
        flags |= Qt::ItemIsUserCheckable;
        flags |= Qt::ItemIsEditable;
    }

    return flags;
}

int PluginsModel::rowCount(const QModelIndex& parent) const
{
    return itemForIndex(parent)->childCount();
}

int PluginsModel::columnCount(const QModelIndex& /*parent*/) const
{
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
            return tr("Name");
        case(1):
            return tr("Version");
        case(2):
            return tr("Author");
        case(3):
            return tr("Load");
        case(4):
            return tr("Status");
        default:
            break;
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
            case(0):
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

    const auto* item = itemForIndex(index);
    const auto* info = item->info();

    if(!info) {
        if(role == Qt::DisplayRole && column == 0) {
            return item->name();
        }
        return {};
    }

    if(role == Qt::CheckStateRole && column == 3) {
        if((info->isDisabled() && !m_enabledPlugins.contains(info->identifier()))
           || m_disabledPlugins.contains(info->identifier())) {
            return Qt::Unchecked;
        }
        return Qt::Checked;
    }

    if(role == Qt::ToolTipRole) {
        return info->isLoaded() ? info->description() : info->error();
    }

    if(role == PluginItem::Plugin) {
        return QVariant::fromValue(info);
    }

    if(role == Qt::DisplayRole) {
        switch(column) {
            case(0):
                return info->name();
            case(1):
                return info->version();
            case(2):
                return info->author();
            case(4):
                return Utils::Enum::toString(info->status());
            default:
                break;
        }
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

    if(item->info()->isDisabled()) {
        if(checked) {
            m_enabledPlugins.append(item->info()->identifier());
        }
        else {
            m_enabledPlugins.removeOne(item->info()->identifier());
        }
    }
    else {
        if(checked) {
            m_disabledPlugins.removeOne(item->info()->identifier());
        }
        else {
            m_disabledPlugins.append(item->info()->identifier());
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
