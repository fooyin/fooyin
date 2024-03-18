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

#include "library/libraryinfo.h"

#include <core/trackfwd.h>
#include <utils/database/dbconnectionpool.h>

#include <QObject>

namespace Fooyin {
class SettingsManager;
class MusicLibrary;
struct ScanResult;
struct ScanRequest;

class LibraryThreadHandler : public QObject
{
    Q_OBJECT

public:
    explicit LibraryThreadHandler(DbConnectionPoolPtr dbPool, MusicLibrary* library, SettingsManager* settings,
                                  QObject* parent = nullptr);
    ~LibraryThreadHandler() override;

    void getAllTracks();

    void setupWatchers(const LibraryInfoMap& libraries, bool enabled);

    ScanRequest scanLibrary(const LibraryInfo& library);
    ScanRequest scanTracks(const TrackList& tracks);

    void saveUpdatedTracks(const TrackList& tracks);
    void saveUpdatedTrackStats(const Track& track);
    void cleanupTracks();

    void libraryRemoved(int id);

signals:
    void progressChanged(int id, int percent);
    void scannedTracks(int id, const TrackList& tracks);
    void statusChanged(const LibraryInfo& library);
    void scanUpdate(const ScanResult& result);
    void tracksUpdated(const TrackList& tracks);

    void gotTracks(const TrackList& result);

private:
    struct Private;
    std::unique_ptr<Private> p;
};
} // namespace Fooyin
