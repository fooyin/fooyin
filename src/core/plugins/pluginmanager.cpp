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

#include "pluginmanager.h"

#include "corepaths.h"
#include "internalcoresettings.h"

#include <utils/fileutils.h>
#include <utils/settings/settingsmanager.h>

#include <QDir>
#include <QLibrary>
#include <QSaveFile>

using namespace Qt::StringLiterals;

namespace Fooyin {
PluginManager::PluginManager(SettingsManager* settings)
    : m_settings{settings}
{ }

const PluginInfoMap& PluginManager::allPluginInfo() const
{
    return m_plugins;
}

PluginInfo* PluginManager::pluginInfo(const QString& identifier) const
{
    if(m_plugins.contains(identifier)) {
        return m_plugins.at(identifier).get();
    }
    return nullptr;
}

void PluginManager::findPlugins(const QStringList& pluginDirs)
{
    for(const QString& pluginDir : pluginDirs) {
        const QDir dir{pluginDir};
        if(!dir.exists()) {
            continue;
        }

        const QStringList disabledPlugins = m_settings->value<Settings::Core::Internal::DisabledPlugins>();

        const auto files = Utils::File::getFilesInDirRecursive(dir, {});
        for(const auto& filepath : files) {
            if(!QLibrary::isLibrary(filepath)) {
                continue;
            }

            const QPluginLoader pluginLoader{filepath};
            auto metaData = pluginLoader.metaData();
            if(metaData.empty() || !metaData.contains("MetaData"_L1)) {
                continue;
            }

            auto plugin = std::make_unique<PluginInfo>(filepath, metaData);
            if(disabledPlugins.contains(plugin->identifier())) {
                plugin->setDisabled(true);
            }

            m_plugins.emplace(plugin->identifier(), std::move(plugin));
        }
    }
}

void PluginManager::loadPlugins()
{
    for(const auto& [name, plugin] : m_plugins) {
        if(!plugin->isDisabled()) {
            plugin->load();
        }
    }
}

PluginManager::InstallResult PluginManager::installPlugin(const QString& filepath, bool overwrite)
{
    QFile pluginFile{filepath};
    const QFileInfo fileInfo{filepath};

    const QString newPlugin = Core::userPluginsPath() + u"/"_s + fileInfo.fileName();
    if(QFileInfo::exists(newPlugin) && !overwrite) {
        return InstallResult::AlreadyInstalled;
    }

    if(!pluginFile.open(QIODevice::ReadOnly)) {
        return InstallResult::Failed;
    }

    QSaveFile newPluginFile{newPlugin};
    if(!newPluginFile.open(QIODevice::WriteOnly)) {
        return InstallResult::Failed;
    }

    while(!pluginFile.atEnd()) {
        const QByteArray data = pluginFile.read(1024LL * 1024);
        if(data.isEmpty() || newPluginFile.write(data) != data.size()) {
            newPluginFile.cancelWriting();
            return InstallResult::Failed;
        }
    }

    if(pluginFile.error() != QFileDevice::NoError) {
        newPluginFile.cancelWriting();
        return InstallResult::Failed;
    }

    newPluginFile.setPermissions(pluginFile.permissions());
    return newPluginFile.commit() ? InstallResult::Installed : InstallResult::Failed;
}

void PluginManager::unloadPlugins()
{
    for(const auto& plugin : m_plugins | std::views::values) {
        plugin->unload();
    }
    m_plugins.clear();
}
} // namespace Fooyin
