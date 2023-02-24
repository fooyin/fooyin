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

#pragma once

#include "core/library/sorting/sortorder.h"
#include "core/models/trackfwd.h"
#include "libraryinfo.h"

#include <QThread>

namespace Utils {
class ThreadManager;
class SettingsManager;
} // namespace Utils

namespace Core {
namespace DB {
class Database;
}

namespace Playlist {
class LibraryPlaylistInterface;
}

namespace Library {
class LibraryManager;
class MusicLibraryInteractor;

class MusicLibrary : public QObject
{
    Q_OBJECT

public:
    MusicLibrary(Playlist::LibraryPlaylistInterface* playlistInteractor, LibraryManager* libraryManager,
                 Utils::ThreadManager* threadManager, DB::Database* database, Utils::SettingsManager* settings,
                 QObject* parent = nullptr);
    ~MusicLibrary() override;

    void load();
    void reloadAll();
    void reload(Library::LibraryInfo* info);
    void refresh();
    void refreshTracks(const TrackList& result);

    [[nodiscard]] Track* track(int id) const;
    [[nodiscard]] TrackPtrList tracks(const std::vector<int>& ids) const;
    [[nodiscard]] TrackPtrList tracks() const;
    [[nodiscard]] TrackPtrList allTracks() const;
    [[nodiscard]] int trackCount() const;

    [[nodiscard]] SortOrder sortOrder() const;
    void changeOrder(SortOrder order);

    void changeTrackSelection(const TrackSet& tracks);
    void trackSelectionChanged(const TrackSet& tracks);

    void libraryAdded();

    void prepareTracks(int idx = 0);

    [[nodiscard]] TrackPtrList selectedTracks() const;

    void getAllTracks();
    void updateTracks(const TrackPtrList& tracks);

    void addInteractor(MusicLibraryInteractor* interactor);

signals:
    void runLibraryScan(const Core::TrackPtrList& tracks, Library::LibraryInfo* info);
    void runAllLibrariesScan(const Core::TrackPtrList& tracks);
    void libraryRemoved();

    void tracksChanged();
    void tracksSelChanged();

    void tracksLoaded(const Core::TrackPtrList& tracks);
    void tracksAdded();
    void tracksUpdated();
    void tracksDeleted();
    void loadAllTracks();
    void updateSaveTracks(Core::TrackPtrList tracks);

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
} // namespace Core
