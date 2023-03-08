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

#include "librarydatabasemanager.h"
#include "libraryscanner.h"
#include "musiclibraryinternal.h"
#include "trackstore.h"

namespace Fy {

namespace Utils {
class ThreadManager;
class SettingsManager;
} // namespace Utils

namespace Core {
namespace DB {
class Database;
}

namespace Library {
struct LibraryInfo;

class SingleMusicLibrary : public MusicLibraryInternal
{
    Q_OBJECT

public:
    SingleMusicLibrary(LibraryInfo* info, DB::Database* database, Utils::ThreadManager* threadManager,
                       Utils::SettingsManager* settings, QObject* parent = nullptr);

    void reload() override;
    void rescan() override;
    void refreshTracks(const TrackList& result);

    [[nodiscard]] LibraryInfo* info() const override;

    [[nodiscard]] Track* track(int id) const override;
    [[nodiscard]] TrackPtrList tracks() const override;
    [[nodiscard]] int trackCount() const override;

    [[nodiscard]] TrackStore* trackStore() const override;

    [[nodiscard]] SortOrder sortOrder() const override;
    void sortTracks(SortOrder order) override;

    void updateTracks(const TrackPtrList& tracks);

signals:
    void tracksSelChanged();
    void updateSaveTracks(Core::TrackPtrList tracks);

protected:
    void getAllTracks();
    void loadTracks(const TrackList& tracks);
    void addNewTracks(const TrackList& tracks);
    void updateChangedTracks(const TrackList& tracks);
    void removeDeletedTracks(const TrackPtrList& tracks);

private:
    LibraryInfo* m_info;
    Utils::ThreadManager* m_threadManager;
    DB::Database* m_database;
    Utils::SettingsManager* m_settings;
    LibraryScanner m_scanner;
    LibraryDatabaseManager m_libraryDatabaseManager;

    std::unique_ptr<TrackStore> m_trackStore;

    SortOrder m_order{Library::SortOrder::YearDesc};
};
} // namespace Library
} // namespace Core
} // namespace Fy
