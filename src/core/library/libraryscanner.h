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

#include <core/library/libraryinfo.h>
#include <core/track.h>
#include <utils/database/dbconnectionpool.h>
#include <utils/worker.h>

namespace Fooyin {
class AudioLoader;
class LibraryManager;
class LibraryScannerPrivate;
class PlaylistLoader;
class SettingsManager;
class TagLoader;

struct ScanResult
{
    TrackList addedTracks;
    TrackList updatedTracks;
};

class LibraryScanner : public Worker
{
    Q_OBJECT

public:
    explicit LibraryScanner(DbConnectionPoolPtr dbPool, std::shared_ptr<PlaylistLoader> playlistLoader,
                            std::shared_ptr<AudioLoader> audioLoader, SettingsManager* settings,
                            QObject* parent = nullptr);
    ~LibraryScanner() override;

    void initialiseThread() override;
    void stopThread() override;

signals:
    void progressChanged(int current, const QString& file, int total);
    void statusChanged(const Fooyin::LibraryInfo& library);
    void scanUpdate(const Fooyin::ScanResult& result);
    void scannedTracks(const Fooyin::TrackList& tracks);
    void playlistLoaded(const Fooyin::TrackList& tracks);
    void directoriesChanged(const Fooyin::LibraryInfo& library, const QStringList& dirs);

public slots:
    void setMonitorLibraries(bool enabled);
    void setupWatchers(const Fooyin::LibraryInfoMap& libraries, bool enabled);
    void scanLibrary(const Fooyin::LibraryInfo& library, const Fooyin::TrackList& tracks, bool onlyModified);
    void scanLibraryDirectoies(const Fooyin::LibraryInfo& library, const QStringList& dirs,
                               const Fooyin::TrackList& tracks);
    void scanTracks(const Fooyin::TrackList& libraryTracks, const Fooyin::TrackList& tracks, bool onlyModified);
    void scanFiles(const Fooyin::TrackList& libraryTracks, const QList<QUrl>& urls);
    void scanPlaylist(const Fooyin::TrackList& libraryTracks, const QList<QUrl>& urls);

private:
    std::unique_ptr<LibraryScannerPrivate> p;
};
} // namespace Fooyin
