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

#include "core/library/sorting/sortorder.h"
#include "core/models/trackfwd.h"
#include "module.h"

namespace Fy::Core::DB {
class LibraryDatabase : public DB::Module
{
public:
    LibraryDatabase(const QString& connectionName, int libraryId);

    bool storeTracks(TrackList& tracksToStore);

    bool getAllTracks(TrackList& result, Core::Library::SortOrder order = Core::Library::SortOrder::NoSorting) const;
    bool getAllTracks(TrackList& result, Core::Library::SortOrder order, int start, int limit) const;

    [[nodiscard]] static QString fetchQueryTracks(const QString& where, const QString& join, const QString& order,
                                                  const QString& offsetLimit);

    static bool dbFetchTracks(Query& q, TrackList& result);

    bool updateTrack(const Track& track);
    bool deleteTrack(int id);
    bool deleteTracks(const TrackPtrList& tracks);
    bool deleteLibraryTracks(int id);

protected:
    Module* module();
    [[nodiscard]] const Module* module() const;

    bool insertArtistsAlbums(TrackList& tracks);
    int insertTrack(const Track& track);

private:
    int m_libraryId;
    QString m_connectionName;
};
} // namespace Fy::Core::DB
