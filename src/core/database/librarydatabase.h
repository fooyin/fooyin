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
    bool getAllAlbums(AlbumList& result) const;
    bool getAllArtists(ArtistHash& result) const;
    bool getAllGenres(GenreHash& result) const;

    [[nodiscard]] static QString fetchQueryTracks(const QString& where, const QString& join, const QString& order,
                                                  const QString& offsetLimit);
    [[nodiscard]] static QString fetchQueryAlbums(const QString& where, const QString& join);
    [[nodiscard]] static QString fetchQueryArtists(const QString& where);
    [[nodiscard]] static QString fetchQueryGenres(const QString& where);

    static bool dbFetchTracks(Query& q, TrackList& result);
    static bool dbFetchAlbums(Query& q, AlbumList& result);
    static bool dbFetchArtists(Query& q, ArtistHash& result);
    static bool dbFetchGenres(Query& q, GenreHash& result);

    bool updateTrack(const Track& track);
    bool deleteTrack(int id);
    bool deleteTracks(const TrackPtrList& tracks);
    bool deleteLibraryTracks(int id);

protected:
    Module* module();
    [[nodiscard]] const Module* module() const;

    bool insertArtistsAlbums(TrackList& tracks);
    int insertArtist(const Artist& artist);
    int insertAlbum(const Album& album);
    bool insertTrackArtists(int id, const IdSet& artists);
    bool insertTrackGenres(int id, const IdSet& genres);
    bool updateTrackArtists(int id, const IdSet& artists);
    bool updateTrackGenres(int id, const IdSet& genres);
    int insertGenre(const QString& genre);
    int insertTrack(const Track& track);

private:
    int m_libraryId;
    QString m_connectionName;
};
} // namespace Fy::Core::DB
