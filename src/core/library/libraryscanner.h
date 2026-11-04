/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <luket@pm.me>
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

#include "fycore_export.h"

#include "core/database/trackdatabase.h"
#include "libraryscantypes.h"

#include <core/library/musiclibrary.h>
#include <core/track.h>
#include <utils/database/dbconnectionpool.h>
#include <utils/worker.h>

#include <memory>
#include <unordered_map>

namespace Fooyin {
class AudioLoader;
class DbConnectionHandler;
class LibraryScanSession;
class PlaylistLoader;
class RemoteIoService;
class TrackMetadataStore;

struct FYCORE_EXPORT ScanResult
{
    TrackList addedTracks;
    TrackList updatedTracks;
};

class FYCORE_EXPORT LibraryScanner : public Worker,
                                     public LibraryScanHost
{
    Q_OBJECT

public:
    explicit LibraryScanner(DbConnectionPoolPtr dbPool, std::shared_ptr<PlaylistLoader> playlistLoader,
                            std::shared_ptr<TrackMetadataStore> metadataStore, std::shared_ptr<AudioLoader> audioLoader,
                            std::shared_ptr<RemoteIoService> remoteIo, QObject* parent = nullptr);
    ~LibraryScanner() override;

    void initialiseThread() override;
    [[nodiscard]] bool stopRequested() const override;

    void reportProgress(int current, const QString& file, int total, int phase, int discovered) override;
    void reportScanUpdate(const ScanResult& result) override;

Q_SIGNALS:
    void scanFinished(int requestId);
    void scanFailed(int requestId, const QString& error);
    void progressChanged(const Fooyin::ScanProgress& progress);
    void statusChanged(const Fooyin::LibraryInfo& library);
    void scanUpdate(int requestId, const Fooyin::ScanResult& result);
    void scannedTracks(int requestId, const Fooyin::TrackList& tracks);
    void playlistLoaded(int requestId, const Fooyin::TrackList& tracks);

public Q_SLOTS:
    void scanLibrary(const Fooyin::LibraryInfo& library, const Fooyin::TrackList& tracks, bool onlyModified,
                     const Fooyin::LibraryScanConfig& config, std::stop_token token, int requestId);
    void scanLibraryDirectoies(const Fooyin::LibraryInfo& library, const QStringList& dirs,
                               const Fooyin::TrackList& tracks, const Fooyin::LibraryScanConfig& config,
                               std::stop_token token, int requestId);
    void scanTracks(const Fooyin::TrackList& tracks, bool onlyModified, const Fooyin::LibraryScanConfig& config,
                    std::stop_token token, int requestId);
    void scanFiles(const Fooyin::TrackList& libraryTracks, const QList<QUrl>& urls,
                   const Fooyin::LibraryScanConfig& config, std::stop_token token, int requestId);
    void scanPlaylist(const Fooyin::TrackList& libraryTracks, const QList<QUrl>& urls,
                      const Fooyin::LibraryScanConfig& config, std::stop_token token, int requestId);

private:
    [[nodiscard]] ScanProgress makeProgress(int current, const QString& file, int total, ScanProgress::Phase phase,
                                            int discovered) const;
    void changeLibraryStatus(LibraryInfo::Status status);
    void startSession(const LibraryScanConfig& config, const LibraryInfo& library = {});
    void finishSession();
    void clearSession();
    bool storeScannedTracks(TrackList& tracks);

    DbConnectionPoolPtr m_dbPool;
    std::shared_ptr<PlaylistLoader> m_playlistLoader;
    std::shared_ptr<TrackMetadataStore> m_metadataStore;
    std::shared_ptr<AudioLoader> m_audioLoader;
    std::shared_ptr<RemoteIoService> m_remoteIo;

    std::unique_ptr<DbConnectionHandler> m_dbHandler;

    int m_requestId;
    bool m_progressOnlyModified;
    LibraryInfo m_currentLibrary;
    TrackDatabase m_trackDatabase;
    std::unique_ptr<LibraryScanSession> m_session;
};
} // namespace Fooyin
