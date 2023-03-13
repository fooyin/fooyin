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

#include "singlemusiclibrary.h"

#include "core/coresettings.h"
#include "librarydatabasemanager.h"
#include "libraryinfo.h"
#include "libraryscanner.h"
#include "singletrackstore.h"

#include <utils/helpers.h>
#include <utils/settings/settingsmanager.h>
#include <utils/threadmanager.h>

#include <QTimer>

namespace Fy::Core::Library {
SingleMusicLibrary::SingleMusicLibrary(LibraryInfo* info, DB::Database* database, Utils::ThreadManager* threadManager,
                                       Utils::SettingsManager* settings, QObject* parent)
    : MusicLibraryInternal{parent}
    , m_info{info}
    , m_threadManager{threadManager}
    , m_database{database}
    , m_settings{settings}
    , m_scanner{info, database}
    , m_libraryDatabaseManager{info->id, database, settings}
    , m_trackStore{std::make_unique<SingleTrackStore>()}
    , m_order{SortOrder::NoSorting}
{
    m_threadManager->moveToNewThread(&m_scanner);
    m_threadManager->moveToNewThread(&m_libraryDatabaseManager);

    connect(this, &SingleMusicLibrary::libraryRemoved, &m_scanner, &LibraryScanner::stopThread);

    connect(this, &SingleMusicLibrary::runLibraryScan, &m_scanner, &LibraryScanner::scanLibrary);
    connect(&m_scanner, &LibraryScanner::addedTracks, this, &SingleMusicLibrary::addNewTracks);
    connect(&m_scanner, &LibraryScanner::updatedTracks, this, &SingleMusicLibrary::updateChangedTracks);
    connect(&m_scanner, &LibraryScanner::tracksDeleted, this, &SingleMusicLibrary::removeDeletedTracks);

    connect(&m_libraryDatabaseManager, &LibraryDatabaseManager::gotTracks, this, &SingleMusicLibrary::loadTracks);
    connect(this, &SingleMusicLibrary::loadAllTracks, &m_libraryDatabaseManager, &LibraryDatabaseManager::getAllTracks);
    connect(this, &SingleMusicLibrary::updateSaveTracks, &m_libraryDatabaseManager,
            &LibraryDatabaseManager::updateTracks);

    if(m_settings->value<Settings::AutoRefresh>()) {
        QTimer::singleShot(3000, this, &Library::SingleMusicLibrary::reload);
    }
}

void SingleMusicLibrary::loadLibrary()
{
    getAllTracks();
}

void SingleMusicLibrary::updateTracks(const TrackPtrList& tracks)
{
    emit updateSaveTracks(tracks);
}

void SingleMusicLibrary::loadTracks(const TrackList& tracks)
{
    //    m_trackStore.clear();
    refreshTracks(tracks);
}

void SingleMusicLibrary::addNewTracks(const TrackList& tracks)
{
    const TrackPtrList addedTracks = m_trackStore->add(tracks);
    //    m_trackStore->sort(m_order);
    emit tracksAdded(addedTracks);
}

void SingleMusicLibrary::updateChangedTracks(const TrackList& tracks)
{
    const TrackPtrList updatedTracks = m_trackStore->update(tracks);
    emit tracksUpdated(updatedTracks);
}

void SingleMusicLibrary::removeDeletedTracks(const TrackPtrList& tracks)
{
    m_trackStore->markForDelete(tracks);
    emit tracksDeleted(tracks);
    m_trackStore->remove(tracks);
}

void SingleMusicLibrary::reload()
{
    emit runLibraryScan(m_trackStore->tracks());
}

void SingleMusicLibrary::rescan()
{
    getAllTracks();
}

void SingleMusicLibrary::refreshTracks(const TrackList& tracks)
{
    //    m_trackStore.clear();

    const TrackPtrList newTracks = m_trackStore->add(tracks);
    //    m_trackStore.sort(m_order);

    emit tracksLoaded(newTracks);
}

LibraryInfo* SingleMusicLibrary::info() const
{
    return m_info;
}

TrackStore* SingleMusicLibrary::trackStore() const
{
    return m_trackStore.get();
}

Track* SingleMusicLibrary::track(int id) const
{
    return m_trackStore->track(id);
}

TrackPtrList SingleMusicLibrary::tracks() const
{
    return m_trackStore->tracks();
}

Library::SortOrder SingleMusicLibrary::sortOrder() const
{
    return m_order;
}

void SingleMusicLibrary::sortTracks(SortOrder order)
{
    m_order = order;
    m_trackStore->sort(m_order);
}

void SingleMusicLibrary::getAllTracks()
{
    emit loadAllTracks(m_order);
}
} // namespace Fy::Core::Library
