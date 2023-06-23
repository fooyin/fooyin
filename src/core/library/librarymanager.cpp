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
bool checkNewPath(const QString& path, const LibraryInfoList& libraries, int libraryId = -1)
{
    if(path.isEmpty()) {
        return false;
    }

    return std::all_of(libraries.cbegin(), libraries.cend(), [libraryId, path](const auto& info) {
        return (info->id != libraryId && !Utils::File::isSamePath(info->path, path)
                && !Utils::File::isSubdir(path, info->path));
    });
}

void eraseLibrary(LibraryInfoList& libraries, int id)
{
    auto it = std::find_if(libraries.begin(), libraries.end(), [id](const auto& info) {
        return info->id == id;
    });
    if(it != libraries.end()) {
        libraries.erase(it);
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
    m_libraries.emplace_back(new LibraryInfo("Unified", "", -2));
    m_libraryConnector->getAllLibraries(m_libraries);
}

const LibraryInfoList& LibraryManager::allLibraries() const
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
        auto* info = m_libraries.emplace_back(std::make_unique<LibraryInfo>(libraryName, path, id)).get();
        emit libraryAdded(info);
        info->status = Initialised;
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
        auto it = std::ranges::find_if(std::as_const(m_libraries), [id](const auto& library) {
            return library->id == id;
        });
        if(it != m_libraries.cend()) {
            it->get()->name = name;

            emit libraryRenamed(id, name);
            return true;
        }
    }
    return false;
}

bool LibraryManager::hasLibrary() const
{
    return m_libraries.size() > 1;
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
} // namespace Fy::Core::Library
