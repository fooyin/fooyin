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

#include "database.h"

#include "core/coresettings.h"
#include "library.h"
#include "librarydatabase.h"
#include "playlistdatabase.h"
#include "query.h"
#include "version.h"

#include <utils/settings/settingsmanager.h>
#include <utils/utils.h>

#include <QFile>
#include <QSqlQuery>

namespace Fy::Core::DB {
Database::Database(Utils::SettingsManager* settings, const QString& directory, const QString& filename)
    : Module{directory + "/" + filename}
    , m_settings{settings}
{
    if(!Utils::File::exists(directory)) {
        Utils::File::createDirectories(directory);
    }
    bool success = Utils::File::exists(connectionName());

    if(!success) {
        success = createDatabase();
    }
    m_initialized = success && db().isOpen();

    if(!Database::isInitialized()) {
        qDebug() << "Database could not be initialised";
    }
    else {
        m_libraryDatabase = std::make_unique<LibraryDatabase>(connectionName(), -1);
    }

    update();
}

Database::~Database() = default;

LibraryDatabase* Database::libraryDatabase()
{
    return m_libraryDatabase.get();
}

void Database::deleteLibraryDatabase(int id)
{
    m_libraryDatabase->deleteLibraryTracks(id);
}

Library* Database::libraryConnector()
{
    if(!m_libraryConnector) {
        m_libraryConnector = std::make_unique<Library>(connectionName());
    }

    return m_libraryConnector.get();
}

bool Database::update()
{
    if(m_settings->value<Settings::DatabaseVersion>() < DATABASE_VERSION) {
        m_settings->set<Settings::DatabaseVersion>(DATABASE_VERSION);
        return true;
    }
    return true;
}

bool Database::createDatabase()
{
    m_initialized = db().isOpen();
    if(!m_initialized) {
        return false;
    }

    checkInsertTable("Tracks", "CREATE TABLE Tracks ("
                               "    TrackID INTEGER PRIMARY KEY AUTOINCREMENT,"
                               "    FilePath TEXT UNIQUE NOT NULL,"
                               "    Title TEXT,"
                               "    TrackNumber INTEGER,"
                               "    TrackTotal INTEGER,"
                               "    Artists TEXT,"
                               "    AlbumArtist TEXT,"
                               "    Album TEXT,"
                               "    CoverPath TEXT,"
                               "    DiscNumber INTEGER,"
                               "    DiscTotal INTEGER,"
                               "    Date TEXT,"
                               "    Composer TEXT,"
                               "    Performer TEXT,"
                               "    Genres TEXT,"
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

    checkInsertIndex("PlaylistIndex", "CREATE INDEX PlaylistIndex ON Playlists(PlaylistID,Name);");
    checkInsertIndex("PlaylistTracksIndex", "CREATE INDEX PlaylistTracksIndex ON PlaylistTracks(PlaylistID,TrackID);");

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
    return m_initialized;
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

        if(!q2.execQuery()) {
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

    if(!q.execQuery()) {
        q.error("Cannot create index " + indexName);
        return false;
    }
    return true;
}
} // namespace Fy::Core::DB
