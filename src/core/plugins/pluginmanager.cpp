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

#include <core/plugins/pluginmanager.h>

#include <QDir>
#include <QLibrary>

namespace Fy::Plugins {
const PluginInfoMap& PluginManager::allPluginInfo() const
{
    return m_plugins;
}

void PluginManager::findPlugins(const QString& pluginDir)
{
    const QDir dir{pluginDir};
    if(!dir.exists()) {
        return;
    }

    const QFileInfoList fileList{dir.entryInfoList()};

    for(const auto& file : fileList) {
        auto pluginFilename = file.absoluteFilePath();

        if(!QLibrary::isLibrary(pluginFilename)) {
            continue;
        }

        auto pluginLoader = std::make_unique<QPluginLoader>(pluginFilename);
        auto metaData     = pluginLoader->metaData();

        if(metaData.isEmpty()) {
            continue;
        }

        auto pluginMetadata = metaData.value("MetaData");
        auto version        = metaData.value("version");

        auto name = pluginMetadata.toObject().value("Name");

        if(name.isNull()) {
            continue;
        }

        m_plugins.emplace(name.toString(), std::make_unique<PluginInfo>(name.toString(), pluginFilename, metaData));
    }
}

void PluginManager::loadPlugins()
{
    for(const auto& [name, plugin] : m_plugins) {
        auto metadata = plugin->metadata();
        loadPlugin(plugin.get());
        if(!plugin->isLoaded()) {
            continue;
        }
    }
}

void PluginManager::loadPlugin(PluginInfo* plugin)
{
    if(plugin->isDisabled()) {
        return;
    }
    plugin->load();
    if(plugin->hasError()) {
        qCritical() << plugin->error();
    }
}

void PluginManager::unloadPlugins()
{
    for(const auto& [name, plugin] : m_plugins) {
        plugin->unload();
    }
    m_plugins.clear();
}

void PluginManager::shutdown()
{
    unloadPlugins();
}

} // namespace Fy::Plugins
