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

#pragma once

#include <utils/treeitem.h>
#include <utils/treemodel.h>

#include <set>
#include <unordered_map>

namespace Fooyin {
class PluginInfo;
class PluginManager;

class PluginItem : public TreeItem<PluginItem>
{
public:
    enum Role
    {
        Plugin = Qt::UserRole
    };

    PluginItem() = default;
    explicit PluginItem(QString name, PluginItem* parent);
    PluginItem(PluginInfo* info, PluginItem* parent);

    [[nodiscard]] QString name() const;
    [[nodiscard]] PluginInfo* info() const;

private:
    QString m_name;
    PluginInfo* m_info;
};

class PluginsModel : public TreeModel<PluginItem>
{
    Q_OBJECT

public:
    enum class Status : uint8_t
    {
        Discovered = 0,
        Disabled,
        Incompatible,
        LoadFailed,
        Loaded,
        Initialised,
        RestartRequired,
        PendingRemoval,
        RemovalFailed,
        Count,
    };

    explicit PluginsModel(PluginManager* pluginManager, QObject* parent = nullptr);
    ~PluginsModel() override;

    void reset(const QStringList& disabledPlugins, const QStringList& removedPlugins);
    void apply(const QStringList& disabledPlugins);
    void addInstalledPlugin(std::unique_ptr<PluginInfo> plugin);
    void toggleRemoval(const QString& identifier);

    [[nodiscard]] bool hasChanges() const;
    [[nodiscard]] bool canRemove(const PluginInfo* plugin) const;
    [[nodiscard]] bool isPendingRemoval(const QString& identifier) const;
    [[nodiscard]] QStringList removedPlugins() const;

    static QString statusText(Status status);
    [[nodiscard]] Status status(const PluginInfo& plugin) const;

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

    static QString categoryName(const QString& category);
    void rebuild();

    PluginManager* m_pluginManager;

    PluginNameMap m_nodes;
    std::unordered_map<QString, std::unique_ptr<PluginInfo>> m_pendingPlugins;
    QStringList m_enabledPlugins;
    QStringList m_disabledPlugins;
    std::set<QString> m_removedPlugins;
    std::set<QString> m_appliedRemovals;
    bool m_pluginsInstalled;
};
} // namespace Fooyin
