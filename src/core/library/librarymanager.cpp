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

#include <core/library/librarymanager.h>

#include "database/database.h"
#include "database/librarydatabase.h"

#include <core/library/libraryinfo.h>
#include <utils/utils.h>

namespace Fooyin {
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

struct LibraryManager::Private
{
    Database* database;
    SettingsManager* settings;
    LibraryDatabase libraryConnector;
    LibraryInfoMap libraries;

    explicit Private(Database* database, SettingsManager* settings)
        : database{database}
        , settings{settings}
        , libraryConnector{database->connectionName()}
    { }
};

LibraryManager::LibraryManager(Database* database, SettingsManager* settings, QObject* parent)
    : QObject{parent}
    , p{std::make_unique<Private>(database, settings)}
{
    reset();
}

LibraryManager::~LibraryManager() = default;

void LibraryManager::reset()
{
    p->libraries.clear();
    p->libraryConnector.getAllLibraries(p->libraries);
}

const LibraryInfoMap& LibraryManager::allLibraries() const
{
    return p->libraries;
}

int LibraryManager::addLibrary(const QString& path, const QString& name)
{
    if(!checkNewPath(path, p->libraries)) {
        return -1;
    }

    QString libraryName = name;

    if(libraryName.isEmpty()) {
        libraryName = "Library " + QString::number(p->libraries.size());
    }

    const auto id = p->libraryConnector.insertLibrary(path, name);

    if(id >= 0) {
        LibraryInfo& info = p->libraries.emplace(id, LibraryInfo{libraryName, path, id}).first->second;
        emit libraryAdded(info);
        info.status = LibraryInfo::Status::Initialised;
        return id;
    }
    return -1;
}

bool LibraryManager::removeLibrary(int id)
{
    if(!hasLibrary(id)) {
        return false;
    }

    if(p->libraryConnector.removeLibrary(id)) {
        eraseLibrary(p->libraries, id);
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

    if(p->libraryConnector.renameLibrary(id, name)) {
        p->libraries.at(id).name = name;
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
    p->libraries.at(library.id).status = library.status;
    emit libraryStatusChanged(library);
}

bool LibraryManager::hasLibrary() const
{
    return p->libraries.size() > 1;
}

bool LibraryManager::hasLibrary(int id) const
{
    return p->libraries.contains(id);
}

std::optional<LibraryInfo> LibraryManager::findLibraryByPath(const QString& path) const
{
    auto it = std::ranges::find_if(std::as_const(p->libraries),
                                   [path](const auto& info) { return info.second.path == path; });
    if(it != p->libraries.end()) {
        return it->second;
    }
    return {};
}

std::optional<LibraryInfo> LibraryManager::libraryInfo(int id) const
{
    if(hasLibrary(id)) {
        return p->libraries.at(id);
    }
    return {};
}
} // namespace Fooyin

#include "core/library/moc_librarymanager.cpp"
