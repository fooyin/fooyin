/*
 * Fooyin
 * Copyright 2022, Luke Taylor <LukeT1@proton.me>
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

#include "plugin.h"
#include "plugininfo.h"

#include <QDir>
#include <QLibrary>
#include <QPluginLoader>

namespace PluginSystem {
void PluginManager::addObject(QObject* object)
{
    QWriteLocker lock(&m_objectLock);
    m_objectList.append(object);
}

void PluginManager::removeObject(QObject* object)
{
    QWriteLocker lock(&m_objectLock);
    m_objectList.removeAll(object);
}

QList<QObject*> PluginManager::allObjects()
{
    return m_objectList;
}

PluginManager* PluginManager::instance()
{
    static PluginManager pluginManager;
    return &pluginManager;
}

QReadWriteLock* PluginManager::objectLock()
{
    return &m_objectLock;
}

void PluginManager::findPlugins(const QString& pluginDir)
{
    QDir dir{pluginDir};
    if(!dir.exists()) {
        return;
    }

    QFileInfoList fileList{dir.entryInfoList()};

    for(const auto& file : fileList) {
        auto pluginFilename = file.absoluteFilePath();

        if(!QLibrary::isLibrary(pluginFilename)) {
            continue;
        }

        auto pluginLoader = std::make_unique<QPluginLoader>(pluginFilename);
        auto metaData = pluginLoader->metaData();

        if(metaData.isEmpty()) {
            continue;
        }

        auto pluginMetadata = metaData.value("MetaData");
        auto version = metaData.value("version");

        auto name = pluginMetadata.toObject().value("Name");

        if(name.isNull()) {
            continue;
        }

        auto* plugin = new PluginInfo(name.toString(), pluginFilename, metaData);

        m_plugins.insert(name.toString(), plugin);
    }
}

void PluginManager::addPlugins()
{
    for(const auto& plugin : qAsConst(m_plugins)) {
        auto metadata = plugin->metadata().value("MetaData").toObject();

        auto* pluginLoader = new QPluginLoader(plugin->filename());

        if(!pluginLoader->load()) {
            qDebug() << QString("Plugin %1 couldn't be loaded (%2)").arg(plugin->name(), pluginLoader->errorString());
            continue;
        }

        auto* pluginInterface = qobject_cast<Plugin*>(pluginLoader->instance());

        if(!pluginInterface) {
            qDebug() << QString("Plugin %1 couldn't be loaded").arg(plugin->name());
            continue;
        }

        m_loadOrder.append({pluginLoader, plugin});
        plugin->m_isLoaded = true;

        for(auto loadPlugin : m_loadOrder) {
            auto* loaderPlugin = qobject_cast<Plugin*>(loadPlugin.loader->instance());
            loaderPlugin->initialise();
        }

        int i = static_cast<int>(m_loadOrder.size() - 1);
        for(; i >= 0; --i) {
            auto* loaderPlugin = qobject_cast<Plugin*>(m_loadOrder.at(i).loader->instance());
            loaderPlugin->pluginsInitialised();
        }
    }
}

PluginManager::PluginManager() = default;

PluginManager::~PluginManager() = default;
}; // namespace PluginSystem
