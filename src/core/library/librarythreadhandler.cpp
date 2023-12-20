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

#include "library/libraryinfo.h"
#include "libraryscanner.h"
#include "trackdatabasemanager.h"

#include <core/library/musiclibrary.h>

#include <QThread>

#include <deque>

namespace {
int nextRequestId()
{
    static int requestId{0};
    return requestId++;
}
} // namespace

namespace Fooyin {
struct LibraryScanRequest : ScanRequest
{
    LibraryInfo library;
    TrackList tracks;
};

struct LibraryThreadHandler::Private
{
    LibraryThreadHandler* self;

    Database* database;
    MusicLibrary* library;

    QThread* thread;
    LibraryScanner scanner;
    TrackDatabaseManager trackDatabaseManager;

    std::deque<std::unique_ptr<LibraryScanRequest>> scanRequests;
    int currentRequestId{-1};

    Private(LibraryThreadHandler* self, Database* database, MusicLibrary* library)
        : self{self}
        , database{database}
        , library{library}
        , thread{new QThread(self)}
        , scanner{database}
        , trackDatabaseManager{database}
    {
        scanner.moveToThread(thread);
        trackDatabaseManager.moveToThread(thread);

        QObject::connect(library, &MusicLibrary::tracksScanned, self, [this]() {
            if(!scanRequests.empty()) {
                execNextRequest();
            }
        });

        thread->start();
    }

    void scanLibrary(const LibraryScanRequest& request)
    {
        currentRequestId = request.id;
        QMetaObject::invokeMethod(&scanner, "scanLibrary", Q_ARG(const LibraryInfo&, request.library),
                                  Q_ARG(const TrackList&, library->tracks()));
    }

    void scanTracks(const LibraryScanRequest& request)
    {
        currentRequestId = request.id;
        QMetaObject::invokeMethod(&scanner, "scanTracks", Q_ARG(const TrackList&, library->tracks()),
                                  Q_ARG(const TrackList&, request.tracks));
    }

    ScanRequest* addLibraryScanRequest(const LibraryInfo& library)
    {
        auto* request = scanRequests
                            .emplace_back(std::make_unique<LibraryScanRequest>(
                                ScanRequest{ScanRequest::Library, nextRequestId(), nullptr}, library, TrackList{}))
                            .get();
        request->cancel = [this, request]() {
            cancelScanRequest(request->id);
        };

        if(scanRequests.size() == 1) {
            scanLibrary(*request);
        }
        return request;
    }

    ScanRequest* addTracksScanRequest(const TrackList& tracks)
    {
        if(!scanRequests.empty()) {
            scanner.pauseThread();
        }

        LibraryScanRequest* request
            = scanRequests
                  .emplace_front(std::make_unique<LibraryScanRequest>(
                      ScanRequest{ScanRequest::Tracks, nextRequestId(), nullptr}, LibraryInfo{}, tracks))
                  .get();
        request->cancel = [this, request]() {
            cancelScanRequest(request->id);
        };

        scanTracks(*request);

        return request;
    }

    void execNextRequest()
    {
        const LibraryScanRequest& request = *scanRequests.front();
        switch(request.type) {
            case(ScanRequest::Tracks):
                scanTracks(request);
                break;
            case(ScanRequest::Library):
                scanLibrary(request);
                break;
        }
    }

    void finishScanRequest()
    {
        const bool scanType = scanRequests.front()->type;

        scanRequests.pop_front();
        currentRequestId = -1;

        if(scanRequests.empty() || scanType == ScanRequest::Tracks) {
            return;
        }

        execNextRequest();
    }

    void cancelScanRequest(int id)
    {
        if(currentRequestId == id) {
            scanner.stopThread();
        }
        else {
            std::erase_if(scanRequests, [id](const auto& request) { return request->id == id; });
        }
    }
};

LibraryThreadHandler::LibraryThreadHandler(Database* database, MusicLibrary* library, QObject* parent)
    : QObject{parent}
    , p{std::make_unique<Private>(this, database, library)}
{
    QObject::connect(&p->trackDatabaseManager, &TrackDatabaseManager::gotTracks, this,
                     &LibraryThreadHandler::gotTracks);
    QObject::connect(&p->scanner, &Worker::finished, this, [this]() { p->finishScanRequest(); });
    QObject::connect(&p->scanner, &LibraryScanner::progressChanged, this,
                     [this](int percent) { emit progressChanged(p->currentRequestId, percent); });
    QObject::connect(&p->scanner, &LibraryScanner::statusChanged, this, &LibraryThreadHandler::statusChanged);
    QObject::connect(&p->scanner, &LibraryScanner::scanUpdate, this, &LibraryThreadHandler::scanUpdate);
    QObject::connect(&p->scanner, &LibraryScanner::scannedTracks, this, &LibraryThreadHandler::scannedTracks);
    QObject::connect(&p->scanner, &LibraryScanner::tracksDeleted, this, &LibraryThreadHandler::tracksDeleted);
}

LibraryThreadHandler::~LibraryThreadHandler()
{
    p->scanner.stopThread();
    p->thread->quit();
    p->thread->wait();
}

void LibraryThreadHandler::getAllTracks()
{
    QMetaObject::invokeMethod(&p->trackDatabaseManager, &TrackDatabaseManager::getAllTracks);
}

void LibraryThreadHandler::scanLibrary(const LibraryInfo& library)
{
    p->addLibraryScanRequest(library);
}

ScanRequest* LibraryThreadHandler::scanTracks(const TrackList& tracks)
{
    return p->addTracksScanRequest(tracks);
}

void LibraryThreadHandler::libraryRemoved(int id)
{
    if(p->scanRequests.empty()) {
        return;
    }

    const LibraryScanRequest& request = *p->scanRequests.front();

    if(request.type == ScanRequest::Library && request.library.id == id) {
        p->scanner.stopThread();
    }
    else {
        std::erase_if(p->scanRequests, [id](const auto& request) { return request->library.id == id; });
    }
}

void LibraryThreadHandler::saveUpdatedTracks(const TrackList& tracks)
{
    QMetaObject::invokeMethod(&p->scanner, "updateTracks", Q_ARG(const TrackList&, tracks));
}

void LibraryThreadHandler::cleanupTracks()
{
    QMetaObject::invokeMethod(&p->trackDatabaseManager, &TrackDatabaseManager::cleanupTracks);
}
} // namespace Fooyin

#include "moc_librarythreadhandler.cpp"
