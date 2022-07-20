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

#pragma once

#include "pluginsystem_global.h"

#include <QMap>
#include <QObject>
#include <QReadWriteLock>

class QPluginLoader;

namespace PluginSystem {
class Plugin;
class PluginInfo;
class PLUGINSYSTEM_EXPORT PluginManager : public QObject
{
    Q_OBJECT

public:
    struct LoadOrderEntry
    {
        QPluginLoader* loader;
        PluginInfo* plugin;
    };

    void addObject(QObject* object);
    void removeObject(QObject* object);
    QList<QObject*> allObjects();
    static PluginManager* instance();
    QReadWriteLock* objectLock();

    void findPlugins(const QString& pluginDir);
    void addPlugins();

private:
    PluginManager();
    ~PluginManager();

    mutable QReadWriteLock m_objectLock;
    QList<QObject*> m_objectList;
    QList<LoadOrderEntry> m_loadOrder;
    QMap<QString, PluginInfo*> m_plugins;
};

inline void addObject(QObject* object)
{
    PluginManager::instance()->addObject(object);
}

inline void removeObject(QObject* object)
{
    PluginManager::instance()->removeObject(object);
}

inline QList<QObject*> allObjects()
{
    return PluginManager::instance()->allObjects();
}

inline QReadWriteLock* objectLock()
{
    return PluginManager::instance()->objectLock();
}

template <typename T>
inline T* object()
{
    QReadLocker lock(objectLock());
    for(auto* object : PluginManager::instance()->allObjects()) {
        if(auto* result = qobject_cast<T*>(object)) {
            return result;
        }
    }
    return nullptr;
}

template <typename T>
inline QList<T*> objects()
{
    QReadLocker lock(objectLock());
    QList<T*> results;
    for(auto* object : PluginManager::instance()->allObjects()) {
        if(auto* result = qobject_cast<T*>(object)) {
            results.append(result);
        }
    }
    return results;
}
}; // namespace PluginSystem
