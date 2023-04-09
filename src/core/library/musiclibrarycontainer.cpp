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

#include "musiclibrarycontainer.h"
#include "libraryinfo.h"
#include "libraryinteractor.h"

#include <utils/helpers.h>

namespace Fy::Core::Library {
MusicLibraryContainer::MusicLibraryContainer(MusicLibraryInternal* library, QObject* parent)
    : MusicLibrary{parent}
    , m_currentLibrary{library}
{ }

void MusicLibraryContainer::loadLibrary()
{
    m_currentLibrary->loadLibrary();
}

LibraryInfo* MusicLibraryContainer::info() const
{
    return m_currentLibrary->info();
}

void MusicLibraryContainer::reload()
{
    m_currentLibrary->reload();
}

void MusicLibraryContainer::rescan()
{
    m_currentLibrary->rescan();
}

void MusicLibraryContainer::changeLibrary(MusicLibraryInternal* library)
{
    m_currentLibrary = library;
    emit libraryChanged();
}

Track MusicLibraryContainer::track(int id) const
{
    return m_currentLibrary->track(id);
}

TrackList MusicLibraryContainer::tracks() const
{
    TrackList intersectedTracks;

    for(const auto& interactor : m_interactors) {
        if(interactor->hasTracks()) {
            auto interactorTracks = interactor->tracks();
            if(intersectedTracks.empty()) {
                intersectedTracks.insert(intersectedTracks.end(), interactorTracks.cbegin(), interactorTracks.cend());
            }
            else {
                intersectedTracks = Utils::intersection<Track, Track::TrackHash>(interactorTracks, intersectedTracks);
            }
        }
    }
    return !intersectedTracks.empty() ? intersectedTracks : m_currentLibrary->tracks();
}

TrackList MusicLibraryContainer::allTracks() const
{
    return m_currentLibrary->tracks();
}

SortOrder MusicLibraryContainer::sortOrder() const
{
    return m_currentLibrary->sortOrder();
}

void MusicLibraryContainer::sortTracks(SortOrder order)
{
    m_currentLibrary->sortTracks(order);
}

void MusicLibraryContainer::addInteractor(LibraryInteractor* interactor)
{
    m_interactors.emplace_back(interactor);
}

void MusicLibraryContainer::removeLibrary(int id)
{
    const int currentId = m_currentLibrary->info()->id;
    if(currentId == -2 || currentId == id) {
        emit libraryRemoved();
    }
}
} // namespace Fy::Core::Library
