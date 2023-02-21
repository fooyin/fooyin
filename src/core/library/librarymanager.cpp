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
bool checkNewPath(const QString& path, const LibraryList& libraries, int libraryId = -1)
{
    if(path.isEmpty()) {
        return false;
    }

    return std::all_of(libraries.cbegin(), libraries.cend(), [libraryId, path](const auto& info) {
        return (info->id != libraryId && !Utils::File::isSamePath(info->path, path)
                && !Utils::File::isSubdir(path, info->path));
    });
}

void eraseLibrary(LibraryList& libraries, int id)
{
    auto it = std::find_if(libraries.begin(), libraries.end(), [id](const auto& info) {
        return info->id == id;
    });
    if(it != libraries.end()) {
        libraries.erase(it);
    }
}

LibraryManager::LibraryManager(DB::Database* database, QObject* parent)
    : QObject{parent}
    , m_database{database}
    , m_libraryConnector{m_database->libraryConnector()}
{
    reset();
}

LibraryManager::~LibraryManager()
{
    m_libraries.clear();
}

void LibraryManager::reset()
{
    m_libraries = m_libraryConnector->getAllLibraries();
}

const LibraryList& LibraryManager::allLibraries() const
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

    const auto id = static_cast<int>(m_libraries.size());

    m_libraries.emplace_back(std::make_unique<LibraryInfo>(name, path, id));

    const bool success = m_libraryConnector->insertLibrary(id, path, name);

    if(success) {
        emit libraryAdded(libraryInfo(id));
        return id;
    }

    return -1;
}

bool LibraryManager::removeLibrary(int id)
{
    if(!hasLibrary(id)) {
        return false;
    }

    m_database->deleteLibraryDatabase(id);
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
        m_libraries.at(id)->name = name;
        emit libraryRenamed(id, name);
        return true;
    }
    return false;
}

bool LibraryManager::hasLibrary() const
{
    return !m_libraries.empty();
}

bool LibraryManager::hasLibrary(int id) const
{
    return std::any_of(m_libraries.cbegin(), m_libraries.cend(), [id](const auto& info) {
        return (info->id == id);
    });
}

LibraryInfo* LibraryManager::findLibraryByPath(const QString& path) const
{
    auto it = std::find_if(m_libraries.cbegin(), m_libraries.cend(), [path](const auto& info) {
        return info->path == path;
    });
    return it == m_libraries.end() ? nullptr : it->get();
}

LibraryInfo* LibraryManager::libraryInfo(int id) const
{
    if(!hasLibrary(id)) {
        return {};
    }
    auto it = std::find_if(m_libraries.cbegin(), m_libraries.cend(), [id](const auto& info) {
        return info->id == id;
    });
    return it->get();
}
} // namespace Core::Library
