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

#include "musiclibrary.h"

#include "core/app/threadmanager.h"
#include "core/library/models/track.h"
#include "core/library/sorting/sorting.h"
#include "core/playlist/libraryplaylistinterface.h"
#include "librarydatabasemanager.h"
#include "libraryinfo.h"
#include "librarymanager.h"
#include "libraryscanner.h"

#include <QTimer>
#include <utility>
#include <utils/helpers.h>

namespace Library {
MusicLibrary::MusicLibrary(LibraryPlaylistInterface* playlistInteractor, LibraryManager* libraryManager,
                           ThreadManager* threadManager, QObject* parent)
    : QObject(parent)
    , m_playlistInteractor(playlistInteractor)
    , m_libraryManager(libraryManager)
    , m_threadManager(threadManager)
    , m_scanner(m_libraryManager)
    , m_order(Library::SortOrder::YearDesc)
{
    m_threadManager->moveToNewThread(&m_scanner);
    m_threadManager->moveToNewThread(&m_libraryDatabaseManager);

    connect(libraryManager, &Library::LibraryManager::libraryRemoved, &m_scanner, &LibraryScanner::stopThread);

    connect(this, &MusicLibrary::runLibraryScan, &m_scanner, &LibraryScanner::scanLibrary);
    connect(this, &MusicLibrary::runAllLibrariesScan, &m_scanner, &LibraryScanner::scanAll);
    connect(&m_scanner, &LibraryScanner::libraryAdded, this, &MusicLibrary::libraryAdded);
    connect(&m_scanner, &LibraryScanner::addedTracks, this, &MusicLibrary::newTracksAdded);
    connect(&m_scanner, &LibraryScanner::updatedTracks, this, &MusicLibrary::tracksUpdated);
    connect(&m_scanner, &LibraryScanner::tracksDeleted, this, &MusicLibrary::tracksDeleted);

    connect(&m_libraryDatabaseManager, &LibraryDatabaseManager::gotTracks, this, &MusicLibrary::tracksLoaded);
    connect(&m_libraryDatabaseManager, &LibraryDatabaseManager::gotItems, this, &MusicLibrary::itemsHaveLoaded);
    connect(&m_libraryDatabaseManager, &LibraryDatabaseManager::tracksFiltered, this,
            &MusicLibrary::filteredTracksLoaded);

    connect(this, &MusicLibrary::loadAllItems, &m_libraryDatabaseManager, &LibraryDatabaseManager::getAllItems);
    connect(this, &MusicLibrary::loadItemsByFilter, &m_libraryDatabaseManager,
            &LibraryDatabaseManager::getItemsByFilter);
    connect(this, &MusicLibrary::loadAllTracks, &m_libraryDatabaseManager, &LibraryDatabaseManager::getAllTracks);
    connect(this, &MusicLibrary::loadFilteredTracks, &m_libraryDatabaseManager, &LibraryDatabaseManager::filterTracks);
    connect(this, &MusicLibrary::updateSaveTracks, &m_libraryDatabaseManager, &LibraryDatabaseManager::updateTracks);

    load();
}

MusicLibrary::~MusicLibrary()
{
    qDeleteAll(m_tracks);
}

void MusicLibrary::load()
{
    getAllTracks();
    emit filteredItems(-1);
    QTimer::singleShot(3000, this, &Library::MusicLibrary::reloadAll);
}

void MusicLibrary::libraryAdded()
{
    load();
}

void MusicLibrary::prepareTracks(int idx)
{
    if(!m_filteredTracks.empty()) {
        m_playlistInteractor->createPlaylist(m_filteredTracks, idx);
    }

    else if(m_selectedTracks.empty()) {
        m_playlistInteractor->createPlaylist(m_tracks, 0);
    }

    else {
        m_playlistInteractor->createPlaylist(m_tracks, idx);
    }
}

TrackPtrList MusicLibrary::selectedTracks()
{
    return m_selectedTracks;
}

void MusicLibrary::changeFilter(int index)
{
    if(!m_activeFilters.isEmpty()) {
        for(const auto& [filterIndex, filter] : asRange(m_filterIndexes)) {
            if(index <= filterIndex) {
                m_activeFilters.remove(filter);
            }
        }
    }
    emit filteredItems(index - 1);
}

void MusicLibrary::resetFilter(Filters::FilterType type)
{
    emit filterReset(type, m_activeFilters.value(type));
}

void MusicLibrary::registerFilter(Filters::FilterType type, int index)
{
    m_filterIndexes.insert(index, type);
    // TODO: Properly handle different sort orders
    m_filterSortOrders.insert(type, SortOrder::TitleAsc);
}

void MusicLibrary::unregisterFilter(int index)
{
    Filters::FilterType type = m_filterIndexes.take(index);
    m_activeFilters.remove(type);
    emit filteredItems(-1);
    getFilteredTracks();
}

void MusicLibrary::updateTracks(const TrackPtrList& tracks)
{
    emit updateSaveTracks(tracks);
}

void MusicLibrary::tracksLoaded(const TrackList& tracks)
{
    qDeleteAll(m_tracks);
    refreshTracks(tracks);
    emit filteredTracks();
}

void MusicLibrary::itemsHaveLoaded(Filters::FilterType type, FilterList result)
{
    emit itemsLoaded(type, std::move(result));
}

void MusicLibrary::filteredTracksLoaded(TrackPtrList tracks)
{
    m_filteredTracks = std::move(tracks);
    emit filteredTracks();
}

void MusicLibrary::newTracksAdded(const TrackList& tracks)
{
    for(const auto& track : tracks) {
        auto* trackPtr = new Track(track);
        m_tracks.emplace_back(trackPtr);
        m_trackMap.emplace(trackPtr->id(), trackPtr);
    }
    Library::sortTracks(m_tracks, m_order);
    emit filteredItems();
    getFilteredTracks();
}

void MusicLibrary::tracksUpdated(const TrackList& tracks)
{
    for(const auto& track : tracks) {
        if(m_trackMap.count(track.id())) {
            Track* libraryTrack = m_trackMap.at(track.id());
            if(libraryTrack) {
                *libraryTrack = track;
            }
        }
    }
    emit filteredItems();
    getFilteredTracks();
}

void MusicLibrary::tracksDeleted(const IdSet& tracks)
{
    for(auto trackId : tracks) {
        if(m_trackMap.count(trackId)) {
            {
                Track* libraryTrack = m_trackMap.at(trackId);
                libraryTrack->setIsEnabled(false);
            }
        }
    }
    emit filteredItems();
    getFilteredTracks();
}

void MusicLibrary::reloadAll()
{
    emit runAllLibrariesScan(m_tracks);
}

void MusicLibrary::reload(const Library::LibraryInfo& info)
{
    emit runLibraryScan(m_tracks, info);
}

void MusicLibrary::refresh()
{
    getAllTracks();
    emit filteredItems(-1);
}

void MusicLibrary::refreshTracks(const TrackList& result)
{
    m_tracks.clear();
    m_trackMap.clear();
    m_filteredTracks.clear();
    for(const auto& track : result) {
        auto* trackPtr = new Track(track);
        m_tracks.emplace_back(trackPtr);
        m_trackMap.emplace(trackPtr->id(), trackPtr);
    }
    Library::sortTracks(m_tracks, m_order);
}

void MusicLibrary::items(Filters::FilterType type)
{
    if(!m_activeFilters.isEmpty() || !m_searchFilter.isEmpty()) {
        getItemsByFilter(type, m_filterSortOrders.value(type));
    }
    else {
        getAllItems(type, m_filterSortOrders.value(type));
    }
}

TrackPtrList MusicLibrary::tracks()
{
    if(!m_filteredTracks.empty() || !m_searchFilter.isEmpty()) {
        return m_filteredTracks;
    }
    return m_tracks;
}

QList<Filters::FilterType> MusicLibrary::filters()
{
    return m_filterIndexes.values();
}

Library::SortOrder MusicLibrary::sortOrder()
{
    return m_order;
}

SortOrder MusicLibrary::filterOrder(Filters::FilterType type)
{
    return m_filterSortOrders.value(type);
}

void MusicLibrary::changeOrder(SortOrder order)
{
    m_order = order;
    Library::sortTracks(m_tracks, m_order);
    if(!m_filteredTracks.empty()) {
        Library::sortTracks(m_filteredTracks, m_order);
    }
    emit filteredTracks();
}

void MusicLibrary::changeFilterOrder(Filters::FilterType type, SortOrder order)
{
    m_filterSortOrders.insert(type, order);
    emit orderedFilter(type);
}

bool MusicLibrary::tracksHaveFiltered()
{
    return !m_filteredTracks.empty();
}

void MusicLibrary::changeSelection(const IdSet& indexes, Filters::FilterType type, int index)
{
    if(m_activeFilters.isEmpty() && indexes.contains(-1)) {
        return;
    }

    for(const auto& [filterIndex, filter] : asRange(m_filterIndexes)) {
        if(index < filterIndex) {
            m_activeFilters.remove(filter);
        }
    }

    if(indexes.contains(-1)) {
        m_activeFilters.remove(type);
    }
    else {
        m_activeFilters.insert(type, indexes);
    }

    m_filteredTracks.clear();
    m_selectedTracks.clear();

    const IdSet& filterIds = m_activeFilters.value(type);

    if((!m_activeFilters.isEmpty() && !filterIds.contains(-1)) || !m_searchFilter.isEmpty()) {
        getFilteredTracks();
    }

    else {
        emit filteredTracks();
    }
}

void MusicLibrary::selectionChanged(const IdSet& indexes, Filters::FilterType type, int index)
{
    if(indexes.empty()) {
        return;
    }

    changeSelection(indexes, type, index);

    emit filteredItems(index);
}

void MusicLibrary::changeTrackSelection(const QSet<Track*>& tracks)
{
    std::vector<Track*> newSelectedTracks;
    for(const auto& track : tracks) {
        newSelectedTracks.emplace_back(track);
    }

    if(m_selectedTracks == newSelectedTracks) {
        return;
    }

    m_selectedTracks = std::move(newSelectedTracks);
}

void MusicLibrary::trackSelectionChanged(const QSet<Track*>& tracks)
{
    if(tracks.isEmpty()) {
        return;
    }

    changeTrackSelection(tracks);
    emit tracksSelChanged();
}

void MusicLibrary::changeSearch(const QString& search)
{
    m_searchFilter = search;
    m_filteredTracks.clear();
    if(search.isEmpty()) {
        if(!m_activeFilters.isEmpty()) {
            getFilteredTracks();
        }
        else {
            emit filteredTracks();
        }
        emit filteredItems(-1);
    }
    else {
        getFilteredTracks();
        emit filteredItems(-1);
    }
}

void MusicLibrary::searchChanged(const QString& search)
{
    changeSearch(search);
}

void MusicLibrary::getAllItems(Filters::FilterType type, SortOrder order)
{
    emit loadAllItems(type, order);
}

void MusicLibrary::getItemsByFilter(Filters::FilterType type, SortOrder order)
{
    emit loadItemsByFilter(type, m_activeFilters, m_searchFilter, order);
}

void MusicLibrary::getAllTracks()
{
    emit loadAllTracks();
}

void MusicLibrary::getFilteredTracks()
{
    m_filteredTracks.clear();
    emit loadFilteredTracks(m_tracks, m_activeFilters, m_searchFilter);
}
} // namespace Library
