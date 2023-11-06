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

namespace Fy::Core {
namespace DB {
class Database;
}

namespace Library {
struct LibraryInfo;

class LibraryThreadHandler : public QObject
{
    Q_OBJECT

public:
    explicit LibraryThreadHandler(DB::Database* database, QObject* parent = nullptr);
    ~LibraryThreadHandler() override;

    void stopScanner();

    void getAllTracks();
    void scanLibrary(const Library::LibraryInfo& library, const Core::TrackList& tracks);
    void libraryRemoved(int id);
    void saveUpdatedTracks(const Core::TrackList& tracks);

signals:
    void progressChanged(int percent);
    void statusChanged(const Library::LibraryInfo& library);

    void addedTracks(const Core::TrackList& tracks);
    void updatedTracks(const Core::TrackList& tracks);
    void tracksDeleted(const Core::TrackList& tracks);

    void gotTracks(const Core::TrackList& result);

private:
    struct Private;
    std::unique_ptr<Private> p;
};
} // namespace Library
} // namespace Fy::Core
