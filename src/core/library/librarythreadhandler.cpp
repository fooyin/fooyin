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

#include <QThread>

namespace Fy::Core::Library {

LibraryThreadHandler::LibraryThreadHandler(DB::Database* database, QObject* parent)
    : QObject{parent}
    , m_database{database}
    , m_thread{new QThread(this)}
    , m_scanner{m_database}
    , m_libraryDatabaseManager{database}
{
    m_scanner.moveToThread(m_thread);
    m_libraryDatabaseManager.moveToThread(m_thread);

    connect(this, &LibraryThreadHandler::scanLibrary, this, &LibraryThreadHandler::addScanRequest);
    connect(this, &LibraryThreadHandler::scanNext, &m_scanner, &LibraryScanner::scanLibrary);
    connect(this, &LibraryThreadHandler::getAllTracks, &m_libraryDatabaseManager,
            &LibraryDatabaseManager::getAllTracks);
    connect(&m_libraryDatabaseManager, &LibraryDatabaseManager::gotTracks, this, &LibraryThreadHandler::gotTracks);
    connect(&m_scanner, &Utils::Worker::finished, this, &LibraryThreadHandler::finishScanRequest);
    connect(&m_scanner, &LibraryScanner::progressChanged, this, &LibraryThreadHandler::progressChanged);
    connect(&m_scanner, &LibraryScanner::statusChanged, this, &LibraryThreadHandler::statusChanged);
    connect(&m_scanner, &LibraryScanner::addedTracks, this, &LibraryThreadHandler::addedTracks);
    connect(&m_scanner, &LibraryScanner::updatedTracks, this, &LibraryThreadHandler::updatedTracks);
    connect(&m_scanner, &LibraryScanner::tracksDeleted, this, &LibraryThreadHandler::tracksDeleted);

    m_thread->start();
}

LibraryThreadHandler::~LibraryThreadHandler()
{
    stopScanner();
    m_thread->quit();
    m_thread->wait();
}

void LibraryThreadHandler::stopScanner()
{
    m_scanner.stopThread();
}

void LibraryThreadHandler::libraryRemoved(int id)
{
    if(m_scanRequests.empty()) {
        return;
    }
    if(m_scanner.currentLibrary().id == id) {
        // Scanner will emit finished signal which will remove library from front of queue
        stopScanner();
    }
    else {
        std::erase_if(m_scanRequests, [id](const ScanRequest& request) {
            return request.library.id == id;
        });
    }
}

void LibraryThreadHandler::addScanRequest(const LibraryInfo& library, const TrackList& tracks)
{
    ScanRequest request{library, tracks};
    m_scanRequests.emplace_back(request);

    if(m_scanRequests.size() == 1) {
        emit scanNext(library, tracks);
    }
}

void LibraryThreadHandler::finishScanRequest()
{
    m_scanRequests.pop_front();
    if(!m_scanRequests.empty()) {
        ScanRequest request = m_scanRequests.front();
        emit scanNext(request.library, request.tracks);
    }
}
} // namespace Fy::Core::Library
