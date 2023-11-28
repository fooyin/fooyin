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

#include "databasequery.h"

#include <core/coresettings.h>
#include <utils/paths.h>
#include <utils/settings/settingsmanager.h>
#include <utils/utils.h>

#include <QFile>

using namespace Qt::Literals::StringLiterals;

static constexpr auto DatabaseVersion = "0.1.0";

namespace Fooyin {
Database::Database(SettingsManager* settings)
    : Database{Utils::sharePath(), u"fooyin.db"_s, settings}
{ }

Database::Database(const QString& directory, const QString& filename, SettingsManager* settings)
    : DatabaseModule{directory + "/" + filename}
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
    if(m_settings->value<Settings::Core::DatabaseVersion>() < DatabaseVersion) {
        m_settings->set<Settings::Core::DatabaseVersion>(DatabaseVersion);
        return true;
    }
    return false;
}

bool Database::createDatabase()
{
    m_initialized = db().isOpen();
    if(!m_initialized) {
        return false;
    }

    checkInsertTable(u"Tracks"_s, u"CREATE TABLE Tracks ("
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
                                  "    Type INTEGER DEFAULT 0,"
                                  "    AddedDate INTEGER,"
                                  "    ModifiedDate INTEGER,"
                                  "    LibraryID INTEGER REFERENCES Libraries ON DELETE CASCADE);"_s);

    checkInsertTable(u"Libraries"_s, u"CREATE TABLE Libraries ("
                                     "    LibraryID INTEGER PRIMARY KEY AUTOINCREMENT,"
                                     "    Name TEXT NOT NULL UNIQUE,"
                                     "    Path TEXT NOT NULL UNIQUE);"_s);

    checkInsertTable(u"Playlists"_s, u"CREATE TABLE Playlists ("
                                     "    PlaylistID INTEGER PRIMARY KEY AUTOINCREMENT,"
                                     "    Name TEXT NOT NULL UNIQUE,"
                                     "    PlaylistIndex INTEGER);"_s);

    checkInsertTable(u"PlaylistTracks"_s, u"CREATE TABLE PlaylistTracks ("
                                          "    PlaylistID INTEGER NOT NULL REFERENCES Playlists ON DELETE CASCADE,"
                                          "    TrackID INTEGER NOT NULL REFERENCES Tracks ON DELETE CASCADE,"
                                          "    TrackIndex INTEGER NOT NULL);"_s);

    checkInsertIndex(u"PlaylistIndex"_s, u"CREATE INDEX PlaylistIndex ON Playlists(PlaylistID,Name);"_s);
    checkInsertIndex(u"PlaylistTracksIndex"_s,
                     u"CREATE INDEX PlaylistTracksIndex ON PlaylistTracks(PlaylistID,TrackIndex);"_s);

    module()->insert(u"Libraries"_s, {{u"LibraryID"_s, u"0"_s}, {u"Name"_s, u"No Library"_s}, {u"Path"_s, u""_s}},
                     "Could not insert default library");

    return true;
}

bool Database::isInitialized()
{
    return m_initialized;
}

bool Database::closeDatabase()
{
    if(!QSqlDatabase::isDriverAvailable(u"QSQLITE"_s)) {
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
    DatabaseQuery q(this);
    const QString queryText = "SELECT * FROM " + tableName + ";";
    q.prepareQuery(queryText);

    if(!q.execQuery()) {
        DatabaseQuery q2(this);
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
    DatabaseQuery q(this);
    q.prepareQuery(createString);

    if(!q.execQuery()) {
        q.error("Cannot create index " + indexName);
        return false;
    }
    return true;
}
} // namespace Fooyin
