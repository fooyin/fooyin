/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
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

#include "librarymonitor.h"

#include "libraryscanutils.h"

#include <utils/fileutils.h>
#include <utils/scopeguard.h>

#include <QDir>
#include <QFileInfo>
#include <QLoggingCategory>

#include <ranges>
#include <set>
#include <vector>

constexpr qsizetype WatchBatchSize = 64;

namespace {
bool addPaths(Fooyin::LibraryWatcher& watcher, const QStringList& paths, QStringList& failedPaths,
              const std::stop_token stopToken)
{
    for(qsizetype offset{0}; offset < paths.size(); offset += WatchBatchSize) {
        if(stopToken.stop_requested()) {
            return false;
        }

        const qsizetype count = std::min(WatchBatchSize, paths.size() - offset);
        failedPaths.append(watcher.addPaths(paths.sliced(offset, count)));
    }

    return !stopToken.stop_requested();
}
} // namespace

namespace Fooyin {
LibraryMonitor::LibraryMonitor(QObject* parent)
    : QObject{parent}
{ }

std::stop_token LibraryMonitor::prepareSetup()
{
    const std::scoped_lock lock{m_setupMutex};
    m_setupStopSource = std::stop_source{};
    return m_setupStopSource.get_token();
}

void LibraryMonitor::cancelSetup()
{
    const std::scoped_lock lock{m_setupMutex};
    m_setupStopSource.request_stop();
}

void LibraryMonitor::setupWatchers(const LibraryInfoMap& libraries, const TrackList& tracks, bool monitorDirectories,
                                   bool monitorTrackFiles, std::stop_token stopToken)
{
    const auto finished = scopeGuard([this]() { Q_EMIT setupFinished(); });
    if(stopToken.stop_requested()) {
        return;
    }

    std::vector<int> removedLibraries;
    for(const auto& id : m_watchers | std::views::keys) {
        if(!libraries.contains(id)) {
            removedLibraries.push_back(id);
        }
    }
    for(const int id : removedLibraries) {
        m_watchers.erase(id);
    }

    m_libraries = libraries;

    std::unordered_map<int, std::set<QString>> trackFiles;
    if(monitorDirectories && monitorTrackFiles) {
        for(const Track& track : tracks) {
            if(stopToken.stop_requested()) {
                return;
            }
            if(track.hasCue() || !m_libraries.contains(track.libraryId())) {
                continue;
            }

            const QString path = physicalTrackPath(track);
            if(!path.isEmpty() && QFileInfo::exists(path)) {
                trackFiles[track.libraryId()].emplace(path);
            }
        }
    }

    for(const auto& library : libraries | std::views::values) {
        if(stopToken.stop_requested()) {
            return;
        }

        if(!monitorDirectories) {
            if(library.status == LibraryInfo::Status::Monitoring) {
                LibraryInfo updatedLibrary{library};
                updatedLibrary.status = LibraryInfo::Status::Idle;
                Q_EMIT statusChanged(updatedLibrary);
            }
        }
        else {
            if(!m_watchers.contains(library.id)) {
                if(!addWatcher(library, stopToken)) {
                    m_watchers.erase(library.id);
                    return;
                }
            }

            if(!syncTrackFiles(library, trackFiles[library.id], stopToken)) {
                return;
            }

            LibraryInfo updatedLibrary{library};
            updatedLibrary.status          = LibraryInfo::Status::Monitoring;
            m_libraries[updatedLibrary.id] = updatedLibrary;
            if(library.status != LibraryInfo::Status::Monitoring) {
                Q_EMIT statusChanged(updatedLibrary);
            }
        }
    }

    if(!monitorDirectories) {
        m_watchers.clear();
    }
}

void LibraryMonitor::shutdown()
{
    m_watchers.clear();
}

bool LibraryMonitor::addDirectoryPaths(const int libraryId, const QString& path, const std::stop_token stopToken)
{
    if(stopToken.stop_requested() || !m_watchers.contains(libraryId) || !m_libraries.contains(libraryId)) {
        return false;
    }

    QStringList dirs = Utils::File::getAllSubdirectories(QDir{path}, stopToken);
    if(stopToken.stop_requested()) {
        return false;
    }

    dirs.append(path);

    auto& watcher                  = m_watchers.at(libraryId);
    const QStringList watchedPaths = watcher.directories();
    const std::set<QString> watchedSet{watchedPaths.cbegin(), watchedPaths.cend()};

    QStringList newPaths;
    newPaths.reserve(dirs.size());
    for(const QString& dir : dirs) {
        if(!watchedSet.contains(dir)) {
            newPaths.push_back(dir);
        }
    }

    QStringList failedPaths;
    const bool completed = addPaths(watcher, newPaths, failedPaths, stopToken);
    if(!failedPaths.isEmpty()) {
        qCWarning(LIB_WATCHER) << "Failed to monitor library directories for" << m_libraries.at(libraryId).name
                               << failedPaths;
    }
    return completed;
}

bool LibraryMonitor::addWatcher(const LibraryInfo& library, const std::stop_token stopToken)
{
    m_watchers.try_emplace(library.id);
    if(!addDirectoryPaths(library.id, library.path, stopToken)) {
        return false;
    }

    auto& watcher = m_watchers.at(library.id);

    QObject::connect(&watcher, &LibraryWatcher::libraryDirsChanged, this,
                     [this, libraryId = library.id](const QStringList& dirs) {
                         for(const QString& dir : dirs) {
                             if(!addDirectoryPaths(libraryId, dir, {})) {
                                 return;
                             }
                         }

                         if(!m_setupStopSource.stop_requested() && m_libraries.contains(libraryId)) {
                             Q_EMIT directoriesChanged(m_libraries.at(libraryId), dirs);
                         }
                     });
    QObject::connect(&watcher, &LibraryWatcher::libraryTrackFilesChanged, this,
                     [this, libraryId = library.id](const QStringList& files) {
                         if(!m_setupStopSource.stop_requested() && m_libraries.contains(libraryId)) {
                             Q_EMIT trackFilesChanged(m_libraries.at(libraryId), files);
                         }
                     });

    return true;
}

bool LibraryMonitor::syncTrackFiles(const LibraryInfo& library, const std::set<QString>& files,
                                    const std::stop_token stopToken)
{
    auto& watcher                  = m_watchers.at(library.id);
    const QStringList watchedPaths = watcher.files();
    const std::set<QString> watchedSet{watchedPaths.cbegin(), watchedPaths.cend()};

    QStringList removedPaths;
    for(const QString& path : watchedSet) {
        if(!files.contains(path)) {
            removedPaths.push_back(path);
        }
    }
    if(!removedPaths.isEmpty()) {
        const QStringList failedPaths = watcher.removePaths(removedPaths);
        if(!failedPaths.isEmpty()) {
            qCWarning(LIB_WATCHER) << "Failed to stop monitoring track files for" << library.name << failedPaths;
        }
    }

    QStringList newPaths;
    newPaths.reserve(static_cast<qsizetype>(files.size()));
    for(const QString& path : files) {
        if(!watchedSet.contains(path)) {
            newPaths.push_back(path);
        }
    }

    QStringList failedPaths;
    const bool completed = addPaths(watcher, newPaths, failedPaths, stopToken);
    if(!failedPaths.isEmpty()) {
        qCWarning(LIB_WATCHER) << "Failed to monitor track files for" << library.name << failedPaths;
    }
    return completed;
}
} // namespace Fooyin

#include "moc_librarymonitor.cpp"
