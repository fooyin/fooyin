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

#include "query.h"

#include <core/coresettings.h>
#include <utils/paths.h>
#include <utils/settings/settingsmanager.h>
#include <utils/utils.h>

#include <QFile>
#include <QSqlQuery>

static constexpr auto DatabaseVersion = "0.1.0";

namespace Fy::Core::DB {
Database::Database(Utils::SettingsManager* settings)
    : Database{Utils::sharePath(), "fooyin.db", settings}
{ }

Database::Database(const QString& directory, const QString& filename, Utils::SettingsManager* settings)
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
        qCritical() << "Database could not be initialised";
    }
    else {
        update();
    }
}

bool Database::update()
{
    if(m_settings->value<Settings::DatabaseVersion>() < DatabaseVersion) {
        m_settings->set<Settings::DatabaseVersion>(DatabaseVersion);
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
                               "    Year INTEGER,"
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
                               "    ExtraTags BLOB,"
                               "    AddedDate INTEGER,"
                               "    ModifiedDate INTEGER,"
                               "    LibraryID INTEGER REFERENCES Libraries ON DELETE CASCADE);");

    checkInsertTable("Libraries", "CREATE TABLE Libraries ("
                                  "    LibraryID INTEGER PRIMARY KEY AUTOINCREMENT,"
                                  "    Name TEXT NOT NULL UNIQUE,"
                                  "    Path TEXT NOT NULL UNIQUE);");

    checkInsertTable("Playlists", "CREATE TABLE Playlists ("
                                  "    PlaylistID INTEGER PRIMARY KEY AUTOINCREMENT,"
                                  "    Name TEXT NOT NULL UNIQUE,"
                                  "    PlaylistIndex INTEGER);");

    checkInsertTable("PlaylistTracks", "CREATE TABLE PlaylistTracks ("
                                       "    PlaylistID INTEGER NOT NULL REFERENCES Playlists ON DELETE CASCADE,"
                                       "    TrackID INTEGER NOT NULL REFERENCES Tracks ON DELETE CASCADE,"
                                       "    TrackIndex INTEGER NOT NULL);");

    checkInsertIndex("PlaylistIndex", "CREATE INDEX PlaylistIndex ON Playlists(PlaylistID,Name);");
    checkInsertIndex("PlaylistTracksIndex",
                     "CREATE INDEX PlaylistTracksIndex ON PlaylistTracks(PlaylistID,TrackIndex);");

    return true;
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
    if(!db().transaction()) {
        qDebug() << "Transaction could not be started";
    }
}

void Database::commit()
{
    if(!db().commit()) {
        qDebug() << "Transaction could not be commited";
    }
}

void Database::rollback()
{
    if(!db().rollback()) {
        qDebug() << "Transaction could not be rolled back";
    }
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
