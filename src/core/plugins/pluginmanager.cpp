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

#include "pluginmanager.h"

#include "corepaths.h"

#include <QDir>
#include <QLibrary>

namespace Fooyin {
PluginManager::PluginManager() = default;

const PluginInfoMap& PluginManager::allPluginInfo() const
{
    return m_plugins;
}

void PluginManager::findPlugins(const QStringList& pluginDirs)
{
    for(const QString& pluginDir : pluginDirs) {
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

            const QString error = pluginLoader->errorString();

            auto pluginMetadata = metaData.value(QStringLiteral("MetaData"));
            auto version        = metaData.value(QStringLiteral("Version"));

            QString name = pluginMetadata.toObject().value(QStringLiteral("Name")).toString();

            if(name.isEmpty()) {
                name = file.fileName();
            }

            auto* plugin = m_plugins.emplace(name, std::make_unique<PluginInfo>(name, pluginFilename, metaData))
                               .first->second.get();
            if(!error.isEmpty()) {
                plugin->setError(error);
            }
        }
    }
}

void PluginManager::loadPlugins()
{
    for(const auto& [name, plugin] : m_plugins) {
        loadPlugin(plugin.get());
        if(!plugin->isLoaded()) {
            continue;
        }
    }
}

bool PluginManager::installPlugin(const QString& filepath)
{
    QFile pluginFile{filepath};
    const QFileInfo fileInfo{filepath};

    const QString newPlugin = Core::userPluginsPath() + QStringLiteral("/") + fileInfo.fileName();
    return pluginFile.copy(newPlugin);
}

void PluginManager::loadPlugin(PluginInfo* plugin)
{
    if(plugin->isDisabled()) {
        return;
    }
    plugin->load();
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

} // namespace Fooyin
