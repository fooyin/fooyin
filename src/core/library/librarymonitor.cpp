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

void LibraryMonitor::cancelSetup()
{
    m_setupStopSource.request_stop();
}

void LibraryMonitor::setupWatchers(const LibraryInfoMap& libraries, const TrackList& tracks, bool monitorDirectories,
                                   bool monitorTrackFiles)
{
    const std::stop_token stopToken = m_setupStopSource.get_token();
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
            if(m_watchers.contains(library.id)) {
                m_watchers.erase(library.id);
            }

            if(!addWatcher(library, tracks, monitorTrackFiles, stopToken)) {
                return;
            }

            LibraryInfo updatedLibrary{library};
            updatedLibrary.status = LibraryInfo::Status::Monitoring;
            Q_EMIT statusChanged(updatedLibrary);
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

bool LibraryMonitor::addWatcher(const LibraryInfo& library, const TrackList& tracks, const bool monitorTrackFiles,
                                const std::stop_token stopToken)
{
    const auto watchPaths = [this, library, stopToken](const QString& path) {
        if(stopToken.stop_requested()) {
            return false;
        }

        QStringList dirs = Utils::File::getAllSubdirectories(QDir{path}, stopToken);
        if(stopToken.stop_requested()) {
            return false;
        }

        dirs.append(path);

        auto& watcher                  = m_watchers[library.id];
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
            qCWarning(LIB_WATCHER) << "Failed to monitor library directories for" << library.name << failedPaths;
        }
        return completed;
    };

    if(!watchPaths(library.path)) {
        return false;
    }

    auto& watcher = m_watchers[library.id];

    QObject::connect(&watcher, &LibraryWatcher::libraryDirsChanged, this,
                     [this, watchPaths, library](const QStringList& dirs) {
                         for(const QString& dir : dirs) {
                             if(!watchPaths(dir)) {
                                 return;
                             }
                         }

                         if(!m_setupStopSource.stop_requested()) {
                             Q_EMIT directoriesChanged(library, dirs);
                         }
                     });
    QObject::connect(&watcher, &LibraryWatcher::libraryTrackFilesChanged, this,
                     [this, library](const QStringList& files) {
                         if(!m_setupStopSource.stop_requested()) {
                             Q_EMIT trackFilesChanged(library, files);
                         }
                     });

    if(!monitorTrackFiles) {
        return true;
    }

    std::set<QString> files;
    for(const Track& track : tracks) {
        if(stopToken.stop_requested()) {
            return false;
        }

        if(track.libraryId() != library.id || track.hasCue()) {
            continue;
        }

        const QString path = physicalTrackPath(track);
        if(!path.isEmpty() && QFileInfo::exists(path)) {
            files.emplace(path);
        }
    }

    QStringList watchFiles;
    watchFiles.reserve(static_cast<qsizetype>(files.size()));
    for(const QString& file : files) {
        if(stopToken.stop_requested()) {
            return false;
        }

        watchFiles.push_back(file);
    }

    QStringList failedPaths;
    const bool completed = addPaths(watcher, watchFiles, failedPaths, stopToken);
    if(!failedPaths.isEmpty()) {
        qCWarning(LIB_WATCHER) << "Failed to monitor track files for" << library.name << failedPaths;
    }
    return completed;
}
} // namespace Fooyin

#include "moc_librarymonitor.cpp"
