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
#include "librarymanager.h"

#include <QThread>

class LibraryPlaylistInterface;
class ThreadManager;
class MusicLibraryInteractor;

namespace Library {
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

    Track* track(int id);
    TrackPtrList tracks(const std::vector<int>& ids);
    TrackPtrList tracks();
    TrackPtrList allTracks();

    SortOrder sortOrder();
    void changeOrder(SortOrder order);

    void changeTrackSelection(const QSet<Track*>& tracks);
    void trackSelectionChanged(const QSet<Track*>& tracks);

    void libraryAdded();

    void prepareTracks(int idx = 0);

    TrackPtrList selectedTracks();

    void getAllTracks();
    void updateTracks(const TrackPtrList& tracks);

    void addInteractor(MusicLibraryInteractor* interactor);

signals:
    void runLibraryScan(TrackPtrList tracks, Library::LibraryInfo info);
    void runAllLibrariesScan(TrackPtrList tracks);
    void libraryRemoved();

    void tracksChanged();
    void tracksSelChanged();

    void tracksLoaded(const TrackPtrList& tracks);
    void tracksAdded();
    void tracksUpdated();
    void tracksDeleted();
    void loadAllTracks();
    void updateSaveTracks(TrackPtrList tracks);

protected:
    void loadTracks(const TrackList& tracks);
    void addNewTracks(const TrackList& tracks);
    void updateChangedTracks(const TrackList& tracks);
    void removeDeletedTracks(const IdSet& tracks);

private:
    struct Private;
    std::unique_ptr<MusicLibrary::Private> p;
};
} // namespace Library
