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

#include "pluginmanager.h"

#include <QDir>
#include <QJsonArray>
#include <QLibrary>
#include <QPluginLoader>

namespace Plugins {
PluginManager::PluginManager(QObject* parent)
    : QObject{parent}
{ }

PluginManager::~PluginManager() = default;

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

        auto* plugin = new PluginInfo(name.toString(), pluginFilename, metaData);

        m_plugins.emplace(name.toString(), plugin);
    }
}

void PluginManager::loadPlugins()
{
    for(const auto& [name, plugin] : m_plugins) {
        auto metadata = plugin->metadata();
        loadPlugin(plugin);
        if(!plugin->isLoaded()) {
            continue;
        }
    }
}

void PluginManager::initialisePlugins()
{
    for(const auto& [name, plugin] : m_plugins) {
        plugin->initialise();
    }
}

void PluginManager::loadPlugin(PluginInfo* plugin)
{
    if(plugin->hasError() || plugin->isDisabled()) {
        return;
    }
    plugin->load();
}

void PluginManager::unloadPlugins()
{
    for(const auto& [name, plugin] : m_plugins) {
        plugin->unload();
        delete plugin;
    }
}

void PluginManager::shutdown()
{
    unloadPlugins();
}

} // namespace Plugins
