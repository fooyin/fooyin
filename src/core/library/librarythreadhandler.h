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

#include <QObject>

namespace Fooyin {
class AudioLoader;
class LibraryThreadHandlerPrivate;
class MusicLibrary;
class PlaylistLoader;
struct ScanProgress;
struct ScanResult;
struct ScanRequest;
class SettingsManager;

class LibraryThreadHandler : public QObject
{
    Q_OBJECT

public:
    explicit LibraryThreadHandler(DbConnectionPoolPtr dbPool, MusicLibrary* library,
                                  std::shared_ptr<PlaylistLoader> playlistLoader,
                                  std::shared_ptr<AudioLoader> audioLoader, SettingsManager* settings,
                                  QObject* parent = nullptr);
    ~LibraryThreadHandler() override;

    void getAllTracks();

    void setupWatchers(const LibraryInfoMap& libraries, bool enabled);

    ScanRequest refreshLibrary(const LibraryInfo& library);
    ScanRequest scanLibrary(const LibraryInfo& library);
    ScanRequest scanTracks(const TrackList& tracks);
    ScanRequest scanFiles(const QList<QUrl>& files);
    ScanRequest loadPlaylist(const QList<QUrl>& files);

    void saveUpdatedTracks(const TrackList& tracks);
    void writeUpdatedTracks(const TrackList& tracks);
    void saveUpdatedTrackStats(const TrackList& track);
    void cleanupTracks();

    void libraryRemoved(int id);

signals:
    void progressChanged(const Fooyin::ScanProgress& progress);
    void scannedTracks(int id, const Fooyin::TrackList& tracks);
    void playlistLoaded(int id, const Fooyin::TrackList& tracks);
    void statusChanged(const Fooyin::LibraryInfo& library);
    void scanUpdate(const Fooyin::ScanResult& result);
    void tracksUpdated(const Fooyin::TrackList& tracks);
    void tracksStatsUpdated(const Fooyin::TrackList& tracks);

    void gotTracks(const Fooyin::TrackList& result);

private:
    std::unique_ptr<LibraryThreadHandlerPrivate> p;
};
} // namespace Fooyin
