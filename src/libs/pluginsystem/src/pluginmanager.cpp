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

#include <QDir>
#include <QJsonArray>
#include <QLibrary>
#include <QPluginLoader>

namespace PluginSystem {
struct PluginManager::Private
{
    mutable QReadWriteLock objectLock;
    QList<QObject*> objectList;
    QList<PluginInfo*> loadOrder;
    QMap<QString, PluginInfo*> plugins;
};

void PluginManager::addObject(QObject* object)
{
    QWriteLocker lock(&p->objectLock);
    p->objectList.append(object);
}

void PluginManager::removeObject(QObject* object)
{
    QWriteLocker lock(&p->objectLock);
    p->objectList.removeAll(object);
}

QList<QObject*> PluginManager::allObjects()
{
    return p->objectList;
}

PluginManager* PluginManager::instance()
{
    static PluginManager pluginManager;
    return &pluginManager;
}

QReadWriteLock* PluginManager::objectLock()
{
    return &p->objectLock;
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

        p->plugins.insert(name.toString(), plugin);
    }
}

QList<PluginInfo*> PluginManager::loadOrder()
{
    QList<PluginInfo*> queue;
    for(auto* plugin : qAsConst(p->plugins)) {
        loadOrder(plugin, queue);
    }
    p->loadOrder = queue;
    return queue;
}

bool PluginManager::loadOrder(PluginInfo* plugin, QList<PluginInfo*>& queue)
{
    if(queue.contains(plugin)) {
        return true;
    }

    if(plugin->status() == PluginInfo::Invalid || plugin->status() == PluginInfo::Read) {
        queue.append(plugin);
        return false;
    }

    const QList<PluginInfo*> deps = plugin->dependencies();
    for(auto* dep : deps) {
        if(!loadOrder(dep, queue)) {
            plugin->setError(QString("Cannot load plugin because dependency failed to load: %1 (%2)\nReason: %3")
                                 .arg(dep->name(), dep->version(), dep->error()));
            return false;
        }
    }
    queue.append(plugin);
    return true;
}

void PluginManager::loadPlugins()
{
    const QList<PluginInfo*> queue = loadOrder();
    for(PluginInfo* plugin : queue) {
        auto metadata = plugin->metadata();
        auto dependencies = metadata.value("Dependencies").toArray();
        for(auto dependency : dependencies) {
            auto dependencyName = dependency.toObject().value("Name").toString();
            if(p->plugins.contains(dependencyName)) {
                plugin->addDependency(p->plugins.value(dependencyName));
            }
        }
        loadPlugin(plugin);
    }

    for(PluginInfo* plugin : queue) {
        initialisePlugin(plugin);
    }

    int i = static_cast<int>(queue.size() - 1);
    for(; i >= 0; --i) {
        queue.at(i)->finalise();
    }
}

void PluginManager::loadPlugin(PluginInfo* plugin)
{
    if(plugin->hasError()) {
        return;
    }

    if(plugin->isDisabled()) {
        return;
    }

    const QList<PluginInfo*> dependencies = plugin->dependencies();
    for(const auto& dependency : dependencies) {
        if(!dependency->isRequired()) {
            continue;
        }

        if(dependency->status() != PluginInfo::Loaded) {
            plugin->setError(
                PluginManager::tr("Cannot load plugin because dependency failed to load: %1(%2)\nReason: %3")
                    .arg(plugin->name(), plugin->version(), plugin->error()));
            return;
        }
    }
    plugin->load();
}

void PluginManager::initialisePlugin(PluginInfo* plugin)
{
    if(!plugin->isLoaded()) {
        return;
    }
    plugin->initialise();
}

void PluginManager::unloadPlugins()
{
    int i = static_cast<int>(p->loadOrder.size() - 1);
    for(; i >= 0; --i) {
        auto* plugin = p->loadOrder.at(i);
        plugin->unload();
        delete plugin;
    }
    p->loadOrder.clear();
}

void PluginManager::shutdown()
{
    unloadPlugins();
}

PluginManager::PluginManager()
    : p(std::make_unique<Private>())
{ }

PluginManager::~PluginManager() = default;

}; // namespace PluginSystem
