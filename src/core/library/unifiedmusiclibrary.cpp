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

#include "unifiedmusiclibrary.h"
#include "librarymanager.h"
#include "unifiedtrackstore.h"

#include <utils/helpers.h>

namespace Fy::Core::Library {
UnifiedMusicLibrary::UnifiedMusicLibrary(LibraryInfo* info, LibraryManager* libraryManager, QObject* parent)
    : MusicLibraryInternal{parent}
    , m_info{info}
    , m_libraryManager{libraryManager}
    , m_trackStore{std::make_unique<UnifiedTrackStore>()}
{ }

void UnifiedMusicLibrary::loadLibrary()
{
    const LibraryIdMap& libraries = m_libraryManager->allLibraries();
    for(const auto& library : libraries) {
        library.second->loadLibrary();
    }
}

void UnifiedMusicLibrary::reload()
{
    const LibraryIdMap& libraries = m_libraryManager->allLibraries();
    for(const auto& library : libraries) {
        library.second->reload();
    }
}

void UnifiedMusicLibrary::rescan()
{
    const LibraryIdMap& libraries = m_libraryManager->allLibraries();
    for(const auto& library : libraries) {
        library.second->rescan();
    }
}

LibraryInfo* UnifiedMusicLibrary::info() const
{
    return m_info;
}

Track* UnifiedMusicLibrary::track(int id) const
{
    return m_trackStore->track(id);
}

TrackPtrList UnifiedMusicLibrary::tracks() const
{
    return m_trackStore->tracks();
}

UnifiedTrackStore* UnifiedMusicLibrary::trackStore() const
{
    return m_trackStore.get();
}

SortOrder UnifiedMusicLibrary::sortOrder() const
{
    return m_order;
}

void UnifiedMusicLibrary::sortTracks(SortOrder order)
{
    m_order = order;
    m_trackStore->sort(m_order);
}
} // namespace Fy::Core::Library
