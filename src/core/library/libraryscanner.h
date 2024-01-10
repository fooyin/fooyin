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

#pragma once

#include <core/library/libraryinfo.h>
#include <core/trackfwd.h>
#include <utils/worker.h>

namespace Fooyin {
class Database;
class LibraryManager;
class SettingsManager;

struct ScanResult
{
    TrackList addedTracks;
    TrackList updatedTracks;
};

class LibraryScanner : public Worker
{
    Q_OBJECT

public:
    explicit LibraryScanner(Database* database, SettingsManager* settings, QObject* parent = nullptr);
    ~LibraryScanner() override;

    void closeThread() override;
    void stopThread() override;

signals:
    void progressChanged(int percent);
    void statusChanged(const LibraryInfo& library);
    void scanUpdate(const ScanResult& result);
    void scannedTracks(const TrackList& tracks);
    void directoryChanged(const LibraryInfo& library, const QString& dir);

public slots:
    void setupWatchers(const LibraryInfoMap& libraries, bool enabled);
    void scanLibrary(const LibraryInfo& library, const TrackList& tracks);
    void scanLibraryDirectory(const LibraryInfo& library, const QString& dir, const TrackList& tracks);
    void scanTracks(const TrackList& libraryTracks, const TrackList& tracks);

private:
    struct Private;
    std::unique_ptr<Private> p;
};
} // namespace Fooyin
