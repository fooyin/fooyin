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
#include <ranges>
#include <vector>

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

QString PluginManager::removalError(const QString& identifier) const
{
    if(const auto error = m_removalErrors.find(identifier); error != m_removalErrors.end()) {
        return error->second;
    }
    return {};
}

void PluginManager::findPlugins(const QStringList& pluginDirs)
{
    const QString userPluginsPath = Core::userPluginsPath();
    const auto pendingRemovals    = PluginInstaller::pendingRemovals();

    m_removalErrors.clear();

    QStringList failedRemovals;
    for(const QString& identifier : pendingRemovals) {
        if(const auto result = PluginInstaller::remove(identifier, userPluginsPath); !result) {
            qCWarning(PLUGIN_MANAGER) << result.error();
            m_removalErrors.emplace(identifier, result.error());
            failedRemovals.append(identifier);
        }
    }
    if(failedRemovals != pendingRemovals) {
        PluginInstaller::setPendingRemovals(failedRemovals);
    }

    for(const QString& pluginDir : pluginDirs) {
        const QDir dir{pluginDir};
        if(!dir.exists()) {
            continue;
        }

        QStringList files;
        const bool isUserDirectory = Utils::File::cleanPath(pluginDir) == Utils::File::cleanPath(userPluginsPath);
        if(isUserDirectory) {
            PluginInstaller::applyPendingUpdates(userPluginsPath, failedRemovals);
            files.append(pluginFiles(dir, true));
        }
        else {
            files = pluginFiles(dir);
        }

        std::vector<std::unique_ptr<PluginInfo>> plugins;
        for(const auto& filepath : files) {
            if(!QLibrary::isLibrary(filepath)) {
                continue;
            }

            const QPluginLoader pluginLoader{filepath};
            auto metaData = pluginLoader.metaData();
            if(metaData.empty() || !metaData.contains("MetaData"_L1)) {
                continue;
            }

            plugins.emplace_back(std::make_unique<PluginInfo>(filepath, metaData));
        }

        // Prefer managed installations within this root
        std::ranges::stable_partition(plugins, [&dir](const auto& plugin) {
            return dir.relativeFilePath(plugin->filepath()).startsWith(plugin->identifier() + u'/');
        });

        const QStringList disabledPlugins = m_settings->value<Settings::Core::Internal::DisabledPlugins>();
        for(auto& plugin : plugins) {
            const auto pluginId = plugin->identifier();
            if(isDepreciatedPlugin(pluginId)) {
                continue;
            }
            if(const auto existing = m_plugins.find(pluginId); existing != m_plugins.end()) {
                qCInfo(PLUGIN_MANAGER) << "Ignoring duplicate plugin" << plugin->filepath() << "in favour of"
                                       << existing->second->filepath();
                continue;
            }

#ifdef Q_OS_WIN
            registerPluginDirectory(plugin->filepath());
#endif

            if(disabledPlugins.contains(pluginId) || (isUserDirectory && m_removalErrors.contains(pluginId))) {
                plugin->setDisabled(true);
            }

            m_plugins.emplace(pluginId, std::move(plugin));
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

void PluginManager::unloadPlugins()
{
    for(const auto& plugin : m_plugins | std::views::values) {
        plugin->unload();
    }
    m_plugins.clear();
}
} // namespace Fooyin
