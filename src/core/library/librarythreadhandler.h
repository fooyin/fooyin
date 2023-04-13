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

#include "librarydatabasemanager.h"
#include "libraryscanner.h"

#include <QObject>

#include <deque>

namespace Fy::Core {
namespace DB {
class Database;
}

namespace Library {
struct LibraryInfo;

struct ScanRequest
{
    LibraryInfo library;
    Core::TrackList tracks;
};

class LibraryThreadHandler : public QObject
{
    Q_OBJECT

public:
    explicit LibraryThreadHandler(DB::Database* database, Utils::SettingsManager* settings, QObject* parent = nullptr);
    ~LibraryThreadHandler();

    void stopScanner();
    void libraryRemoved(int id);

signals:
    void scanLibrary(const Library::LibraryInfo& library, const Core::TrackList& tracks);
    void scanNext(const Library::LibraryInfo& library, const Core::TrackList& tracks);

    void progressChanged(int percent);
    void statusChanged(const Library::LibraryInfo& library);

    void addedTracks(Core::TrackList tracks);
    void updatedTracks(Core::TrackList tracks);
    void tracksDeleted(const Core::TrackList& tracks);

    void getAllTracks();
    void allTracksLoaded();
    void gotTracks(const Core::TrackList& result);

private:
    void addScanRequest(const Library::LibraryInfo& library, const Core::TrackList& tracks);
    void finishScanRequest();

    DB::Database* m_database;
    Utils::SettingsManager* m_settings;

    QThread* m_thread;
    LibraryScanner m_scanner;
    LibraryDatabaseManager m_libraryDatabaseManager;

    std::deque<ScanRequest> m_scanRequests;
};
} // namespace Library
} // namespace Fy::Core
