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

#pragma once

#include "database/trackdatabase.h"
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

struct ScanResult
{
    TrackList addedTracks;
    TrackList updatedTracks;
};

class LibraryScanner : public Worker,
                       public LibraryScanHost
{
    Q_OBJECT

public:
    explicit LibraryScanner(DbConnectionPoolPtr dbPool, std::shared_ptr<PlaylistLoader> playlistLoader,
                            std::shared_ptr<AudioLoader> audioLoader, QObject* parent = nullptr);
    ~LibraryScanner() override;

    void initialiseThread() override;
    void stopThread() override;
    [[nodiscard]] bool stopRequested() const override;

    void reportProgress(int current, const QString& file, int total, int phase, int discovered) override;
    void reportScanUpdate(const ScanResult& result) override;

signals:
    void progressChanged(const Fooyin::ScanProgress& progress);
    void statusChanged(const Fooyin::LibraryInfo& library);
    void scanUpdate(const Fooyin::ScanResult& result);
    void scannedTracks(const Fooyin::TrackList& tracks);
    void playlistLoaded(const Fooyin::TrackList& tracks);

public slots:
    void scanLibrary(const Fooyin::LibraryInfo& library, const Fooyin::TrackList& tracks, bool onlyModified,
                     const Fooyin::LibraryScanConfig& config);
    void scanLibraryDirectoies(const Fooyin::LibraryInfo& library, const QStringList& dirs,
                               const Fooyin::TrackList& tracks, const Fooyin::LibraryScanConfig& config);
    void scanTracks(const Fooyin::TrackList& tracks, bool onlyModified);
    void scanFiles(const Fooyin::TrackList& libraryTracks, const QList<QUrl>& urls,
                   const Fooyin::LibraryScanConfig& config);
    void scanPlaylist(const Fooyin::TrackList& libraryTracks, const QList<QUrl>& urls,
                      const Fooyin::LibraryScanConfig& config);

private:
    [[nodiscard]] ScanProgress makeProgress(int current, const QString& file, int total, ScanProgress::Phase phase,
                                            int discovered) const;
    void changeLibraryStatus(LibraryInfo::Status status);
    void startSession(const LibraryScanConfig& config, const LibraryInfo& library = {});
    void finishSession();
    void clearSession();

    DbConnectionPoolPtr m_dbPool;
    std::shared_ptr<PlaylistLoader> m_playlistLoader;
    std::shared_ptr<AudioLoader> m_audioLoader;

    std::unique_ptr<DbConnectionHandler> m_dbHandler;

    bool m_progressOnlyModified;
    LibraryInfo m_currentLibrary;
    TrackDatabase m_trackDatabase;
    std::unique_ptr<LibraryScanSession> m_session;
};
} // namespace Fooyin
