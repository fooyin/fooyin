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

namespace Core::Library {
bool checkNewPath(const QString& path, const IdLibraryMap& libraries, int libraryId = -1)
{
    if(path.isEmpty()) {
        return false;
    }

    return std::all_of(libraries.cbegin(), libraries.cend(), [libraryId, path](const auto& info) {
        return (info.first != libraryId && !Utils::File::isSamePath(info.second.path, path)
                && !Utils::File::isSubdir(path, info.second.path));
    });
}

LibraryManager::LibraryManager(DB::Database* database, QObject* parent)
    : QObject{parent}
    , m_database{database}
    , m_libraryConnector{m_database->libraryConnector()}
{
    reset();
}

void LibraryManager::reset()
{
    m_libraries = m_libraryConnector->getAllLibraries();
}

IdLibraryMap LibraryManager::allLibraries() const
{
    return m_libraries;
}

int LibraryManager::addLibrary(const QString& path, QString& name)
{
    if(!checkNewPath(path, m_libraries)) {
        return -1;
    }

    if(name.isEmpty()) {
        name = QString("Library %1").arg(m_libraries.size());
    }

    const auto id = static_cast<int>(m_libraries.size());

    m_libraries.emplace(id, LibraryInfo{name, path, id});

    const bool success = m_libraryConnector->insertLibrary(id, path, name);

    if(success) {
        emit libraryAdded(libraryInfo(id));
        return id;
    }

    return -1;
}

bool LibraryManager::removeLibrary(int id)
{
    if(!m_libraries.count(id)) {
        return false;
    }

    m_database->deleteLibraryDatabase(id);
    if(m_libraryConnector->removeLibrary(id)) {
        m_libraries.erase(id);
        emit libraryRemoved(id);
        return true;
    }
    return false;
}

bool LibraryManager::renameLibrary(int id, const QString& name)
{
    if(!m_libraries.count(id)) {
        return false;
    }

    if(m_libraryConnector->renameLibrary(id, name)) {
        m_libraries.at(id);
        emit libraryRenamed(id, name);
        return true;
    }
    return false;
}

bool LibraryManager::hasLibrary() const
{
    return !m_libraries.empty();
}

bool LibraryManager::findLibraryByPath(const QString& path, LibraryInfo& info) const
{
    auto it = std::find_if(m_libraries.begin(), m_libraries.end(), [path](const auto& info) {
        return info.second.path == path;
    });
    info    = it == m_libraries.end() ? LibraryInfo{} : it->second;
    return info.id != -1;
}

LibraryInfo LibraryManager::libraryInfo(int id) const
{
    if(!m_libraries.count(id)) {
        return {};
    }
    return m_libraries.at(id);
}
} // namespace Core::Library
