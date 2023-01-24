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

#include "musiclibrary.h"

#include "core/app/threadmanager.h"
#include "core/coresettings.h"
#include "core/library/sorting/sorting.h"
#include "core/playlist/libraryplaylistinterface.h"
#include "librarydatabasemanager.h"
#include "libraryinfo.h"
#include "librarymanager.h"
#include "libraryscanner.h"
#include "musiclibraryinteractor.h"

#include <utils/helpers.h>

#include <QTimer>
#include <utility>

namespace Core::Library {
struct MusicLibrary::Private
{
    Playlist::LibraryPlaylistInterface* playlistInteractor;
    LibraryManager* libraryManager;
    ThreadManager* threadManager;
    SettingsManager* settings;
    LibraryScanner scanner;
    LibraryDatabaseManager libraryDatabaseManager;

    TrackList trackStore;
    TrackPtrList tracks;
    TrackHash trackMap;

    TrackPtrList selectedTracks;
    std::vector<MusicLibraryInteractor*> interactors;

    SortOrder order{Library::SortOrder::YearDesc};

    Private(Playlist::LibraryPlaylistInterface* playlistInteractor, LibraryManager* libraryManager,
            ThreadManager* threadManager, SettingsManager* settings)
        : playlistInteractor{playlistInteractor}
        , libraryManager{libraryManager}
        , threadManager{threadManager}
        , settings{settings}
        , scanner{libraryManager}
    { }
};

MusicLibrary::MusicLibrary(Playlist::LibraryPlaylistInterface* playlistInteractor, LibraryManager* libraryManager,
                           ThreadManager* threadManager, SettingsManager* settings, QObject* parent)
    : QObject{parent}
    , p{std::make_unique<Private>(playlistInteractor, libraryManager, threadManager, settings)}
{
    p->threadManager->moveToNewThread(&p->scanner);
    p->threadManager->moveToNewThread(&p->libraryDatabaseManager);

    connect(p->libraryManager, &Library::LibraryManager::libraryRemoved, &p->scanner, &LibraryScanner::stopThread);
    connect(p->libraryManager, &Library::LibraryManager::libraryRemoved, this, &MusicLibrary::libraryRemoved);

    connect(this, &MusicLibrary::runLibraryScan, &p->scanner, &LibraryScanner::scanLibrary);
    connect(this, &MusicLibrary::runAllLibrariesScan, &p->scanner, &LibraryScanner::scanAll);
    connect(&p->scanner, &LibraryScanner::libraryAdded, this, &MusicLibrary::libraryAdded);
    connect(&p->scanner, &LibraryScanner::addedTracks, this, &MusicLibrary::addNewTracks);
    connect(&p->scanner, &LibraryScanner::updatedTracks, this, &MusicLibrary::updateChangedTracks);
    connect(&p->scanner, &LibraryScanner::tracksDeleted, this, &MusicLibrary::removeDeletedTracks);

    connect(&p->libraryDatabaseManager, &LibraryDatabaseManager::gotTracks, this, &MusicLibrary::loadTracks);
    connect(this, &MusicLibrary::loadAllTracks, &p->libraryDatabaseManager, &LibraryDatabaseManager::getAllTracks);
    connect(this, &MusicLibrary::updateSaveTracks, &p->libraryDatabaseManager, &LibraryDatabaseManager::updateTracks);
}

MusicLibrary::~MusicLibrary() = default;

void MusicLibrary::load()
{
    getAllTracks();

    if(p->settings->value<Settings::AutoRefresh>()) {
        QTimer::singleShot(3000, this, &Library::MusicLibrary::reloadAll);
    }
}

void MusicLibrary::libraryAdded()
{
    load();
}

void MusicLibrary::prepareTracks(int idx)
{
    if(p->selectedTracks.empty()) {
        p->playlistInteractor->createPlaylist(tracks(), 0);
    }

    else {
        p->playlistInteractor->createPlaylist(tracks(), idx);
    }
}

TrackPtrList MusicLibrary::selectedTracks() const
{
    return p->selectedTracks;
}

void MusicLibrary::updateTracks(const TrackPtrList& tracks)
{
    emit updateSaveTracks(tracks);
}

void MusicLibrary::addInteractor(MusicLibraryInteractor* interactor)
{
    p->interactors.emplace_back(interactor);
}

void MusicLibrary::loadTracks(const TrackList& tracks)
{
    p->trackStore.clear();
    p->tracks.clear();
    p->trackMap.clear();

    refreshTracks(tracks);
}

void MusicLibrary::addNewTracks(const TrackList& tracks)
{
    p->tracks.reserve(p->tracks.size() + tracks.size());
    p->trackMap.reserve(p->trackMap.size() + tracks.size());

    for(const auto& track : tracks) {
        auto& newTrack = p->trackStore.emplace_back(track);
        p->tracks.emplace_back(&newTrack);
        p->trackMap.emplace(newTrack.id(), &newTrack);
    }
    Library::sortTracks(p->tracks, p->order);
    emit tracksAdded();
}

void MusicLibrary::updateChangedTracks(const TrackList& tracks)
{
    for(const auto& track : tracks) {
        if(p->trackMap.count(track.id())) {
            Track* libraryTrack = p->trackMap.at(track.id());
            *libraryTrack       = track;
        }
    }
    emit tracksUpdated();
}

void MusicLibrary::removeDeletedTracks(const IdSet& tracks)
{
    for(auto trackId : tracks) {
        if(p->trackMap.count(trackId)) {
            {
                Track* libraryTrack = p->trackMap.at(trackId);
                libraryTrack->setIsEnabled(false);
            }
        }
    }
    emit tracksDeleted();
}

void MusicLibrary::reloadAll()
{
    emit runAllLibrariesScan(p->trackStore);
}

void MusicLibrary::reload(const Library::LibraryInfo& info)
{
    emit runLibraryScan(p->trackStore, info);
}

void MusicLibrary::refresh()
{
    getAllTracks();
}

void MusicLibrary::refreshTracks(const TrackList& result)
{
    p->trackStore.clear();
    p->tracks.clear();
    p->trackMap.clear();

    p->tracks.reserve(result.size());
    p->trackMap.reserve(result.size());

    for(const auto& track : result) {
        auto& newTrack = p->trackStore.emplace_back(track);
        p->tracks.emplace_back(&newTrack);
        p->trackMap.emplace(newTrack.id(), &newTrack);
    }
    Library::sortTracks(p->tracks, p->order);
    emit tracksLoaded(p->tracks);
}

Track* MusicLibrary::track(int id) const
{
    return p->trackMap.at(id);
}

TrackPtrList MusicLibrary::tracks(const std::vector<int>& ids) const
{
    TrackPtrList tracks;
    for(const auto& id : ids) {
        tracks.emplace_back(p->trackMap.at(id));
    }
    return tracks;
}

TrackPtrList MusicLibrary::tracks() const
{
    TrackPtrList lst;
    bool haveTracks{false};

    for(auto* inter : p->interactors) {
        if(inter->hasTracks()) {
            haveTracks = true;
            auto trks  = inter->tracks();
            lst.insert(lst.end(), trks.begin(), trks.end());
        }
    }
    return haveTracks ? lst : p->tracks;
}

TrackPtrList MusicLibrary::allTracks() const
{
    return p->tracks;
}

int MusicLibrary::trackCount() const
{
    return static_cast<int>(tracks().size());
}

Library::SortOrder MusicLibrary::sortOrder() const
{
    return p->order;
}

void MusicLibrary::changeOrder(SortOrder order)
{
    p->order = order;
    Library::sortTracks(p->tracks, p->order);
}

void MusicLibrary::changeTrackSelection(const TrackSet& tracks)
{
    TrackPtrList newSelectedTracks;
    for(const auto& track : tracks) {
        newSelectedTracks.emplace_back(track);
    }

    if(p->selectedTracks == newSelectedTracks) {
        return;
    }

    p->selectedTracks = std::move(newSelectedTracks);
}

void MusicLibrary::trackSelectionChanged(const TrackSet& tracks)
{
    if(tracks.empty()) {
        return;
    }

    changeTrackSelection(tracks);
    emit tracksSelChanged();
}

void MusicLibrary::getAllTracks()
{
    emit loadAllTracks();
}
} // namespace Core::Library
