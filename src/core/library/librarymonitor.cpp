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

#include <utils/fileutils.h>

#include <QDir>

#include <ranges>

namespace Fooyin {
LibraryMonitor::LibraryMonitor(QObject* parent)
    : QObject{parent}
{ }

void LibraryMonitor::addWatcher(const LibraryInfo& library)
{
    auto watchPaths = [this, library](const QString& path) {
        QStringList dirs = Utils::File::getAllSubdirectories(QDir{path});
        dirs.append(path);
        m_watchers[library.id].addPaths(dirs);
    };

    watchPaths(library.path);

    QObject::connect(&m_watchers.at(library.id), &LibraryWatcher::libraryDirsChanged, this,
                     [this, watchPaths, library](const QStringList& dirs) {
                         std::ranges::for_each(dirs, watchPaths);
                         emit directoriesChanged(library, dirs);
                     });
}

void LibraryMonitor::setupWatchers(const LibraryInfoMap& libraries, bool enabled)
{
    for(const auto& library : libraries | std::views::values) {
        if(!enabled) {
            if(library.status == LibraryInfo::Status::Monitoring) {
                LibraryInfo updatedLibrary{library};
                updatedLibrary.status = LibraryInfo::Status::Idle;
                emit statusChanged(updatedLibrary);
            }
        }
        else if(!m_watchers.contains(library.id)) {
            addWatcher(library);
            LibraryInfo updatedLibrary{library};
            updatedLibrary.status = LibraryInfo::Status::Monitoring;
            emit statusChanged(updatedLibrary);
        }
    }

    if(!enabled) {
        m_watchers.clear();
    }
}
} // namespace Fooyin

#include "moc_librarymonitor.cpp"
