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

#include "database.h"

#include "core/coresettings.h"
#include "library.h"
#include "librarydatabase.h"
#include "playlistdatabase.h"
#include "query.h"
#include "version.h"

#include <pluginsystem/pluginmanager.h>
#include <utils/paths.h>
#include <utils/utils.h>

#include <QFile>
#include <QSqlQuery>

namespace Core::DB {
struct Database::Private
{
    bool initialized{false};

    std::unique_ptr<Library> libraryConnector;
    std::unique_ptr<Playlist> playlistConnector;
    std::unique_ptr<LibraryDatabase> libraryDatabase;
};

Database::Database(const QString& directory, const QString& filename)
    : Module(directory + "/" + filename)
    , p(std::make_unique<Private>())
{
    if(!Utils::File::exists(directory)) {
        Utils::File::createDirectories(directory);
    }
    bool success = Utils::File::exists(connectionName());

    if(!success) {
        success = createDatabase();
    }
    p->initialized = success && db().isOpen();

    if(!Database::isInitialized()) {
        qDebug() << "Database could not be initialised";
    }
    else {
        p->libraryDatabase = std::make_unique<LibraryDatabase>(connectionName(), -1);
    }

    update();
}

Database::~Database() = default;

Database* Database::instance()
{
    const QString directory        = Utils::sharePath();
    const QString databaseFilename = "fooyin.db";

    static Database database(directory, databaseFilename);
    return &database;
}

LibraryDatabase* Database::libraryDatabase()
{
    return p->libraryDatabase.get();
}

void Database::deleteLibraryDatabase(int id)
{
    p->libraryDatabase->deleteLibraryTracks(id);
}

Library* Database::libraryConnector()
{
    if(!p->libraryConnector) {
        p->libraryConnector = std::make_unique<Library>(connectionName());
    }

    return p->libraryConnector.get();
}

bool Database::update()
{
    auto* settings = PluginSystem::object<SettingsManager>();
    if(settings->value<Settings::DatabaseVersion>() < DATABASE_VERSION) {
        settings->set<Settings::DatabaseVersion>(DATABASE_VERSION);
        return true;
    }
    return true;
}

bool Database::createDatabase()
{
    p->initialized = db().isOpen();
    if(!p->initialized) {
        return false;
    }

    checkInsertTable("Artists", "CREATE TABLE Artists ("
                                "    ArtistID INTEGER PRIMARY KEY AUTOINCREMENT,"
                                "    Name TEXT UNIQUE);");

    checkInsertTable("Albums", "CREATE TABLE Albums ("
                               "    AlbumID INTEGER PRIMARY KEY AUTOINCREMENT,"
                               "    Title TEXT,"
                               "    ArtistID INTEGER REFERENCES Artists,"
                               "    Year INTEGER);");

    checkInsertTable("AlbumView", "CREATE VIEW AlbumView AS"
                                  "    SELECT Albums.AlbumID,"
                                  "           Albums.Title,"
                                  "           Albums.Year,"
                                  "           Albums.ArtistID,"
                                  "           Artists.Name AS ArtistName"
                                  "    FROM Albums"
                                  "    LEFT JOIN Artists ON Artists.ArtistID = Albums.ArtistID"
                                  "    GROUP BY Albums.AlbumID");

    checkInsertTable("Tracks", "CREATE TABLE Tracks ("
                               "    TrackID INTEGER PRIMARY KEY AUTOINCREMENT,"
                               "    FilePath TEXT UNIQUE NOT NULL,"
                               "    Title TEXT,"
                               "    TrackNumber INTEGER,"
                               "    TrackTotal INTEGER,"
                               "    AlbumArtistID INTEGER REFERENCES Artists,"
                               "    AlbumID INTEGER REFERENCES Albums,"
                               "    CoverPath TEXT,"
                               "    DiscNumber INTEGER,"
                               "    DiscTotal INTEGER,"
                               "    Year INTEGER,"
                               "    Composer TEXT,"
                               "    Performer TEXT,"
                               "    Lyrics TEXT,"
                               "    Comment TEXT,"
                               "    Duration INTEGER DEFAULT 0,"
                               "    PlayCount INTEGER DEFAULT 0,"
                               "    Rating INTEGER DEFAULT 0,"
                               "    FileSize INTEGER DEFAULT 0,"
                               "    BitRate INTEGER DEFAULT 0,"
                               "    SampleRate INTEGER DEFAULT 0,"
                               "    ExtraTags TEXT,"
                               "    AddedDate INTEGER,"
                               "    ModifiedDate INTEGER,"
                               "    LibraryID INTEGER REFERENCES Libraries);");

    checkInsertTable("TrackView", "CREATE VIEW TrackView AS"
                                  "    SELECT Tracks.TrackID,"
                                  "           Tracks.FilePath,"
                                  "           Tracks.Title,"
                                  "           Tracks.TrackNumber,"
                                  "           Tracks.TrackTotal,"
                                  "           TrackArtists.ArtistIDs AS ArtistIDs,"
                                  "           TrackArtists.Artists AS Artists,"
                                  "           Tracks.AlbumArtistID,"
                                  "           Artists.Name AS AlbumArtist,"
                                  "           Tracks.AlbumID,"
                                  "           Albums.Title AS Album,"
                                  "           Tracks.CoverPath,"
                                  "           Tracks.DiscNumber,"
                                  "           Tracks.DiscTotal,"
                                  "           Tracks.Year,"
                                  "           Tracks.Composer,"
                                  "           Tracks.Performer,"
                                  "           TrackGenres.GenreIDs AS GenreIDs,"
                                  "           TrackGenres.Genres AS Genres,"
                                  "           Tracks.Lyrics,"
                                  "           Tracks.Comment,"
                                  "           Tracks.Duration,"
                                  "           Tracks.PlayCount,"
                                  "           Tracks.Rating,"
                                  "           Tracks.FileSize,"
                                  "           Tracks.BitRate,"
                                  "           Tracks.SampleRate,"
                                  "           Tracks.ExtraTags,"
                                  "           Tracks.AddedDate,"
                                  "           Tracks.ModifiedDate,"
                                  "           Tracks.LibraryID"
                                  "    FROM Tracks"
                                  "    LEFT JOIN Artists ON Artists.ArtistID = Tracks.AlbumArtistID"
                                  "    LEFT JOIN Albums ON Albums.AlbumID = Tracks.AlbumID"
                                  "    LEFT JOIN ("
                                  "           SELECT TrackID, "
                                  "                  GROUP_CONCAT(TrackArtists.ArtistID, '|') AS ArtistIDs, "
                                  "                  GROUP_CONCAT(Name, '|') AS Artists "
                                  "           FROM TrackArtists "
                                  "           LEFT JOIN Artists ON Artists.ArtistID = TrackArtists.ArtistID"
                                  "           GROUP BY TrackID) "
                                  "    AS TrackArtists ON Tracks.TrackID = TrackArtists.TrackID"
                                  "    LEFT JOIN ("
                                  "           SELECT TrackID, "
                                  "                  GROUP_CONCAT(TrackGenres.GenreID, '|') AS GenreIDs, "
                                  "                  GROUP_CONCAT(Name, '|') AS Genres "
                                  "           FROM TrackGenres "
                                  "           LEFT JOIN Genres ON Genres.GenreID = TrackGenres.GenreID"
                                  "           GROUP BY TrackID) "
                                  "    AS TrackGenres ON Tracks.TrackID = TrackGenres.TrackID"
                                  "    GROUP BY Tracks.TrackID;");

    checkInsertTable("Genres", "CREATE TABLE Genres ("
                               "    GenreID INTEGER PRIMARY KEY AUTOINCREMENT,"
                               "    Name TEXT NOT NULL UNIQUE);");

    checkInsertTable("TrackGenres", "CREATE TABLE TrackGenres ("
                                    "    TrackID INTEGER NOT NULL REFERENCES Tracks ON DELETE CASCADE,"
                                    "    GenreID INTEGER NOT NULL REFERENCES Genres ON DELETE CASCADE,"
                                    "    PRIMARY KEY (TrackID, GenreID));");

    checkInsertTable("TrackArtists", "CREATE TABLE TrackArtists ("
                                     "    TrackID INTEGER NOT NULL REFERENCES Tracks ON DELETE CASCADE,"
                                     "    ArtistID INTEGER NOT NULL REFERENCES Artists ON DELETE CASCADE,"
                                     "    PRIMARY KEY (TrackID, ArtistID));");

    checkInsertTable("Libraries", "CREATE TABLE Libraries ("
                                  "    LibraryID INTEGER PRIMARY KEY AUTOINCREMENT,"
                                  "    Name TEXT NOT NULL UNIQUE,"
                                  "    Path TEXT NOT NULL UNIQUE);");

    checkInsertTable("Playlists", "CREATE TABLE Playlists ("
                                  "    PlaylistID INTEGER PRIMARY KEY AUTOINCREMENT,"
                                  "    Name TEXT NOT NULL UNIQUE);");

    checkInsertTable("PlaylistTracks", "CREATE TABLE PlaylistTracks ("
                                       "    PlaylistID INTEGER NOT NULL REFERENCES Playlists ON DELETE CASCADE,"
                                       "    TrackID  INTEGER NOT NULL REFERENCES Tracks ON DELETE CASCADE,"
                                       "    PRIMARY KEY (PlaylistID, TrackID));");

    checkInsertIndex("AlbumIndex", "CREATE INDEX AlbumIndex ON Albums(AlbumID, Title, Year, ArtistID);");
    checkInsertIndex("GenreIndex", "CREATE INDEX GenreIndex ON Genres(GenreID,Name);");
    checkInsertIndex("TrackIndex", "CREATE INDEX TrackIndex ON Tracks(Year,AlbumArtistID,AlbumID,TrackID);");
    checkInsertIndex("PlaylistIndex", "CREATE INDEX PlaylistIndex ON Playlists(PlaylistID,Name);");
    checkInsertIndex("TrackViewIndex", "CREATE INDEX TrackViewIndex ON Tracks(TrackID,AlbumID,AlbumArtistID);");
    checkInsertIndex("TrackAlbumIndex", "CREATE INDEX TrackAlbumIndex ON Tracks(AlbumID,DiscNumber,Duration);");
    checkInsertIndex("TrackGenresIndex", "CREATE INDEX TrackGenresIndex ON TrackGenres(TrackID,GenreID);");
    checkInsertIndex("GenresTrackIndex", "CREATE INDEX GenresTrackIndex ON TrackGenres(GenreID,TrackID);");
    checkInsertIndex("TrackArtistsIndex", "CREATE INDEX TrackArtistsIndex ON TrackArtists(TrackID,ArtistID);");
    checkInsertIndex("PlaylistTracksIndex", "CREATE INDEX PlaylistTracksIndex ON PlaylistTracks(PlaylistID,TrackID);");
    checkInsertIndex("ArtistsTrackIndex", "CREATE INDEX ArtistsTrackIndex ON TrackArtists(ArtistID,TrackID);");

    return true;
}

bool Database::cleanup()
{
    Query q(this);
    QString queryText = "DELETE FROM Albums "
                        "WHERE AlbumID NOT IN "
                        "   (SELECT DISTINCT AlbumID FROM Tracks);";
    q.prepareQuery(queryText);

    if(q.execQuery()) {
        Query q2(this);
        queryText = "DELETE FROM Artists "
                    "WHERE ArtistID NOT IN "
                    "   (SELECT DISTINCT ArtistID FROM TrackArtists) "
                    "AND ArtistID NOT IN "
                    "   (SELECT DISTINCT ArtistID FROM Albums);";
        q2.prepareQuery(queryText);

        if(q2.execQuery()) {
            Query q3(this);
            queryText = "DELETE FROM Genres "
                        "WHERE GenreID NOT IN "
                        "   (SELECT DISTINCT GenreID FROM TrackGenres);";
            q3.prepareQuery(queryText);

            return q3.execQuery();
        }
    }

    return false;
}

bool Database::isInitialized()
{
    return p->initialized;
}

bool Database::closeDatabase()
{
    if(!QSqlDatabase::isDriverAvailable("QSQLITE")) {
        return false;
    }

    QString connectionName;
    {
        QSqlDatabase database             = db();
        connectionName                    = database.connectionName();
        const QStringList connectionNames = QSqlDatabase::connectionNames();
        if(!connectionNames.contains(connectionName)) {
            return false;
        }

        if(database.isOpen()) {
            database.close();
        }
    }

    QSqlDatabase::removeDatabase(connectionName);

    return true;
}

void Database::transaction()
{
    db().transaction();
}

void Database::commit()
{
    db().commit();
}

void Database::rollback()
{
    db().rollback();
}

bool Database::checkInsertTable(const QString& tableName, const QString& createString)
{
    Query q(this);
    const QString queryText = "SELECT * FROM " + tableName + ";";
    q.prepareQuery(queryText);

    if(!q.execQuery()) {
        Query q2(this);
        q2.prepareQuery(createString);

        if(!q2.exec()) {
            q.error("Cannot create table " + tableName);
            return false;
        }
    }
    return true;
}

bool Database::checkInsertIndex(const QString& indexName, const QString& createString)
{
    Query q(this);
    q.prepareQuery(createString);

    if(!q.exec()) {
        q.error("Cannot create index " + indexName);
        return false;
    }
    return true;
}
} // namespace Core::DB
