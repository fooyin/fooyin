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

#pragma once

#include "core/library/models/trackfwd.h"
#include "core/library/sorting/sortorder.h"
#include "librarydatabasemanager.h"
#include "libraryinfo.h"
#include "libraryscanner.h"

#include <QThread>

class LibraryPlaylistInterface;
class LibraryDatabaseManager;
class ThreadManager;
class MusicLibraryInteractor;

namespace Library {
class LibraryScanner;

class MusicLibrary : public QObject
{
    Q_OBJECT

public:
    MusicLibrary(LibraryPlaylistInterface* playlistInteractor, LibraryManager* libraryManager,
                 ThreadManager* threadManager, QObject* parent = nullptr);
    ~MusicLibrary() override;

    void load();
    void reloadAll();
    void reload(const Library::LibraryInfo& info);
    void refresh();
    void refreshTracks(const TrackList& result);

    TrackPtrList tracks();

    //    QList<Filters::FilterType> filters();
    SortOrder sortOrder();
    //    SortOrder filterOrder(Filters::FilterType type);

    void changeOrder(SortOrder order);
    //    void changeFilterOrder(Filters::FilterType type, SortOrder order);

    void changeSelection(const IdSet& indexes, Filters::FilterType type, int index);
    void selectionChanged(const IdSet& indexes, Filters::FilterType type, int index);

    void changeTrackSelection(const QSet<Track*>& tracks);
    void trackSelectionChanged(const QSet<Track*>& tracks);

    //    void changeSearch(const QString& search);
    //    void searchChanged(const QString& search);

    void libraryAdded();

    void prepareTracks(int idx = 0);

    TrackPtrList selectedTracks();

    void getAllTracks();
    void updateTracks(const TrackPtrList& tracks);

    //    void changeFilter(int index);
    //    void resetFilter(Filters::FilterType type);
    //    void unregisterFilter(int index);
    void addInteractor(MusicLibraryInteractor* interactor);

signals:
    void runLibraryScan(TrackPtrList tracks, Library::LibraryInfo info);
    void runAllLibrariesScan(TrackPtrList tracks);
    void filteredTracks();
    void filteredItems(int index = -1);
    //    void orderedFilter(Filters::FilterType type);
    void filterReset(Filters::FilterType type, const IdSet& selection);
    void tracksSelChanged();

    void tracksLoaded(const TrackPtrList& tracks);
    void loadAllTracks();
    void updateSaveTracks(TrackPtrList tracks);

protected:
    void tracksHaveLoaded(const TrackList& tracks);
    void newTracksAdded(const TrackList& tracks);
    void tracksUpdated(const TrackList& tracks);
    void tracksDeleted(const IdSet& tracks);

private:
    LibraryPlaylistInterface* m_playlistInteractor;
    LibraryManager* m_libraryManager;
    ThreadManager* m_threadManager;
    LibraryScanner m_scanner;
    LibraryDatabaseManager m_libraryDatabaseManager;

    TrackPtrList m_tracks;
    TrackHash m_trackMap;

    std::vector<Track*> m_selectedTracks;
    std::vector<MusicLibraryInteractor*> m_interactors;

    SortOrder m_order;
};
} // namespace Library
