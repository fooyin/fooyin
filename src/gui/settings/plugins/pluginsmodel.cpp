/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <luket@pm.me>
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

#include <core/corepaths.h>
#include <core/plugins/plugin.h>
#include <core/plugins/plugininstaller.h>
#include <core/plugins/pluginmanager.h>

#include <ranges>

using namespace Qt::StringLiterals;

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
    , m_pluginsInstalled{false}
{ }

PluginsModel::~PluginsModel() = default;

void PluginsModel::reset(const QStringList& disabledPlugins, const QStringList& removedPlugins)
{
    beginResetModel();

    m_enabledPlugins.clear();
    m_disabledPlugins.clear();

    m_removedPlugins  = {removedPlugins.cbegin(), removedPlugins.cend()};
    m_appliedRemovals = m_removedPlugins;

    for(const auto& [identifier, plugin] : m_pendingPlugins) {
        plugin->setDisabled(disabledPlugins.contains(identifier));
    }

    rebuild();
    endResetModel();
}

void PluginsModel::apply(const QStringList& disabledPlugins)
{
    m_pluginsInstalled = false;
    reset(disabledPlugins, removedPlugins());
}

void PluginsModel::addInstalledPlugin(std::unique_ptr<PluginInfo> plugin)
{
    beginResetModel();

    const QString identifier = plugin->identifier();
    m_removedPlugins.erase(identifier);
    m_appliedRemovals.erase(identifier);
    m_pendingPlugins.insert_or_assign(identifier, std::move(plugin));
    m_pluginsInstalled = true;
    rebuild();

    endResetModel();
}

void PluginsModel::toggleRemoval(const QString& identifier)
{
    const auto node = m_nodes.find(identifier);
    if(node == m_nodes.end() || !canRemove(node->second.info())) {
        return;
    }

    if(!m_removedPlugins.erase(identifier)) {
        m_removedPlugins.emplace(identifier);
    }

    const QModelIndex index = indexOfItem(&node->second);
    Q_EMIT dataChanged(index, index.siblingAtColumn(4), {Qt::DisplayRole, Qt::CheckStateRole, Qt::ToolTipRole});
}

bool PluginsModel::hasChanges() const
{
    return m_pluginsInstalled || !m_enabledPlugins.empty() || !m_disabledPlugins.empty()
        || m_removedPlugins != m_appliedRemovals;
}

bool PluginsModel::canRemove(const PluginInfo* plugin) const
{
    return plugin
        && (isPendingRemoval(plugin->identifier()) || PluginInstaller::canRemove(*plugin, Core::userPluginsPath()));
}

bool PluginsModel::isPendingRemoval(const QString& identifier) const
{
    return m_removedPlugins.contains(identifier);
}

QStringList PluginsModel::removedPlugins() const
{
    return QStringList{m_removedPlugins.cbegin(), m_removedPlugins.cend()};
}

QString PluginsModel::statusText(Status status)
{
    switch(status) {
        case Status::Discovered:
            return tr("Discovered");
        case Status::Disabled:
            return tr("Disabled");
        case Status::Incompatible:
            return tr("Incompatible");
        case Status::LoadFailed:
            return tr("Load failed");
        case Status::Loaded:
            return tr("Loaded");
        case Status::Initialised:
            return tr("Initialised");
        case Status::RestartRequired:
            return tr("Restart required");
        case Status::PendingRemoval:
            return tr("Pending removal");
        case Status::RemovalFailed:
            return tr("Removal failed");
        case Status::Count:
            break;
    }
    return {};
}

PluginsModel::Status PluginsModel::status(const PluginInfo& plugin) const
{
    const QString identifier = plugin.identifier();
    if(isPendingRemoval(identifier)) {
        return m_pluginManager->removalError(identifier).isEmpty() ? Status::PendingRemoval : Status::RemovalFailed;
    }
    if(m_pendingPlugins.contains(identifier)) {
        return Status::RestartRequired;
    }

    switch(plugin.status()) {
        case PluginInfo::Status::Discovered:
            return Status::Discovered;
        case PluginInfo::Status::Disabled:
            return Status::Disabled;
        case PluginInfo::Status::Incompatible:
            return Status::Incompatible;
        case PluginInfo::Status::LoadFailed:
            return Status::LoadFailed;
        case PluginInfo::Status::Loaded:
            return Status::Loaded;
        case PluginInfo::Status::Initialised:
            return Status::Initialised;
    }
    return Status::Discovered;
}

Qt::ItemFlags PluginsModel::flags(const QModelIndex& index) const
{
    Qt::ItemFlags flags = QAbstractItemModel::flags(index);

    if(!index.isValid()) {
        return flags;
    }

    const auto* item = itemForIndex(index);

    if(index.column() == 3 && item->info() && !isPendingRemoval(item->info()->identifier())) {
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
        return Qt::AlignCenter;
    }

    if(role != Qt::DisplayRole || orientation == Qt::Orientation::Vertical) {
        return {};
    }

    switch(section) {
        case 0:
            return tr("Name");
        case 1:
            return tr("Version");
        case 2:
            return tr("Author");
        case 3:
            return tr("Load");
        case 4:
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
            case 0:
                return (Qt::AlignVCenter | Qt::AlignLeft).toInt();
            case 1:
            case 2:
            case 3:
            case 5:
            case 4:
            default:
                return Qt::AlignCenter;
        }
    }

    const auto* item = itemForIndex(index);
    const auto* info = item->info();

    if(!info) {
        if(role == Qt::DisplayRole && column == 0) {
            return categoryName(item->name());
        }
        return {};
    }

    if(role == Qt::CheckStateRole && column == 3) {
        if(isPendingRemoval(info->identifier())
           || (info->isDisabled() && !m_enabledPlugins.contains(info->identifier()))
           || m_disabledPlugins.contains(info->identifier())) {
            return Qt::Unchecked;
        }
        return Qt::Checked;
    }

    if(role == Qt::ToolTipRole) {
        if(isPendingRemoval(info->identifier())) {
            const QString error = m_pluginManager->removalError(info->identifier());
            return error.isEmpty() ? tr("This plugin is marked for removal at the next restart.") : error;
        }
        return info->isLoaded() || m_pendingPlugins.contains(info->identifier()) ? info->description() : info->error();
    }

    if(role == PluginItem::Plugin) {
        return QVariant::fromValue(info);
    }

    if(role == Qt::DisplayRole) {
        switch(column) {
            case 0:
                return info->name();
            case 1:
                return info->version();
            case 2:
                return info->author();
            case 4:
                return statusText(status(*info));
            default:
                break;
        }
    }

    return {};
}

bool PluginsModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if(!index.isValid() || index.column() != 3 || role != Qt::CheckStateRole || !itemForIndex(index)->info()) {
        return false;
    }

    const auto* item = static_cast<PluginItem*>(index.internalPointer());
    if(isPendingRemoval(item->info()->identifier())) {
        return false;
    }

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

    Q_EMIT dataChanged(index, index, {role});

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

QString PluginsModel::categoryName(const QString& category)
{
    // Translates the common category names used by fooyin's built-in plugins.
    // Plugins can use any categories they want, so unknown names are left untranslated.

    if(category == "Audio"_L1) {
        return tr("Audio");
    }
    if(category == "Output"_L1) {
        return tr("Output");
    }
    if(category == "System Integration"_L1) {
        return tr("System Integration");
    }
    if(category == "Online Services"_L1) {
        return tr("Online Services");
    }
    if(category == "Display"_L1) {
        return tr("Display");
    }
    if(category == "Visualisations"_L1) {
        return tr("Visualisations");
    }
    if(category == "Analysis"_L1) {
        return tr("Analysis");
    }
    if(category == "Input"_L1) {
        return tr("Input");
    }
    if(category == "Widgets"_L1) {
        return tr("Widgets");
    }
    if(category == "Library"_L1) {
        return tr("Library");
    }
    if(category == "Tagging"_L1) {
        return tr("Tagging");
    }
    if(category == "DSP"_L1) {
        return tr("DSP");
    }
    if(category == "File Management"_L1) {
        return tr("File Management");
    }

    return category;
}

void PluginsModel::rebuild()
{
    resetRoot();
    m_nodes.clear();

    const auto appendPlugin = [this](PluginInfo* info) {
        PluginItem* parent = rootItem();

        const auto categories = info->category();

        for(const QString& category : categories) {
            if(!m_nodes.contains(category)) {
                m_nodes.emplace(category, PluginItem{category, parent});
                parent->appendChild(&m_nodes.at(category));
            }
            parent = &m_nodes.at(category);
        }

        const QString identifier = info->identifier();
        if(!m_nodes.contains(identifier)) {
            m_nodes.emplace(identifier, PluginItem{info, parent});
        }
        PluginItem* child = &m_nodes.at(identifier);
        parent->appendChild(child);
    };

    for(const auto& [identifier, info] : m_pluginManager->allPluginInfo()) {
        if(!m_pendingPlugins.contains(identifier)) {
            appendPlugin(info.get());
        }
    }
    for(const auto& info : m_pendingPlugins | std::views::values) {
        appendPlugin(info.get());
    }
}
} // namespace Fooyin
