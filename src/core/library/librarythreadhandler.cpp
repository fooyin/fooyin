/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
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
#include <utils/settings/settingsmanager.h>

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
struct LibraryScanRequest
{
    int id;
    ScanRequest::Type type;
    LibraryInfo library;
    QString dir;
    TrackList tracks;
    bool onlyModified{true};
};

struct LibraryThreadHandler::Private
{
    LibraryThreadHandler* self;

    DbConnectionPoolPtr dbPool;
    MusicLibrary* library;
    SettingsManager* settings;

    QThread thread;
    LibraryScanner scanner;
    TrackDatabaseManager trackDatabaseManager;

    std::deque<LibraryScanRequest> scanRequests;
    int currentRequestId{-1};

    Private(LibraryThreadHandler* self_, DbConnectionPoolPtr dbPool_, MusicLibrary* library_,
            SettingsManager* settings_)
        : self{self_}
        , dbPool{std::move(dbPool_)}
        , library{library_}
        , settings{settings_}
        , scanner{dbPool, settings}
        , trackDatabaseManager{dbPool}
    {
        scanner.moveToThread(&thread);
        trackDatabaseManager.moveToThread(&thread);

        QObject::connect(library, &MusicLibrary::tracksScanned, self, [this]() {
            if(!scanRequests.empty()) {
                execNextRequest();
            }
        });

        thread.start();
    }

    void scanLibrary(const LibraryScanRequest& request)
    {
        QMetaObject::invokeMethod(&scanner, [this, request]() {
            scanner.scanLibrary(request.library, library->tracks(), request.onlyModified);
        });
    }

    void scanTracks(const LibraryScanRequest& request)
    {
        QMetaObject::invokeMethod(&scanner,
                                  [this, request]() { scanner.scanTracks(library->tracks(), request.tracks); });
    }

    void scanDirectory(const LibraryScanRequest& request)
    {
        QMetaObject::invokeMethod(&scanner, [this, request]() {
            scanner.scanLibraryDirectory(request.library, request.dir, library->tracks());
        });
    }

    ScanRequest addLibraryScanRequest(const LibraryInfo& libraryInfo, bool onlyModified)
    {
        const int id = nextRequestId();

        ScanRequest request{.type = ScanRequest::Library, .id = id, .cancel = [this, id]() {
                                cancelScanRequest(id);
                            }};

        scanRequests.emplace_back(id, ScanRequest::Library, libraryInfo, QStringLiteral(""), TrackList{}, onlyModified);

        if(scanRequests.size() == 1) {
            execNextRequest();
        }

        return request;
    }

    ScanRequest addTracksScanRequest(const TrackList& tracks)
    {
        const int id = nextRequestId();

        ScanRequest request{.type = ScanRequest::Tracks, .id = id, .cancel = [this, id]() {
                                cancelScanRequest(id);
                            }};

        scanRequests.emplace_front(id, ScanRequest::Tracks, LibraryInfo{}, QStringLiteral(""), tracks);

        // Track scans take precedence
        const auto currRequest = currentRequest();
        if(currRequest && currRequest->type == ScanRequest::Library) {
            scanner.pauseThread();
            execNextRequest();
        }
        else if(scanRequests.size() == 1) {
            execNextRequest();
        }

        return request;
    }

    ScanRequest addDirectoryScanRequest(const LibraryInfo& libraryInfo, const QString& dir)
    {
        const int id = nextRequestId();

        ScanRequest request{.type = ScanRequest::Library, .id = id, .cancel = [this, id]() {
                                cancelScanRequest(id);
                            }};

        scanRequests.emplace_back(id, ScanRequest::Library, libraryInfo, dir, TrackList{});

        if(scanRequests.size() == 1) {
            execNextRequest();
        }

        return request;
    }

    std::optional<LibraryScanRequest> currentRequest() const
    {
        const auto requestIt = std::ranges::find_if(
            scanRequests, [this](const auto& request) { return request.id == currentRequestId; });
        if(requestIt != scanRequests.cend()) {
            return *requestIt;
        }
        return {};
    }

    void execNextRequest()
    {
        if(scanRequests.empty()) {
            return;
        }

        const auto& request = scanRequests.front();
        currentRequestId    = request.id;

        if(request.type == ScanRequest::Tracks) {
            scanTracks(request);
        }
        else {
            if(request.dir.isEmpty()) {
                scanLibrary(request);
            }
            else {
                scanDirectory(request);
            }
        }
    }

    void finishScanRequest()
    {
        if(const auto request = currentRequest()) {
            std::erase_if(scanRequests,
                          [this](const auto& pendingRequest) { return pendingRequest.id == currentRequestId; });

            if(request->type == ScanRequest::Tracks) {
                // Next request (if any) will be started after tracksScanned is emitted from MusicLibrary
                return;
            }
        }

        currentRequestId = -1;
        execNextRequest();
    }

    void cancelScanRequest(int id)
    {
        if(currentRequestId == id) {
            // Will be removed in finishScanRequest
            scanner.stopThread();
        }
        else {
            std::erase_if(scanRequests, [id](const auto& request) { return request.id == id; });
        }
    }
};

LibraryThreadHandler::LibraryThreadHandler(DbConnectionPoolPtr dbPool, MusicLibrary* library, SettingsManager* settings,
                                           QObject* parent)
    : QObject{parent}
    , p{std::make_unique<Private>(this, std::move(dbPool), library, settings)}
{
    QObject::connect(&p->trackDatabaseManager, &TrackDatabaseManager::gotTracks, this,
                     &LibraryThreadHandler::gotTracks);
    QObject::connect(&p->trackDatabaseManager, &TrackDatabaseManager::updatedTracks, this,
                     &LibraryThreadHandler::tracksUpdated);
    QObject::connect(&p->scanner, &Worker::finished, this, [this]() { p->finishScanRequest(); });
    QObject::connect(&p->scanner, &LibraryScanner::progressChanged, this,
                     [this](int percent) { emit progressChanged(p->currentRequestId, percent); });
    QObject::connect(&p->scanner, &LibraryScanner::scannedTracks, this,
                     [this](const TrackList& tracks) { emit scannedTracks(p->currentRequestId, tracks); });
    QObject::connect(&p->scanner, &LibraryScanner::statusChanged, this, &LibraryThreadHandler::statusChanged);
    QObject::connect(&p->scanner, &LibraryScanner::scanUpdate, this, &LibraryThreadHandler::scanUpdate);
    QObject::connect(
        &p->scanner, &LibraryScanner::directoryChanged, this,
        [this](const LibraryInfo& libraryInfo, const QString& dir) { p->addDirectoryScanRequest(libraryInfo, dir); });

    QMetaObject::invokeMethod(&p->scanner, &Worker::initialiseThread);
    QMetaObject::invokeMethod(&p->trackDatabaseManager, &Worker::initialiseThread);
}

LibraryThreadHandler::~LibraryThreadHandler()
{
    p->scanner.stopThread();
    p->trackDatabaseManager.stopThread();

    p->thread.quit();
    p->thread.wait();
}

void LibraryThreadHandler::getAllTracks()
{
    QMetaObject::invokeMethod(&p->trackDatabaseManager, &TrackDatabaseManager::getAllTracks);
}

void LibraryThreadHandler::setupWatchers(const LibraryInfoMap& libraries, bool enabled)
{
    QMetaObject::invokeMethod(&p->scanner,
                              [this, libraries, enabled]() { p->scanner.setupWatchers(libraries, enabled); });
}

ScanRequest LibraryThreadHandler::refreshLibrary(const LibraryInfo& library)
{
    return p->addLibraryScanRequest(library, true);
}

ScanRequest LibraryThreadHandler::scanLibrary(const LibraryInfo& library)
{
    return p->addLibraryScanRequest(library, false);
}

ScanRequest LibraryThreadHandler::scanTracks(const TrackList& tracks)
{
    return p->addTracksScanRequest(tracks);
}

void LibraryThreadHandler::libraryRemoved(int id)
{
    if(p->scanRequests.empty()) {
        return;
    }

    const auto request = p->currentRequest();
    if(request && request->type == ScanRequest::Library && request->library.id == id) {
        p->scanner.stopThread();
    }
    else {
        std::erase_if(p->scanRequests, [id](const auto& pendingRequest) { return pendingRequest.library.id == id; });
    }
}

void LibraryThreadHandler::saveUpdatedTracks(const TrackList& tracks)
{
    QMetaObject::invokeMethod(&p->trackDatabaseManager,
                              [this, tracks]() { p->trackDatabaseManager.updateTracks(tracks); });
}

void LibraryThreadHandler::saveUpdatedTrackStats(const TrackList& track)
{
    QMetaObject::invokeMethod(&p->trackDatabaseManager,
                              [this, track]() { p->trackDatabaseManager.updateTrackStats(track); });
}

void LibraryThreadHandler::cleanupTracks()
{
    QMetaObject::invokeMethod(&p->trackDatabaseManager, &TrackDatabaseManager::cleanupTracks);
}
} // namespace Fooyin

#include "moc_librarythreadhandler.cpp"
