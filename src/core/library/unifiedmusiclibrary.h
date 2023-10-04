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

#include "libraryscanner.h"
#include "librarythreadhandler.h"
#include "musiclibrary.h"

#include <QCoroCore>

namespace Fy {

namespace Utils {
class SettingsManager;
} // namespace Utils

namespace Core {
namespace DB {
class Database;
}

namespace Library {
struct LibraryInfo;

class UnifiedMusicLibrary : public MusicLibrary
{
    Q_OBJECT

public:
    UnifiedMusicLibrary(LibraryManager* libraryManager, DB::Database* database, Utils::SettingsManager* settings,
                        QObject* parent = nullptr);

    void loadLibrary() override;
    void reloadAll() override;
    void reload(const LibraryInfo& library) override;
    void rescan() override;

    [[nodiscard]] bool hasLibrary() const override;
    [[nodiscard]] bool isEmpty() const override;

    [[nodiscard]] TrackList tracks() const override;

    void saveTracks(const Core::TrackList& tracks) override;

    QCoro::Task<void> changeSort(QString sort);

    void removeLibrary(int id) override;

private:
    void getAllTracks();
    QCoro::Task<void> loadTracks(TrackList tracks);
    QCoro::Task<TrackList> addTracks(TrackList tracks);
    QCoro::Task<void> newTracks(TrackList tracks);
    QCoro::Task<void> updateTracks(TrackList tracks);
    void removeTracks(const TrackList& tracks);

    void libraryStatusChanged(const LibraryInfo& library);

    LibraryManager* m_libraryManager;
    DB::Database* m_database;
    Utils::SettingsManager* m_settings;
    LibraryThreadHandler m_threadHandler;

    TrackList m_tracks;
};
} // namespace Library
} // namespace Core
} // namespace Fy
