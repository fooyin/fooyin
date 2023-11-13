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

#include "librarythreadhandler.h"

#include "database/database.h"
#include "librarydatabasemanager.h"
#include "libraryscanner.h"

#include <core/library/libraryinfo.h>
#include <core/track.h>

#include <QThread>

#include <deque>

namespace Fy::Core::Library {
struct ScanRequest
{
    LibraryInfo library;
    Core::TrackList tracks;
};

struct LibraryThreadHandler::Private
{
    LibraryThreadHandler* self;

    DB::Database* database;

    QThread* thread;
    LibraryScanner scanner;
    LibraryDatabaseManager libraryDatabaseManager;

    std::deque<ScanRequest> scanRequests;
    int currentLibraryRequest{-1};

    Private(LibraryThreadHandler* self, DB::Database* database)
        : self{self}
        , database{database}
        , thread{new QThread(self)}
        , scanner{database}
        , libraryDatabaseManager{database}
    {
        scanner.moveToThread(thread);
        libraryDatabaseManager.moveToThread(thread);

        thread->start();
    }

    void addScanRequest(const LibraryInfo& library, const TrackList& tracks)
    {
        const ScanRequest request{library, tracks};
        scanRequests.emplace_back(request);

        if(scanRequests.size() == 1) {
            currentLibraryRequest = library.id;
            QMetaObject::invokeMethod(&scanner, "scanLibrary", Q_ARG(const LibraryInfo&, library),
                                      Q_ARG(const TrackList&, tracks));
        }
    }

    void finishScanRequest()
    {
        scanRequests.pop_front();
        currentLibraryRequest = -1;

        if(!scanRequests.empty()) {
            const ScanRequest request = scanRequests.front();
            currentLibraryRequest     = request.library.id;
            QMetaObject::invokeMethod(&scanner, "scanLibrary", Q_ARG(const LibraryInfo&, request.library),
                                      Q_ARG(const TrackList&, request.tracks));
        }
    }
};

LibraryThreadHandler::LibraryThreadHandler(DB::Database* database, QObject* parent)
    : QObject{parent}
    , p{std::make_unique<Private>(this, database)}
{
    QObject::connect(&p->libraryDatabaseManager, &LibraryDatabaseManager::gotTracks, this,
                     &LibraryThreadHandler::gotTracks);
    QObject::connect(&p->scanner, &Utils::Worker::finished, this, [this]() { p->finishScanRequest(); });
    QObject::connect(&p->scanner, &LibraryScanner::progressChanged, this, &LibraryThreadHandler::progressChanged);
    QObject::connect(&p->scanner, &LibraryScanner::statusChanged, this, &LibraryThreadHandler::statusChanged);
    QObject::connect(&p->scanner, &LibraryScanner::addedTracks, this, &LibraryThreadHandler::addedTracks);
    QObject::connect(&p->scanner, &LibraryScanner::updatedTracks, this, &LibraryThreadHandler::updatedTracks);
    QObject::connect(&p->scanner, &LibraryScanner::tracksDeleted, this, &LibraryThreadHandler::tracksDeleted);
}

LibraryThreadHandler::~LibraryThreadHandler()
{
    stopScanner();
    p->thread->quit();
    p->thread->wait();
}

void LibraryThreadHandler::stopScanner()
{
    p->scanner.stopThread();
}

void LibraryThreadHandler::getAllTracks()
{
    QMetaObject::invokeMethod(&p->libraryDatabaseManager, &LibraryDatabaseManager::getAllTracks);
}

void LibraryThreadHandler::scanLibrary(const LibraryInfo& library, const TrackList& tracks)
{
    p->addScanRequest(library, tracks);
}

void LibraryThreadHandler::libraryRemoved(int id)
{
    if(p->scanRequests.empty()) {
        return;
    }
    if(p->currentLibraryRequest == id) {
        // Scanner will emit finished signal which will remove library from front of queue
        stopScanner();
    }
    else {
        std::erase_if(p->scanRequests, [id](const ScanRequest& request) { return request.library.id == id; });
    }
}

void LibraryThreadHandler::saveUpdatedTracks(const TrackList& tracks)
{
    QMetaObject::invokeMethod(&p->scanner, "updateTracks", Q_ARG(const TrackList&, tracks));
}
} // namespace Fy::Core::Library
