/*
 * Fooyin
 * Copyright © 2022, Luke Taylor <LukeT1@proton.me>
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

#include "database/librarydatabase.h"
#include "database/trackdatabase.h"

#include <utils/database/dbconnectionhandler.h>
#include <utils/database/dbconnectionprovider.h>
#include <utils/fileutils.h>
#include <utils/helpers.h>

#include <utility>

using namespace Qt::StringLiterals;

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

class LibraryManagerPrivate
{
public:
    explicit LibraryManagerPrivate(DbConnectionPoolPtr dbPool, SettingsManager* settings)
        : m_dbPool{std::move(dbPool)}
        , m_settings{settings}
    {
        const DbConnectionProvider dbProvider{m_dbPool};

        m_libraryConnector.initialise(dbProvider);
        m_trackConnector.initialise(dbProvider);
    }

    [[nodiscard]] QString findUniqueName(const QString& name) const
    {
        const QString uniqueName{name.isEmpty() ? u"New Library"_s : name};
        return Utils::findUniqueString(uniqueName, m_libraries, [](const auto& item) { return item.second.name; });
    }

    DbConnectionPoolPtr m_dbPool;
    SettingsManager* m_settings;

    LibraryDatabase m_libraryConnector;
    TrackDatabase m_trackConnector;
    LibraryInfoMap m_libraries;
};

LibraryManager::LibraryManager(DbConnectionPoolPtr dbPool, SettingsManager* settings, QObject* parent)
    : QObject{parent}
    , p{std::make_unique<LibraryManagerPrivate>(std::move(dbPool), settings)}
{
    reset();
}

LibraryManager::~LibraryManager() = default;

void LibraryManager::reset()
{
    p->m_libraries.clear();
    p->m_libraryConnector.getAllLibraries(p->m_libraries);
}

const LibraryInfoMap& LibraryManager::allLibraries() const
{
    return p->m_libraries;
}

int LibraryManager::addLibrary(const QString& path, const QString& name)
{
    if(!checkNewPath(path, p->m_libraries)) {
        return -1;
    }

    const QString libraryName = p->findUniqueName(name);

    const auto id = p->m_libraryConnector.insertLibrary(path, libraryName);

    if(id > 0) {
        LibraryInfo& info = p->m_libraries.emplace(id, LibraryInfo{libraryName, path, id}).first->second;
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

    const auto library = p->m_libraries.at(id);
    emit libraryAboutToBeRemoved(library);

    if(p->m_libraryConnector.removeLibrary(id)) {
        eraseLibrary(p->m_libraries, id);

        const auto tracksRemoved = p->m_trackConnector.deleteLibraryTracks(id);
        emit libraryRemoved(library, tracksRemoved);

        return true;
    }

    return false;
}

bool LibraryManager::renameLibrary(int id, const QString& name)
{
    if(!hasLibrary(id)) {
        return false;
    }

    const QString libraryName = p->findUniqueName(name);

    if(p->m_libraryConnector.renameLibrary(id, libraryName)) {
        p->m_libraries.at(id).name = libraryName;
        emit libraryRenamed(p->m_libraries.at(id));
        return true;
    }

    return false;
}

void LibraryManager::updateLibraryStatus(const LibraryInfo& library)
{
    if(!hasLibrary(library.id)) {
        return;
    }
    p->m_libraries.at(library.id).status = library.status;
    emit libraryStatusChanged(library);
}

bool LibraryManager::hasLibrary() const
{
    return !p->m_libraries.empty();
}

bool LibraryManager::hasLibrary(int id) const
{
    return p->m_libraries.contains(id);
}

std::optional<LibraryInfo> LibraryManager::findLibraryByPath(const QString& path) const
{
    auto it = std::ranges::find_if(std::as_const(p->m_libraries),
                                   [path](const auto& info) { return info.second.path == path; });
    if(it != p->m_libraries.end()) {
        return it->second;
    }
    return {};
}

std::optional<LibraryInfo> LibraryManager::libraryInfo(int id) const
{
    if(hasLibrary(id)) {
        return p->m_libraries.at(id);
    }
    return {};
}
} // namespace Fooyin

#include "moc_librarymanager.cpp"
