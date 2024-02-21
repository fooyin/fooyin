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
#include <ranges>

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
};

struct LibraryThreadHandler::Private
{
    LibraryThreadHandler* self;

    Database* database;
    MusicLibrary* library;
    SettingsManager* settings;

    QThread thread;
    LibraryScanner scanner;
    TrackDatabaseManager trackDatabaseManager;

    std::deque<LibraryScanRequest> scanRequests;
    int currentRequestId{-1};

    Private(LibraryThreadHandler* self_, Database* database_, MusicLibrary* library_, SettingsManager* settings_)
        : self{self_}
        , database{database_}
        , library{library_}
        , settings{settings_}
        , scanner{database, settings}
        , trackDatabaseManager{database}
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
        QMetaObject::invokeMethod(&scanner, "scanLibrary", Q_ARG(const LibraryInfo&, request.library),
                                  Q_ARG(const TrackList&, library->tracks()));
    }

    void scanTracks(const LibraryScanRequest& request)
    {
        QMetaObject::invokeMethod(&scanner, "scanTracks", Q_ARG(const TrackList&, library->tracks()),
                                  Q_ARG(const TrackList&, request.tracks));
    }

    void scanDirectory(const LibraryScanRequest& request)
    {
        QMetaObject::invokeMethod(&scanner, "scanLibraryDirectory", Q_ARG(const LibraryInfo&, request.library),
                                  Q_ARG(const QString&, request.dir), Q_ARG(const TrackList&, library->tracks()));
    }

    ScanRequest addLibraryScanRequest(const LibraryInfo& libraryInfo)
    {
        const int id = nextRequestId();

        ScanRequest request{.type = ScanRequest::Library, .id = id, .cancel = [this, id]() {
                                cancelScanRequest(id);
                            }};

        scanRequests.emplace_back(id, ScanRequest::Library, libraryInfo, QStringLiteral(""), TrackList{});

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

        switch(request.type) {
            case(ScanRequest::Tracks):
                scanTracks(request);
                break;
            case(ScanRequest::Library):
                if(request.dir.isEmpty()) {
                    scanLibrary(request);
                }
                else {
                    scanDirectory(request);
                }
                break;
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

LibraryThreadHandler::LibraryThreadHandler(Database* database, MusicLibrary* library, SettingsManager* settings,
                                           QObject* parent)
    : QObject{parent}
    , p{std::make_unique<Private>(this, database, library, settings)}
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
    QMetaObject::invokeMethod(&p->scanner, "setupWatchers", Q_ARG(const LibraryInfoMap&, libraries),
                              Q_ARG(bool, enabled));
}

ScanRequest LibraryThreadHandler::scanLibrary(const LibraryInfo& library)
{
    return p->addLibraryScanRequest(library);
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
    QMetaObject::invokeMethod(&p->trackDatabaseManager, "updateTracks", Q_ARG(const TrackList&, tracks));
}

void LibraryThreadHandler::saveUpdatedTrackStats(const Track& track)
{
    QMetaObject::invokeMethod(&p->trackDatabaseManager, "updateTrackStats", Q_ARG(const Track&, track));
}

void LibraryThreadHandler::cleanupTracks()
{
    QMetaObject::invokeMethod(&p->trackDatabaseManager, &TrackDatabaseManager::cleanupTracks);
}
} // namespace Fooyin

#include "moc_librarythreadhandler.cpp"
