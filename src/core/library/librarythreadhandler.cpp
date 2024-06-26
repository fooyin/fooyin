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
#include <QUrl>

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
    QList<QUrl> files;
    TrackList tracks;
    bool onlyModified{true};
};

struct LibraryThreadHandler::Private
{
    LibraryThreadHandler* m_self;

    DbConnectionPoolPtr m_dbPool;
    MusicLibrary* m_library;
    SettingsManager* m_settings;

    QThread m_thread;
    LibraryScanner m_scanner;
    TrackDatabaseManager m_trackDatabaseManager;

    std::deque<LibraryScanRequest> m_scanRequests;
    int m_currentRequestId{-1};

    Private(LibraryThreadHandler* self, DbConnectionPoolPtr dbPool, MusicLibrary* library,
            std::shared_ptr<PlaylistLoader> playlistLoader, std::shared_ptr<TagLoader> tagLoader,
            SettingsManager* settings)
        : m_self{self}
        , m_dbPool{std::move(dbPool)}
        , m_library{library}
        , m_settings{settings}
        , m_scanner{m_dbPool, std::move(playlistLoader), tagLoader, m_settings}
        , m_trackDatabaseManager{m_dbPool, tagLoader}
    {
        m_scanner.moveToThread(&m_thread);
        m_trackDatabaseManager.moveToThread(&m_thread);

        QObject::connect(m_library, &MusicLibrary::tracksScanned, m_self, [this]() {
            if(!m_scanRequests.empty()) {
                execNextRequest();
            }
        });

        m_thread.start();
    }

    void scanLibrary(const LibraryScanRequest& request)
    {
        QMetaObject::invokeMethod(&m_scanner, [this, request]() {
            m_scanner.scanLibrary(request.library, m_library->tracks(), request.onlyModified);
        });
    }

    void scanTracks(const LibraryScanRequest& request)
    {
        QMetaObject::invokeMethod(&m_scanner,
                                  [this, request]() { m_scanner.scanTracks(m_library->tracks(), request.tracks); });
    }

    void scanFiles(const LibraryScanRequest& request)
    {
        QMetaObject::invokeMethod(&m_scanner,
                                  [this, request]() { m_scanner.scanFiles(m_library->tracks(), request.files); });
    }

    void scanDirectory(const LibraryScanRequest& request)
    {
        QMetaObject::invokeMethod(&m_scanner, [this, request]() {
            m_scanner.scanLibraryDirectory(request.library, request.dir, m_library->tracks());
        });
    }

    ScanRequest addLibraryScanRequest(const LibraryInfo& libraryInfo, bool onlyModified)
    {
        const int id = nextRequestId();

        ScanRequest request{.type = ScanRequest::Library, .id = id, .cancel = [this, id]() {
                                cancelScanRequest(id);
                            }};

        LibraryScanRequest libraryRequest;
        libraryRequest.id           = id;
        libraryRequest.type         = ScanRequest::Library;
        libraryRequest.library      = libraryInfo;
        libraryRequest.onlyModified = onlyModified;

        m_scanRequests.emplace_back(libraryRequest);

        if(m_scanRequests.size() == 1) {
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

        LibraryScanRequest libraryRequest;
        libraryRequest.id     = id;
        libraryRequest.type   = ScanRequest::Tracks;
        libraryRequest.tracks = tracks;

        m_scanRequests.emplace_front(libraryRequest);

        // Track scans take precedence over library scans
        const auto currRequest = currentRequest();
        if(currRequest && currRequest->type == ScanRequest::Library) {
            m_scanner.pauseThread();
            execNextRequest();
        }
        else if(m_scanRequests.size() == 1) {
            execNextRequest();
        }

        return request;
    }

    ScanRequest addFilesScanRequest(const QList<QUrl>& files)
    {
        const int id = nextRequestId();

        ScanRequest request{.type = ScanRequest::Tracks, .id = id, .cancel = [this, id]() {
                                cancelScanRequest(id);
                            }};

        LibraryScanRequest libraryRequest;
        libraryRequest.id    = id;
        libraryRequest.type  = ScanRequest::Files;
        libraryRequest.files = files;

        m_scanRequests.emplace_front(libraryRequest);

        // File scans take precedence over library and track scans
        const auto currRequest = currentRequest();
        if(currRequest && (currRequest->type == ScanRequest::Library || currRequest->type == ScanRequest::Tracks)) {
            m_scanner.pauseThread();
            execNextRequest();
        }
        else if(m_scanRequests.size() == 1) {
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

        LibraryScanRequest libraryRequest;
        libraryRequest.id      = id;
        libraryRequest.type    = ScanRequest::Library;
        libraryRequest.library = libraryInfo;
        libraryRequest.dir     = dir;

        m_scanRequests.emplace_back(libraryRequest);

        if(m_scanRequests.size() == 1) {
            execNextRequest();
        }

        return request;
    }

    std::optional<LibraryScanRequest> currentRequest() const
    {
        const auto requestIt = std::ranges::find_if(
            m_scanRequests, [this](const auto& request) { return request.id == m_currentRequestId; });
        if(requestIt != m_scanRequests.cend()) {
            return *requestIt;
        }
        return {};
    }

    void execNextRequest()
    {
        if(m_scanRequests.empty()) {
            return;
        }

        const auto& request = m_scanRequests.front();
        m_currentRequestId  = request.id;

        switch(request.type) {
            case(ScanRequest::Files):
                scanFiles(request);
                break;
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
            std::erase_if(m_scanRequests,
                          [this](const auto& pendingRequest) { return pendingRequest.id == m_currentRequestId; });

            if(request->type == ScanRequest::Tracks) {
                // Next request (if any) will be started after tracksScanned is emitted from MusicLibrary
                return;
            }
        }

        m_currentRequestId = -1;
        execNextRequest();
    }

    void cancelScanRequest(int id)
    {
        if(m_currentRequestId == id) {
            // Will be removed in finishScanRequest
            m_scanner.stopThread();
        }
        else {
            std::erase_if(m_scanRequests, [id](const auto& request) { return request.id == id; });
        }
    }
};

LibraryThreadHandler::LibraryThreadHandler(DbConnectionPoolPtr dbPool, MusicLibrary* library,
                                           std::shared_ptr<PlaylistLoader> playlistLoader,
                                           std::shared_ptr<TagLoader> tagLoader, SettingsManager* settings,
                                           QObject* parent)
    : QObject{parent}
    , p{std::make_unique<Private>(this, std::move(dbPool), library, std::move(playlistLoader), std::move(tagLoader),
                                  settings)}
{
    QObject::connect(&p->m_trackDatabaseManager, &TrackDatabaseManager::gotTracks, this,
                     &LibraryThreadHandler::gotTracks);
    QObject::connect(&p->m_trackDatabaseManager, &TrackDatabaseManager::updatedTracks, this,
                     &LibraryThreadHandler::tracksUpdated);
    QObject::connect(&p->m_scanner, &Worker::finished, this, [this]() { p->finishScanRequest(); });
    QObject::connect(&p->m_scanner, &LibraryScanner::progressChanged, this,
                     [this](int percent) { emit progressChanged(p->m_currentRequestId, percent); });
    QObject::connect(&p->m_scanner, &LibraryScanner::scannedTracks, this,
                     [this](const TrackList& newTracks, const TrackList& existingTracks) {
                         emit scannedTracks(p->m_currentRequestId, newTracks, existingTracks);
                     });
    QObject::connect(&p->m_scanner, &LibraryScanner::statusChanged, this, &LibraryThreadHandler::statusChanged);
    QObject::connect(&p->m_scanner, &LibraryScanner::scanUpdate, this, &LibraryThreadHandler::scanUpdate);
    QObject::connect(
        &p->m_scanner, &LibraryScanner::directoryChanged, this,
        [this](const LibraryInfo& libraryInfo, const QString& dir) { p->addDirectoryScanRequest(libraryInfo, dir); });

    QMetaObject::invokeMethod(&p->m_scanner, &Worker::initialiseThread);
    QMetaObject::invokeMethod(&p->m_trackDatabaseManager, &Worker::initialiseThread);
}

LibraryThreadHandler::~LibraryThreadHandler()
{
    p->m_scanner.stopThread();
    p->m_trackDatabaseManager.stopThread();

    p->m_thread.quit();
    p->m_thread.wait();
}

void LibraryThreadHandler::getAllTracks()
{
    QMetaObject::invokeMethod(&p->m_trackDatabaseManager, &TrackDatabaseManager::getAllTracks);
}

void LibraryThreadHandler::setupWatchers(const LibraryInfoMap& libraries, bool enabled)
{
    QMetaObject::invokeMethod(&p->m_scanner,
                              [this, libraries, enabled]() { p->m_scanner.setupWatchers(libraries, enabled); });
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

ScanRequest LibraryThreadHandler::scanFiles(const QList<QUrl>& files)
{
    return p->addFilesScanRequest(files);
}

void LibraryThreadHandler::libraryRemoved(int id)
{
    if(p->m_scanRequests.empty()) {
        return;
    }

    const auto request = p->currentRequest();
    if(request && request->type == ScanRequest::Library && request->library.id == id) {
        p->m_scanner.stopThread();
    }
    else {
        std::erase_if(p->m_scanRequests, [id](const auto& pendingRequest) { return pendingRequest.library.id == id; });
    }
}

void LibraryThreadHandler::saveUpdatedTracks(const TrackList& tracks)
{
    QMetaObject::invokeMethod(&p->m_trackDatabaseManager,
                              [this, tracks]() { p->m_trackDatabaseManager.updateTracks(tracks); });
}

void LibraryThreadHandler::saveUpdatedTrackStats(const TrackList& track)
{
    QMetaObject::invokeMethod(&p->m_trackDatabaseManager,
                              [this, track]() { p->m_trackDatabaseManager.updateTrackStats(track); });
}

void LibraryThreadHandler::cleanupTracks()
{
    QMetaObject::invokeMethod(&p->m_trackDatabaseManager, &TrackDatabaseManager::cleanupTracks);
}
} // namespace Fooyin

#include "moc_librarythreadhandler.cpp"
