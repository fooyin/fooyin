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
#include "plugininstaller.h"

#include <utils/fileutils.h>
#include <utils/settings/settingsmanager.h>

#include <QDir>
#include <QLibrary>
#include <QLoggingCategory>

#include <queue>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

using namespace Qt::StringLiterals;

Q_LOGGING_CATEGORY(PLUGIN_MANAGER, "fy.pluginmanager")

namespace Fooyin {
namespace {
bool isDepreciatedPlugin(const QString& pluginId)
{
    return pluginId == "fooyin.filters"_L1;
}

QStringList pluginFiles(const QDir& baseDir, bool skipInstallerDirs = false)
{
    QStringList files;
    std::queue<QDir> dirs;
    dirs.emplace(baseDir);

    while(!dirs.empty()) {
        const QDir directory = dirs.front();
        dirs.pop();

        const QFileInfoList subDirs = directory.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
        for(const QFileInfo& subDirectory : subDirs) {
            if(skipInstallerDirs
               && (subDirectory.fileName().startsWith(".staging-"_L1) || subDirectory.fileName() == ".updates"_L1)) {
                continue;
            }
            dirs.emplace(subDirectory.absoluteFilePath());
        }

        const QFileInfoList dirFiles = directory.entryInfoList(QDir::Files);
        for(const QFileInfo& file : dirFiles) {
            files.emplace_back(file.absoluteFilePath());
        }
    }

    return files;
}
} // namespace

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

        QStringList files;
        if(Utils::File::cleanPath(pluginDir) == Utils::File::cleanPath(Core::userPluginsPath())) {
            PluginInstaller::applyPendingUpdates(Core::userPluginsPath());
            files.append(pluginFiles(dir, true));
        }
        else {
            files = pluginFiles(dir);
        }

        const QStringList disabledPlugins = m_settings->value<Settings::Core::Internal::DisabledPlugins>();
        for(const auto& filepath : files) {
            if(!QLibrary::isLibrary(filepath)) {
                continue;
            }

            const QPluginLoader pluginLoader{filepath};
            auto metaData = pluginLoader.metaData();
            if(metaData.empty() || !metaData.contains("MetaData"_L1)) {
                continue;
            }

#ifdef Q_OS_WIN
            registerPluginDirectory(filepath);
#endif

            auto plugin         = std::make_unique<PluginInfo>(filepath, metaData);
            const auto pluginId = plugin->identifier();
            if(isDepreciatedPlugin(pluginId)) {
                continue;
            }
            if(disabledPlugins.contains(plugin->identifier())) {
                plugin->setDisabled(true);
            }

            m_plugins.emplace(plugin->identifier(), std::move(plugin));
        }
    }
}

#ifdef Q_OS_WIN
void PluginManager::registerPluginDirectory(const QString& filepath)
{
    const QString directory = QFileInfo{filepath}.absolutePath();
    if(m_registeredPluginDirectories.contains(directory)) {
        return;
    }

    if(!AddDllDirectory(reinterpret_cast<LPCWSTR>(directory.utf16()))) {
        qCWarning(PLUGIN_MANAGER) << "Failed to add plugin DLL search directory:" << directory;
        return;
    }

    m_registeredPluginDirectories.emplace(directory);
}
#endif

void PluginManager::loadPlugins()
{
    for(const auto& [name, plugin] : m_plugins) {
        if(!plugin->isDisabled() && plugin->status() == PluginInfo::Status::Discovered) {
            plugin->load();
        }
    }
}

PluginManager::InstallResult PluginManager::installPlugin(const QString& filepath, bool overwrite)
{
    switch(PluginInstaller::install(filepath, Core::userPluginsPath(), overwrite)) {
        case PluginInstaller::Result::Installed:
            return InstallResult::Installed;
        case PluginInstaller::Result::AlreadyInstalled:
            return InstallResult::AlreadyInstalled;
        case PluginInstaller::Result::Failed:
            return InstallResult::Failed;
    }
    return InstallResult::Failed;
}

void PluginManager::unloadPlugins()
{
    for(const auto& plugin : m_plugins | std::views::values) {
        plugin->unload();
    }
    m_plugins.clear();
}
} // namespace Fooyin
