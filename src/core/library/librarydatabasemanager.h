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

#include "core/models/trackfwd.h"

#include <utils/worker.h>

namespace Fy::Core {

namespace DB {
class Database;
class LibraryDatabase;
} // namespace DB

namespace Library {
class LibraryDatabaseManager : public Utils::Worker
{
    Q_OBJECT

public:
    explicit LibraryDatabaseManager(DB::Database* database, QObject* parent = nullptr);
    ~LibraryDatabaseManager() override;

    void getAllTracks();
    void updateTracks(const TrackPtrList& tracks);

signals:
    void gotTracks(const Core::TrackList& result);

private:
    DB::Database* m_database;
    DB::LibraryDatabase* m_libraryDatabase;
};
} // namespace Library
} // namespace Core
