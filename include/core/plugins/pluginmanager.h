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

#pragma once

#include <core/plugins/plugininfo.h>

namespace Fy::Plugins {
using PluginInfoMap = std::unordered_map<QString, std::unique_ptr<PluginInfo>>;

class FYCORE_EXPORT PluginManager
{
public:
    const PluginInfoMap& allPluginInfo() const;

    void findPlugins(const QString& pluginDir);
    void loadPlugins();

    template <typename T, typename Function>
    void initialisePlugins(Function function)
    {
        for(auto& [name, plugin] : m_plugins) {
            if(const auto& pluginInstance = qobject_cast<T*>(plugin->root())) {
                function(pluginInstance);
                plugin->initialise();
            }
        }
    }

    void loadPlugin(PluginInfo* plugin);
    void unloadPlugins();

    void shutdown();

private:
    PluginInfoMap m_plugins;
};
} // namespace Fy::Plugins
