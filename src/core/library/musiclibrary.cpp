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

#include "core/coresettings.h"
#include "core/playlist/libraryplaylistinterface.h"
#include "librarydatabasemanager.h"
#include "libraryinfo.h"
#include "librarymanager.h"
#include "libraryscanner.h"
#include "musiclibraryinteractor.h"
#include "trackstore.h"

#include <utils/helpers.h>
#include <utils/settings/settingsmanager.h>
#include <utils/threadmanager.h>

#include <QTimer>
#include <utility>

namespace Core::Library {
struct MusicLibrary::Private
{
    Playlist::LibraryPlaylistInterface* playlistInteractor;
    LibraryManager* libraryManager;
    Utils::ThreadManager* threadManager;
    DB::Database* database;
    Utils::SettingsManager* settings;
    LibraryScanner scanner;
    LibraryDatabaseManager libraryDatabaseManager;

    TrackStore trackStore;

    TrackPtrList selectedTracks;
    std::vector<MusicLibraryInteractor*> interactors;

    SortOrder order{Library::SortOrder::YearDesc};

    Private(Playlist::LibraryPlaylistInterface* playlistInteractor, LibraryManager* libraryManager,
            Utils::ThreadManager* threadManager, DB::Database* database, Utils::SettingsManager* settings)
        : playlistInteractor{playlistInteractor}
        , libraryManager{libraryManager}
        , threadManager{threadManager}
        , database{database}
        , settings{settings}
        , scanner{libraryManager, database}
        , libraryDatabaseManager{database, settings}
    { }
};

MusicLibrary::MusicLibrary(Playlist::LibraryPlaylistInterface* playlistInteractor, LibraryManager* libraryManager,
                           Utils::ThreadManager* threadManager, DB::Database* database,
                           Utils::SettingsManager* settings, QObject* parent)
    : QObject{parent}
    , p{std::make_unique<Private>(playlistInteractor, libraryManager, threadManager, database, settings)}
{
    p->threadManager->moveToNewThread(&p->scanner);
    p->threadManager->moveToNewThread(&p->libraryDatabaseManager);

    connect(p->libraryManager, &Library::LibraryManager::libraryRemoved, &p->scanner, &LibraryScanner::stopThread);
    connect(p->libraryManager, &Library::LibraryManager::libraryRemoved, this, &MusicLibrary::libraryRemoved);

    connect(this, &MusicLibrary::runLibraryScan, &p->scanner, &LibraryScanner::scanLibrary);
    connect(this, &MusicLibrary::runAllLibrariesScan, &p->scanner, &LibraryScanner::scanAll);
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
    //    p->trackStore.clear();
    refreshTracks(tracks);
}

void MusicLibrary::addNewTracks(const TrackList& tracks)
{
    p->trackStore.add(tracks);
    p->trackStore.sort(p->order);
    emit tracksAdded();
}

void MusicLibrary::updateChangedTracks(const TrackList& tracks)
{
    p->trackStore.update(tracks);
    emit tracksUpdated();
}

void MusicLibrary::removeDeletedTracks(const IdSet& tracks)
{
    p->trackStore.markForDelete(tracks);
    emit tracksDeleted();
    p->trackStore.remove(tracks);
}

void MusicLibrary::reloadAll()
{
    emit runAllLibrariesScan(p->trackStore.tracks());
}

void MusicLibrary::reload(Library::LibraryInfo* info)
{
    emit runLibraryScan(p->trackStore.tracks(), info);
}

void MusicLibrary::refresh()
{
    getAllTracks();
}

void MusicLibrary::refreshTracks(const TrackList& result)
{
    //    p->trackStore.clear();

    p->trackStore.add(result);
    //    p->trackStore.sort(p->order);

    emit tracksLoaded(p->trackStore.tracks());
}

Track* MusicLibrary::track(int id) const
{
    return p->trackStore.track(id);
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
    return haveTracks ? lst : p->trackStore.tracks();
}

TrackPtrList MusicLibrary::allTracks() const
{
    return p->trackStore.tracks();
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
    p->trackStore.sort(p->order);
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
    emit loadAllTracks(p->order);
}
} // namespace Core::Library
