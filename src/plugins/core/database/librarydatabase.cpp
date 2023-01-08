/*
 * Fooyin
 * Copyright 2022, Luke Taylor <LukeT1@proton.me>
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

#include "librarydatabase.h"

#include "core/library/libraryutils.h"
#include "query.h"

#include <QBuffer>
#include <QPixmap>
#include <utils/helpers.h>
#include <utils/utils.h>

namespace Core::DB {
QMap<QString, QVariant> getTrackBindings(const Track& track)
{
    return QMap<QString, QVariant>{
        {QStringLiteral("FilePath"), Util::File::cleanPath(track.filepath())},
        {QStringLiteral("Title"), track.title()},
        {QStringLiteral("TrackNumber"), track.trackNumber()},
        {QStringLiteral("TrackTotal"), track.trackTotal()},
        {QStringLiteral("AlbumArtistID"), track.albumArtistId()},
        {QStringLiteral("AlbumID"), track.albumId()},
        {QStringLiteral("CoverPath"), track.coverPath()},
        {QStringLiteral("DiscNumber"), track.discNumber()},
        {QStringLiteral("DiscTotal"), track.discTotal()},
        {QStringLiteral("Year"), track.year()},
        {QStringLiteral("Composer"), track.composer()},
        {QStringLiteral("Performer"), track.performer()},
        {QStringLiteral("Lyrics"), track.lyrics()},
        {QStringLiteral("Comment"), track.comment()},
        {QStringLiteral("Duration"), track.duration()},
        {QStringLiteral("FileSize"), track.fileSize()},
        {QStringLiteral("BitRate"), track.bitrate()},
        {QStringLiteral("SampleRate"), track.sampleRate()},
        {QStringLiteral("ExtraTags"), track.extraTagsToJson()},
        {QStringLiteral("AddedDate"), track.addedTime()},
        {QStringLiteral("ModifiedDate"), track.mTime()},
        {QStringLiteral("LibraryID"), track.libraryId()},
    };
}

LibraryDatabase::LibraryDatabase(const QString& connectionName, int libraryId)
    : DB::Module(connectionName)
    , m_libraryId(libraryId)
    , m_connectionName(connectionName)
{ }

LibraryDatabase::~LibraryDatabase() = default;

bool LibraryDatabase::insertArtistsAlbums(TrackList& tracks)
{
    if(tracks.empty()) {
        return {};
    }

    // Gather all albums
    QHash<QString, Album> albumMap;
    {
        AlbumList dbAlbums;
        getAllAlbums(dbAlbums);

        for(const auto& album : qAsConst(dbAlbums)) {
            QString hash = Library::Utils::calcAlbumHash(album.title(), album.artist(), album.year());
            albumMap.insert(hash, album);
        }
    }

    // Gather all artists
    QHash<QString, Artist> artistMap;
    {
        ArtistHash dbArtists;
        getAllArtists(dbArtists);

        for(const auto& [id, artist] : dbArtists) {
            artistMap.emplace(artist.name(), artist);
        }
    }

    // Gather all genres
    QHash<QString, int> genreMap;
    {
        GenreHash dbGenres;
        getAllGenres(dbGenres);

        for(const auto& [id, name] : dbGenres) {
            genreMap.insert(name, id);
        }
    }

    // Gather all tracks
    QHash<QString, Track> trackMap;
    {
        TrackList dbTracks;
        getAllTracks(dbTracks);

        for(const auto& track : qAsConst(dbTracks)) {
            trackMap.insert(track.filepath(), track);
        }
    }

    db().transaction();

    for(auto& track : tracks) {
        if(track.libraryId() < 0) {
            track.setLibraryId(m_libraryId);
        }

        // Check artists
        for(const auto& trackArtist : track.artists()) {
            if(!artistMap.contains(trackArtist)) {
                Artist artist{trackArtist};
                int id = insertArtist(artist);
                artist.setId(id);
                artistMap.insert(trackArtist, artist);
            }
            auto artist = artistMap.value(trackArtist);
            track.addArtistId(artist.id());
        }

        // Check album artist
        if(!artistMap.contains(track.albumArtist())) {
            Artist albumArtist{track.albumArtist()};
            int id = insertArtist(albumArtist);
            albumArtist.setId(id);
            artistMap.insert(track.albumArtist(), albumArtist);
        }
        auto albumArtist = artistMap.value(track.albumArtist());
        track.setAlbumArtistId(albumArtist.id());

        // Check genres
        for(const auto& genre : track.genres()) {
            if(!genreMap.contains(genre)) {
                int id = insertGenre(genre);
                genreMap.insert(genre, id);
            }
            int trackGenre = genreMap.value(genre);
            track.addGenreId(trackGenre);
        }

        // Check album id
        QString hash = Library::Utils::calcAlbumHash(track.album(), track.albumArtist(), track.year());
        if(!albumMap.contains(hash)) {
            Album album{track.album()};
            album.setYear(track.year());
            album.setGenres(track.genres());
            album.setArtistId(albumArtist.id());
            album.setArtist(track.albumArtist());

            int id = insertAlbum(album);
            album.setId(id);
            albumMap.insert(hash, album);
        }
        auto album = albumMap.value(hash);
        track.setAlbumId(album.id());
        if(!album.hasCover()) {
            QString coverPath = Library::Utils::storeCover(track);
            album.setCoverPath(coverPath);
        }
        track.setCoverPath(album.coverPath());

        // Check track id
        if(trackMap.contains(track.filepath())) {
            int id = trackMap.value(track.filepath()).id();
            track.setId(id);
        }

        db().commit();
    }
    return true;
}

bool LibraryDatabase::storeTracks(TrackList& tracks)
{
    if(tracks.empty()) {
        return true;
    }

    insertArtistsAlbums(tracks);

    db().transaction();

    for(auto& track : tracks) {
        if(track.id() >= 0) {
            updateTrack(track);
            //            updateTrackArtists(track.id(), track.artistIds());
            //            updateTrackGenres(track.id(), track.genreIds());
        }
        else {
            int id = insertTrack(track);
            track.setId(id);
            insertTrackArtists(id, track.artistIds());
            insertTrackGenres(id, track.genreIds());
        }
    }

    return db().commit();
}

bool LibraryDatabase::getAllTracks(TrackList& result) const
{
    auto q = Query(module());
    const auto query = fetchQueryTracks({}, {});
    q.prepareQuery(query);

    return dbFetchTracks(q, result);
}

bool LibraryDatabase::getAllAlbums(AlbumList& result) const
{
    const auto query = QString("%1 GROUP BY AlbumView.AlbumID, AlbumView.Title;").arg(fetchQueryAlbums({}, {}));

    auto q = Query(module());
    q.prepareQuery(query);

    return dbFetchAlbums(q, result);
}

bool LibraryDatabase::getAllArtists(ArtistHash& result) const
{
    const auto queryText = QString("%1 GROUP BY Artists.ArtistID, Artists.Name;").arg(fetchQueryArtists(""));

    auto query = Query(module());
    query.prepareQuery(queryText);

    return dbFetchArtists(query, result);
}

bool LibraryDatabase::getAllGenres(GenreHash& result) const
{
    const auto queryText = QString("%1;").arg(fetchQueryGenres(""));

    auto query = Query(module());
    query.prepareQuery(queryText);

    return dbFetchGenres(query, result);
}

QString LibraryDatabase::fetchQueryTracks(const QString& where, const QString& join)
{
    static const auto fields = QStringList{
        QStringLiteral("TrackID"),       // 0
        QStringLiteral("FilePath"),      // 1
        QStringLiteral("Title"),         // 2
        QStringLiteral("TrackNumber"),   // 3
        QStringLiteral("TrackTotal"),    // 4
        QStringLiteral("ArtistIDs"),     // 5
        QStringLiteral("Artists"),       // 6
        QStringLiteral("AlbumArtistID"), // 7
        QStringLiteral("AlbumArtist"),   // 8
        QStringLiteral("AlbumID"),       // 9
        QStringLiteral("Album"),         // 10
        QStringLiteral("CoverPath"),     // 11
        QStringLiteral("DiscNumber"),    // 12
        QStringLiteral("DiscTotal"),     // 13
        QStringLiteral("Year"),          // 14
        QStringLiteral("Composer"),      // 15
        QStringLiteral("Performer"),     // 16
        QStringLiteral("GenreIDs"),      // 17
        QStringLiteral("Genres"),        // 18
        QStringLiteral("Lyrics"),        // 19
        QStringLiteral("Comment"),       // 20
        QStringLiteral("Duration"),      // 21
        QStringLiteral("PlayCount"),     // 22
        QStringLiteral("FileSize"),      // 23
        QStringLiteral("BitRate"),       // 24
        QStringLiteral("SampleRate"),    // 25
        QStringLiteral("ExtraTags"),     // 26
        QStringLiteral("AddedDate"),     // 27
        QStringLiteral("ModifiedDate"),  // 28
        QStringLiteral("LibraryID"),     // 29
    };

    const auto joinedFields = fields.join(", ");

    return QString("SELECT %1 FROM TrackView %2 WHERE %3;")
        .arg(joinedFields, join.isEmpty() ? "" : join, where.isEmpty() ? "1" : where);
}

QString LibraryDatabase::fetchQueryAlbums(const QString& where, const QString& join)
{
    static const auto fields = QStringList{
        QStringLiteral("AlbumView.AlbumID"),    // 0
        QStringLiteral("AlbumView.Title"),      // 1
        QStringLiteral("AlbumView.Year"),       // 2
        QStringLiteral("AlbumView.ArtistID"),   // 3
        QStringLiteral("AlbumView.ArtistName"), // 4
    };

    const auto joinedFields = fields.join(", ");

    auto query = QString("SELECT %1 FROM AlbumView %2 WHERE %3")
                     .arg(joinedFields, join.isEmpty() ? "" : join, where.isEmpty() ? "1" : where);

    return query;
}

QString LibraryDatabase::fetchQueryArtists(const QString& where)
{
    static const auto fields = QStringList{
        QStringLiteral("Artists.ArtistID"), // 0
        QStringLiteral("Artists.Name"),     // 1
    };

    const auto joinedFields = fields.join(", ");

    auto queryText = QString("SELECT %1 FROM Artists WHERE %2").arg(joinedFields, where.isEmpty() ? "1" : where);

    return queryText;
}

QString LibraryDatabase::fetchQueryGenres(const QString& where)
{
    static const auto fields = QStringList{
        QStringLiteral("Genres.GenreID"), // 0
        QStringLiteral("Genres.Name"),    // 1
    };

    const auto joinedFields = fields.join(", ");

    auto queryText = QString("SELECT %1 FROM Genres WHERE %2").arg(joinedFields, where.isEmpty() ? "1" : where);

    return queryText;
}

bool LibraryDatabase::dbFetchTracks(Query& q, TrackList& result)
{
    result.clear();

    if(!q.execQuery()) {
        q.error("Cannot fetch tracks from database");
        return false;
    }

    while(q.next()) {
        Track track{q.value(1).toString()};

        track.setId(q.value(0).toInt());
        track.setTitle(q.value(2).toString());
        track.setTrackNumber(q.value(3).toInt());
        track.setTrackTotal(q.value(4).toInt());
        const QStringList artistIds = q.value(5).toString().split("|");
        track.setArtists(q.value(6).toString().split("|", Qt::SkipEmptyParts));
        track.setAlbumArtistId(q.value(7).toInt());
        track.setAlbumArtist(q.value(8).toString());
        track.setAlbumId(q.value(9).toInt());
        track.setAlbum(q.value(10).toString());
        track.setCoverPath(q.value(11).toString());
        track.setDiscNumber(q.value(12).toInt());
        track.setDiscTotal(q.value(13).toInt());
        track.setYear(q.value(14).toInt());
        track.setComposer(q.value(15).toString());
        track.setPerformer(q.value(16).toString());
        const QStringList genreIds = q.value(17).toString().split("|");
        track.setGenres(q.value(18).toString().split("|", Qt::SkipEmptyParts));
        track.setLyrics(q.value(19).toString());
        track.setComment(q.value(20).toString());
        track.setDuration(q.value(21).value<quint64>());
        track.setPlayCount(q.value(22).toInt());
        track.setFileSize(q.value(23).toInt());
        track.setBitrate(q.value(24).toInt());
        track.setSampleRate(q.value(25).toInt());
        track.jsonToExtraTags(q.value(26).toByteArray());
        track.setAddedTime(static_cast<qint64>(q.value(27).toULongLong()));
        track.setMTime(static_cast<qint64>(q.value(28).toULongLong()));
        track.setLibraryId(q.value(29).toInt());

        for(const auto& id : artistIds) {
            track.addArtistId(id.toInt());
        }

        for(const auto& id : genreIds) {
            track.addGenreId(id.toInt());
        }

        result.emplace_back(track);
    }

    return true;
}

bool LibraryDatabase::dbFetchAlbums(Query& q, AlbumList& result)
{
    result.clear();

    if(!q.execQuery()) {
        q.error("Could not get all albums from database");
        return false;
    }

    while(q.next()) {
        Album album{q.value(1).toString()};

        album.setId(q.value(0).value<int>());
        album.setYear(q.value(2).value<int>());
        album.setArtistId(q.value(3).value<int>());
        album.setArtist(q.value(4).toString());
        album.setGenres(q.value(5).toString().split("|"));
        album.setDiscCount(q.value(6).value<int>());
        album.setTrackCount(q.value(7).value<int>());
        album.setDuration(q.value(8).value<quint64>());
        album.setCoverPath(q.value(9).toString());

        result.emplace_back(album);
    }

    return true;
}

bool LibraryDatabase::dbFetchArtists(Query& q, ArtistHash& result)
{
    result.clear();

    if(!q.execQuery()) {
        q.error("Could not get all artists from database");
        return false;
    }

    while(q.next()) {
        Artist artist{q.value(1).toString()};
        artist.setId(q.value(0).toInt());

        result.emplace(artist.id(), artist);
    }

    return true;
}

bool LibraryDatabase::dbFetchGenres(Query& q, GenreHash& result)
{
    result.clear();

    if(!q.execQuery()) {
        q.error("Could not get all genres from database");
        return false;
    }

    while(q.next()) {
        auto id = q.value(0).toInt();
        auto genre = q.value(1).toString();

        result.emplace(id, genre);
    }

    return true;
}

bool LibraryDatabase::updateTrack(const Track& track)
{
    if(track.id() < 0 || track.albumId() < 0 || track.albumArtistId() < 0 || track.libraryId() < 0) {
        qDebug() << "Cannot update track (value negative): "
                 << " AlbumArtistID: " << track.albumArtistId() << " AlbumID: " << track.albumId()
                 << " TrackID: " << track.id() << " LibraryID: " << track.libraryId();
        return false;
    }

    auto bindings = getTrackBindings(track);

    const auto q = module()->update("Tracks", bindings, {"TrackID", track.id()},
                                    QString("Cannot update track %1").arg(track.filepath()));

    if(!q.hasError()) {
        return (updateTrackArtists(track.id(), track.artistIds()) && updateTrackGenres(track.id(), track.genreIds()));
    }
    return false;
}

bool LibraryDatabase::deleteTrack(int id)
{
    const auto queryText = QStringLiteral("DELETE FROM Tracks WHERE TrackID = :TrackID;");
    const auto q = module()->runQuery(queryText, {":TrackID", id}, QString("Cannot delete track %1").arg(id));

    return (!q.hasError());
}

bool LibraryDatabase::deleteTracks(const IdSet& tracks)
{
    if(tracks.empty()) {
        return true;
    }

    module()->db().transaction();

    const int fileCount = static_cast<int>(std::count_if(tracks.cbegin(), tracks.cend(), [&](int trackId) {
        return deleteTrack(trackId);
    }));

    const auto success = module()->db().commit();

    return (success && (fileCount == static_cast<int>(tracks.size())));
}

bool LibraryDatabase::deleteLibraryTracks(int id)
{
    if(id < 0) {
        return false;
    }

    const auto query = QStringLiteral("DELETE FROM Tracks WHERE LibraryID=:libraryId;");
    const auto q = module()->runQuery(query, {":libraryId", id}, "Cannot delete library tracks");
    return (!q.hasError());
}

DB::Module* LibraryDatabase::module()
{
    return this;
}

const DB::Module* LibraryDatabase::module() const
{
    return this;
}

int LibraryDatabase::insertArtist(const Artist& artist)
{
    const auto bindings = QMap<QString, QVariant>{
        {"Name", artist.name()},
    };

    auto query = module()->insert("Artists", bindings, QString("Cannot insert artist %1").arg(artist.name()));
    return (query.hasError()) ? -1 : query.lastInsertId().toInt();
}

int LibraryDatabase::insertAlbum(const Album& album)
{
    const auto bindings
        = QMap<QString, QVariant>{{"Title", album.title()}, {"ArtistID", album.artistId()}, {"Year", album.year()}};

    const auto q = module()->insert("Albums", bindings, QString("Cannot insert album %1").arg(album.title()));

    return (!q.hasError()) ? q.lastInsertId().toInt() : -1;
}

bool LibraryDatabase::insertTrackArtists(int id, const IdSet& artists)
{
    for(const auto& artist : artists) {
        const auto bindings = QMap<QString, QVariant>{{"TrackID", id}, {"ArtistID", artist}};

        const auto q = module()->insert("TrackArtists", bindings, QString("Cannot insert track artist %1").arg(artist));
        if(!q.hasError()) {
            continue;
        }
        return false;
    }
    return true;
}

bool LibraryDatabase::insertTrackGenres(int id, const IdSet& genres)
{
    for(const auto& genre : genres) {
        const auto bindings = QMap<QString, QVariant>{{"TrackID", id}, {"GenreID", genre}};

        const auto q = module()->insert("TrackGenres", bindings, QString("Cannot insert track genre %1").arg(genre));
        if(!q.hasError()) {
            continue;
        }
        return false;
    }
    return true;
}

bool LibraryDatabase::updateTrackArtists(int id, const IdSet& artists)
{
    const auto query = QString("SELECT ArtistID FROM TrackArtists WHERE TrackID = %1;").arg(id);

    auto q = Query(module());
    q.prepareQuery(query);

    if(!q.execQuery()) {
        q.error("Could not get all track artists from database");
        return false;
    }

    IdSet databaseArtists;
    IdSet artistsToDelete;
    IdSet artistsToInsert;

    // Gather track artists in database
    if(!q.hasError()) {
        while(q.next()) {
            int artistId = q.value(0).toInt();
            databaseArtists.insert(artistId);
        }
    }

    // Remove artists not in track
    for(auto artistId : databaseArtists) {
        if(!contains(artists, artistId)) {
            artistsToDelete.insert(artistId);
        }
    }

    // Insert new artists
    for(auto artistId : artists) {
        if(!contains(databaseArtists, artistId)) {
            artistsToInsert.insert(artistId);
        }
    }

    if(!artistsToDelete.empty()) {
        for(auto artistId : artistsToDelete) {
            const auto bindings = QList<QPair<QString, QVariant>>{{"TrackID", id}, {"ArtistID", artistId}};

            const auto q2
                = module()->remove("TrackArtists", bindings, QString("Cannot remove track artist %1").arg(artistId));
            if(!q2.hasError()) {
                continue;
            }
            return false;
        }
    }

    for(auto artist : artistsToInsert) {
        const auto bindings = QMap<QString, QVariant>{{"TrackID", id}, {"ArtistID", artist}};

        const auto q2
            = module()->insert("TrackArtists", bindings, QString("Cannot insert track artist %1").arg(artist));
        if(!q2.hasError()) {
            continue;
        }
        return false;
    }
    return true;
}

bool LibraryDatabase::updateTrackGenres(int id, const IdSet& genres)
{
    const auto query = QString("SELECT GenreID FROM TrackGenres WHERE TrackID = %1;").arg(id);

    auto q = Query(module());
    q.prepareQuery(query);

    if(!q.execQuery()) {
        q.error("Could not get all track genres from database");
        return false;
    }

    IdSet databaseGenres;
    IdSet genresToDelete;
    IdSet genresToInsert;

    if(!q.hasError()) {
        while(q.next()) {
            int genreId = q.value(0).toInt();
            databaseGenres.insert(genreId);
        }
    }

    for(auto genreId : databaseGenres) {
        if(!contains(genres, genreId)) {
            genresToDelete.insert(genreId);
        }
    }

    for(auto genreId : genres) {
        if(!contains(databaseGenres, genreId)) {
            genresToInsert.insert(genreId);
        }
    }

    if(!genresToDelete.empty()) {
        for(auto genreId : genresToDelete) {
            const auto bindings = QList<QPair<QString, QVariant>>{{"TrackID", id}, {"GenreID", genreId}};

            const auto q2
                = module()->remove("TrackGenres", bindings, QString("Cannot remove track genre %1").arg(genreId));
            if(!q2.hasError()) {
                continue;
            }
            return false;
        }
    }

    for(auto genre : genresToInsert) {
        const auto bindings = QMap<QString, QVariant>{{"TrackID", id}, {"GenreID", genre}};

        const auto q3 = module()->insert("TrackGenres", bindings, QString("Cannot insert track genre %1").arg(genre));
        if(!q3.hasError()) {
            continue;
        }
        return false;
    }
    return true;
}

int LibraryDatabase::insertGenre(const QString& genre)
{
    const auto bindings = QMap<QString, QVariant>{
        {"Name", genre},
    };

    auto query = module()->insert("Genres", bindings, QString("Cannot insert genre %1").arg(genre));
    return (query.hasError()) ? -1 : query.lastInsertId().toInt();
}

int LibraryDatabase::insertTrack(const Track& track)
{
    auto bindings = getTrackBindings(track);

    const auto query = module()->insert("Tracks", bindings, QString("Cannot insert track %1").arg(track.filepath()));

    return (query.hasError()) ? -1 : query.lastInsertId().toInt();
}
} // namespace Core::DB
