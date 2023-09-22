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

#include "librarymanager.h"

#include "core/database/database.h"
#include "core/database/library.h"
#include "libraryinfo.h"

#include <utils/utils.h>

namespace Fy::Core::Library {
bool checkNewPath(const QString& path, const LibraryInfoMap& libraries, int libraryId = -1)
{
    if(path.isEmpty()) {
        return false;
    }

    return std::ranges::all_of(std::as_const(libraries), [libraryId, path](const auto& info) {
        return (info.second.id != libraryId && !Utils::File::isSamePath(info.second.path, path)
                && !Utils::File::isSubdir(path, info.second.path));
    });
}

void eraseLibrary(LibraryInfoMap& libraries, int id)
{
    if(libraries.contains(id)) {
        libraries.erase(id);
    }
}

LibraryManager::LibraryManager(DB::Database* database, Utils::SettingsManager* settings, QObject* parent)
    : QObject{parent}
    , m_database{database}
    , m_settings{settings}
    , m_libraryConnector{m_database->libraryConnector()}
{
    reset();
}

void LibraryManager::reset()
{
    m_libraries.clear();
    m_libraries.emplace(-2, LibraryInfo{"Unified", "", -2});
    m_libraryConnector->getAllLibraries(m_libraries);
}

const LibraryInfoMap& LibraryManager::allLibraries() const
{
    return m_libraries;
}

int LibraryManager::addLibrary(const QString& path, const QString& name)
{
    if(!checkNewPath(path, m_libraries)) {
        return -1;
    }

    QString libraryName = name;

    if(libraryName.isEmpty()) {
        libraryName = QString{"Library %1"}.arg(m_libraries.size());
    }

    const auto id = m_libraryConnector->insertLibrary(path, name);

    if(id >= 0) {
        LibraryInfo& info = m_libraries.emplace(id, LibraryInfo{libraryName, path, id}).first->second;
        emit libraryAdded(info);
        info.status = Initialised;
        return id;
    }
    return -1;
}

bool LibraryManager::removeLibrary(int id)
{
    if(!hasLibrary(id)) {
        return false;
    }

    if(m_libraryConnector->removeLibrary(id)) {
        eraseLibrary(m_libraries, id);
        emit libraryRemoved(id);
        return true;
    }
    return false;
}

bool LibraryManager::renameLibrary(int id, const QString& name)
{
    if(!hasLibrary(id)) {
        return false;
    }

    if(m_libraryConnector->renameLibrary(id, name)) {
        m_libraries.at(id).name = name;
        emit libraryRenamed(id, name);
        return true;
    }
    return false;
}

void LibraryManager::updateLibraryStatus(const LibraryInfo& library)
{
    if(!hasLibrary(library.id)) {
        return;
    }
    m_libraries.at(library.id).status = library.status;
    emit libraryStatusChanged(library);
}

bool LibraryManager::hasLibrary() const
{
    return m_libraries.size() > 1;
}

bool LibraryManager::hasLibrary(int id) const
{
    return m_libraries.contains(id);
}

std::optional<LibraryInfo> LibraryManager::findLibraryByPath(const QString& path) const
{
    auto it = std::ranges::find_if(std::as_const(m_libraries), [path](const auto& info) {
        return info.second.path == path;
    });
    if(it != m_libraries.end()) {
        return it->second;
    }
    return {};
}

std::optional<LibraryInfo> LibraryManager::libraryInfo(int id) const
{
    if(hasLibrary(id)) {
        return m_libraries.at(id);
    }
    return {};
}
} // namespace Fy::Core::Library
