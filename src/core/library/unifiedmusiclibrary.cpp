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

#include "unifiedmusiclibrary.h"

#include "core/coresettings.h"
#include "libraryinfo.h"
#include "librarymanager.h"
#include "libraryscanner.h"

#include <utils/helpers.h>
#include <utils/settings/settingsmanager.h>

#include <QThread>
#include <QTimer>

#include <ranges>
#include <vector>

namespace Fy::Core::Library {
UnifiedMusicLibrary::UnifiedMusicLibrary(LibraryManager* libraryManager, DB::Database* database,
                                         Utils::SettingsManager* settings, QObject* parent)
    : MusicLibrary{parent}
    , m_libraryManager{libraryManager}
    , m_database{database}
    , m_settings{settings}
    , m_threadHandler{database}
{
    connect(m_libraryManager, &LibraryManager::libraryAdded, this, &MusicLibrary::reload);
    connect(m_libraryManager, &LibraryManager::libraryAdded, this, &MusicLibrary::libraryAdded);
    connect(m_libraryManager, &LibraryManager::libraryRemoved, this, &MusicLibrary::removeLibrary);
    connect(this, &UnifiedMusicLibrary::libraryRemoved, &m_threadHandler, &LibraryThreadHandler::libraryRemoved);

    connect(this, &UnifiedMusicLibrary::runLibraryScan, &m_threadHandler, &LibraryThreadHandler::scanLibrary);
    connect(&m_threadHandler, &LibraryThreadHandler::progressChanged, this, &UnifiedMusicLibrary::scanProgress);
    connect(&m_threadHandler, &LibraryThreadHandler::statusChanged, this, &UnifiedMusicLibrary::libraryStatusChanged);
    connect(&m_threadHandler, &LibraryThreadHandler::statusChanged, m_libraryManager,
            &LibraryManager::libraryStatusChanged);
    connect(&m_threadHandler, &LibraryThreadHandler::addedTracks, this, &UnifiedMusicLibrary::addTracks);
    connect(&m_threadHandler, &LibraryThreadHandler::updatedTracks, this, &UnifiedMusicLibrary::updateTracks);
    connect(&m_threadHandler, &LibraryThreadHandler::tracksDeleted, this, &UnifiedMusicLibrary::removeTracks);

    connect(&m_threadHandler, &LibraryThreadHandler::gotTracks, this, &UnifiedMusicLibrary::loadTracks);
    connect(this, &UnifiedMusicLibrary::loadAllTracks, &m_threadHandler, &LibraryThreadHandler::getAllTracks);

    m_settings->subscribe<Settings::SortScript>(this, &UnifiedMusicLibrary::changeSort);
    m_trackSorter.changeSorting(m_settings->value<Settings::SortScript>());

    if(m_settings->value<Settings::AutoRefresh>()) {
        QTimer::singleShot(3000, this, &Library::UnifiedMusicLibrary::reloadAll);
    }
}

void UnifiedMusicLibrary::loadLibrary()
{
    getAllTracks();
}

void UnifiedMusicLibrary::reloadAll()
{
    const LibraryInfoList& libraries = m_libraryManager->allLibraries();
    for(const auto& library : libraries | std::views::filter([](const auto& lib) {
                                  return lib->id >= 0;
                              })) {
        reload(library.get());
    }
}

void UnifiedMusicLibrary::reload(LibraryInfo* library)
{
    emit runLibraryScan(*library, tracks());
}

void UnifiedMusicLibrary::rescan()
{
    getAllTracks();
}

bool UnifiedMusicLibrary::hasLibrary() const
{
    return m_libraryManager->hasLibrary();
}

TrackList UnifiedMusicLibrary::tracks() const
{
    return m_tracks;
}

void UnifiedMusicLibrary::changeSort(const QString& sort)
{
    m_trackSorter.changeSorting(sort);
    m_trackSorter.calcSortFields(m_tracks);
    m_trackSorter.sortTracks(m_tracks);
    emit tracksSorted(m_tracks);
}

void UnifiedMusicLibrary::removeLibrary(int id)
{
    auto filtered = m_tracks | std::views::filter([id](const Track& track) {
                        return track.libraryId() != id;
                    });
    std::ranges::copy(filtered, std::back_inserter(m_tracks));

    emit libraryRemoved(id);
}

void UnifiedMusicLibrary::getAllTracks()
{
    emit loadAllTracks();
}

void UnifiedMusicLibrary::loadTracks(const TrackList& tracks)
{
    addTracks(tracks);
    emit tracksLoaded(tracks);
}

void UnifiedMusicLibrary::addTracks(const TrackList& tracks)
{
    m_tracks.reserve(m_tracks.size() + tracks.size());

    TrackList newTracks{tracks};
    for(Track& track : newTracks) {
        m_trackSorter.calcSortField(track);
        m_tracks.push_back(track);
    }
    m_trackSorter.sortTracks(m_tracks);
    emit tracksAdded(newTracks);
}

void UnifiedMusicLibrary::updateTracks(const TrackList& tracks)
{
    std::ranges::for_each(tracks, [this](const auto& track) {
        std::ranges::replace_if(
            m_tracks,
            [track](const Track& libraryTrack) {
                return libraryTrack.id() == track.id();
            },
            track);
    });

    emit tracksUpdated(tracks);
}

void UnifiedMusicLibrary::removeTracks(const TrackList& tracks)
{
    auto [begin, end] = std::ranges::remove_if(m_tracks, [tracks](const Track& libraryTrack) {
        return std::ranges::any_of(tracks, [libraryTrack](const Track& track) {
            return libraryTrack.id() == track.id();
        });
    });
    m_tracks.erase(begin, end);

    emit tracksDeleted(tracks);
}

void UnifiedMusicLibrary::libraryStatusChanged(const LibraryInfo& library)
{
    if(LibraryInfo* info = m_libraryManager->libraryInfo(library.id)) {
        *info = library;
    }
}
} // namespace Fy::Core::Library