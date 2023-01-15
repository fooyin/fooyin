/*
 * Fooyin
 * Copyright 2022, Luke Taylor <LukeT1@proton.me>
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
bool checkNewPath(const QString& path, const QMap<int, LibraryInfo>& libraries, qint8 libraryId = -1)
{
    if(path.isEmpty()) {
        return false;
    }

    return std::all_of(libraries.constBegin(), libraries.constEnd(), [libraryId, path](const LibraryInfo& info) {
        return (info.id() != libraryId && !Utils::File::isSamePath(info.path(), path)
                && !Utils::File::isSubdir(path, info.path()));
    });
}

LibraryManager::LibraryManager(QObject* parent)
    : QObject(parent)
    , m_database(DB::Database::instance())
    , m_libraryConnector(m_database->libraryConnector())
{
    reset();
}

LibraryManager::~LibraryManager() = default;

void LibraryManager::reset()
{
    m_libraries = m_libraryConnector->getAllLibraries();
}

QMap<int, LibraryInfo> LibraryManager::allLibraries() const
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

    m_libraries.insert(id, LibraryInfo(path, name, id));

    const bool success = m_libraryConnector->insertLibrary(id, path, name);

    if(success) {
        emit libraryAdded(libraryInfo(id));
        return id;
    }

    return -1;
}

void LibraryManager::removeLibrary(int id)
{
    if(!m_libraries.contains(id)) {
        return;
    }

    m_database->deleteLibraryDatabase(id);
    m_libraryConnector->removeLibrary(id);
    m_libraries.remove(id);
    emit libraryRemoved();
}

bool LibraryManager::hasLibrary() const
{
    return !m_libraries.isEmpty();
}

LibraryInfo LibraryManager::libraryInfo(int id) const
{
    return m_libraries.value(id);
}
} // namespace Core::Library
