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

#include <ranges>
#include <set>
#include <vector>

namespace Fooyin {
LibraryMonitor::LibraryMonitor(QObject* parent)
    : QObject{parent}
{ }

void LibraryMonitor::addWatcher(const LibraryInfo& library, const TrackList& tracks, bool monitorTrackFiles)
{
    const auto watchPaths = [this, library](const QString& path) {
        QStringList dirs = Utils::File::getAllSubdirectories(QDir{path});
        dirs.append(path);
        m_watchers[library.id].addPaths(dirs);
    };

    watchPaths(library.path);

    auto& watcher = m_watchers[library.id];

    QObject::connect(&watcher, &LibraryWatcher::libraryDirsChanged, this,
                     [this, watchPaths, library](const QStringList& dirs) {
                         std::ranges::for_each(dirs, watchPaths);
                         Q_EMIT directoriesChanged(library, dirs);
                     });
    QObject::connect(&watcher, &LibraryWatcher::libraryTrackFilesChanged, this,
                     [this, library](const QStringList& files) { Q_EMIT trackFilesChanged(library, files); });

    if(!monitorTrackFiles) {
        return;
    }

    std::set<QString> files;
    for(const Track& track : tracks) {
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
        watchFiles.push_back(file);
    }
    watcher.addPaths(watchFiles);
}

void LibraryMonitor::setupWatchers(const LibraryInfoMap& libraries, const TrackList& tracks, bool monitorDirectories,
                                   bool monitorTrackFiles)
{
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

            addWatcher(library, tracks, monitorTrackFiles);
            LibraryInfo updatedLibrary{library};
            updatedLibrary.status = LibraryInfo::Status::Monitoring;
            Q_EMIT statusChanged(updatedLibrary);
        }
    }

    if(!monitorDirectories) {
        m_watchers.clear();
    }
}
} // namespace Fooyin

#include "moc_librarymonitor.cpp"
