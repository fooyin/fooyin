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

#include <core/trackfwd.h>

#include <QObject>

namespace Fooyin {
class Database;
struct LibraryInfo;
struct ScanResult;

class LibraryThreadHandler : public QObject
{
    Q_OBJECT

public:
    explicit LibraryThreadHandler(Database* database, QObject* parent = nullptr);
    ~LibraryThreadHandler() override;

    void stopScanner();

    void getAllTracks();
    void scanLibrary(const LibraryInfo& library, const TrackList& tracks);
    void libraryRemoved(int id);
    void saveUpdatedTracks(const TrackList& tracks);

signals:
    void progressChanged(int percent);
    void statusChanged(const LibraryInfo& library);
    void scanUpdate(const ScanResult& result);
    void tracksDeleted(const TrackList& tracks);

    void gotTracks(const TrackList& result);

private:
    struct Private;
    std::unique_ptr<Private> p;
};
} // namespace Fooyin
