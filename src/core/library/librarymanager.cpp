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
#include "musiclibrarycontainer.h"
#include "singlemusiclibrary.h"
#include "unifiedtrackstore.h"

#include <utils/threadmanager.h>
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

LibraryManager::LibraryManager(Utils::ThreadManager* threadManager, DB::Database* database,
                               Utils::SettingsManager* settings, QObject* parent)
    : QObject{parent}
    , m_threadManager{threadManager}
    , m_database{database}
    , m_settings{settings}
    , m_libraryConnector{m_database->libraryConnector()}
{
    reset();
}

LibraryManager::~LibraryManager()
{
    m_libraryInfos.clear();
}

void LibraryManager::reset()
{
    m_libraryInfos.clear();

    auto* unifiedInfo = m_libraryInfos.emplace_back(std::make_unique<LibraryInfo>("Unified", "", -2)).get();
    m_unifiedLibrary  = new UnifiedMusicLibrary(unifiedInfo, this, this);
    m_unifiedStore    = m_unifiedLibrary->trackStore();
    m_libraryHandler  = new MusicLibraryContainer(m_unifiedLibrary, this);

    connect(this, &Library::LibraryManager::libraryAdded, m_unifiedLibrary, &Library::MusicLibraryInternal::reload);

    if(m_libraryConnector->getAllLibraries(m_libraryInfos)) {
        for(const auto& library : m_libraryInfos) {
            addNewLibrary(library.get());
        }
    }
}

MusicLibrary* LibraryManager::currentLibrary() const
{
    return m_libraryHandler;
}

const LibraryInfoList& LibraryManager::allLibrariesInfo() const
{
    return m_libraryInfos;
}

const LibraryIdMap& LibraryManager::allLibraries() const
{
    return m_libraries;
}

int LibraryManager::addLibrary(const QString& path, const QString& name)
{
    if(!checkNewPath(path, m_libraryInfos)) {
        return -1;
    }

    QString libraryName = name;

    if(libraryName.isEmpty()) {
        libraryName = QString{"Library %1"}.arg(m_libraryInfos.size());
    }

    const auto id = m_libraryConnector->insertLibrary(path, name);

    if(id >= 0) {
        auto* info = m_libraryInfos.emplace_back(std::make_unique<LibraryInfo>(name, path, id)).get();
        addNewLibrary(info);
        emit libraryAdded(info);
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
        eraseLibrary(m_libraryInfos, id);
        m_unifiedStore->removeLibrary(id);
        m_libraryHandler->removeLibrary(id);
        m_libraries.erase(id);
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
        m_libraryInfos.at(id)->name = name;
        emit libraryRenamed(id, name);
        return true;
    }
    return false;
}

void LibraryManager::changeCurrentLibrary(int id)
{
    if(id == -2) {
        m_libraryHandler->changeLibrary(m_unifiedLibrary);
    }
    else if(m_libraries.count(id)) {
        m_libraryHandler->changeLibrary(m_libraries.at(id));
    }
}

bool LibraryManager::hasLibrary() const
{
    return !m_libraries.empty();
}

bool LibraryManager::hasLibrary(int id) const
{
    return std::any_of(m_libraryInfos.cbegin(), m_libraryInfos.cend(), [id](const auto& info) {
        return (info->id == id);
    });
}

LibraryInfo* LibraryManager::findLibraryByPath(const QString& path) const
{
    auto it = std::find_if(m_libraryInfos.cbegin(), m_libraryInfos.cend(), [path](const auto& info) {
        return info->path == path;
    });
    return it == m_libraryInfos.end() ? nullptr : it->get();
}

LibraryInfo* LibraryManager::libraryInfo(int id) const
{
    if(!hasLibrary(id)) {
        return {};
    }
    auto it = std::find_if(m_libraryInfos.cbegin(), m_libraryInfos.cend(), [id](const auto& info) {
        return info->id == id;
    });
    return it->get();
}

MusicLibraryInternal* LibraryManager::addNewLibrary(LibraryInfo* info)
{
    if(!(info->id >= 0)) {
        return {};
    }

    auto* library
        = m_libraries.emplace(info->id, new SingleMusicLibrary(info, m_database, m_threadManager, m_settings, this))
              .first->second;

    m_unifiedStore->addLibrary(info->id, library->trackStore());

    // Forward individual library signals
    connect(library, &Library::MusicLibraryInternal::tracksLoaded, m_libraryHandler,
            &Library::MusicLibrary::tracksLoaded);
    connect(library, &Library::MusicLibraryInternal::tracksAdded, m_libraryHandler,
            &Library::MusicLibrary::tracksAdded);
    connect(library, &Library::MusicLibraryInternal::tracksChanged, m_libraryHandler,
            &Library::MusicLibrary::tracksChanged);
    connect(library, &Library::MusicLibraryInternal::tracksUpdated, m_libraryHandler,
            &Library::MusicLibrary::tracksUpdated);
    connect(library, &Library::MusicLibraryInternal::tracksDeleted, m_libraryHandler,
            &Library::MusicLibrary::tracksDeleted);

    return library;
}
} // namespace Fy::Core::Library
