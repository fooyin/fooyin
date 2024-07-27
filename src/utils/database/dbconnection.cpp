/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <LukeT1@proton.me>
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

#include <utils/database/dbconnection.h>

#include <QDebug>
#include <QLoggingCategory>
#include <QSqlError>

Q_LOGGING_CATEGORY(DB_CON, "DB")

namespace {
void createDatabase(const Fooyin::DbConnection::DbParams& params, const QString& connectionName)
{
    QSqlDatabase database = QSqlDatabase::addDatabase(params.type, connectionName);
    database.setConnectOptions(params.connectOptions);
    database.setHostName(params.hostName);
    database.setDatabaseName(params.filePath);
}

void cloneDatabase(const Fooyin::DbConnection& original, const QString& connectionName)
{
    QSqlDatabase::cloneDatabase(original.name(), connectionName);
}
} // namespace

namespace Fooyin {
DbConnection::DbConnection(const DbParams& params, const QString& connectionName)
    : m_name{connectionName}
{
    createDatabase(params, connectionName);
}

DbConnection::DbConnection(const DbConnection& original, const QString& connectionName)
    : m_name{connectionName}
{
    cloneDatabase(original, connectionName);
}

DbConnection::~DbConnection()
{
    close();
    QSqlDatabase::removeDatabase(m_name);
}

QString DbConnection::name() const
{
    return m_name;
}

bool DbConnection::open()
{
    auto db = this->db();

    if(!db.open()) {
        qCWarning(DB_CON) << "Failed to open connection:" << m_name << db.lastError();
        return false;
    }

    return true;
}

void DbConnection::close()
{
    auto db = this->db();
    if(db.isOpen()) {
        if(db.rollback()) {
            qCWarning(DB_CON) << "Rolled back open transaction before closing connection:" << m_name;
        }
        db.close();
    }
}

bool DbConnection::isOpen() const
{
    return db().isOpen();
}

QSqlDatabase DbConnection::db() const
{
    return QSqlDatabase::database(m_name);
}
} // namespace Fooyin
