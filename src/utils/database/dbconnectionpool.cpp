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

#include <utils/database/dbconnectionpool.h>

#include <QLoggingCategory>
#include <QSqlQuery>

Q_LOGGING_CATEGORY(DB_POOL, "fy.db")

using namespace Qt::StringLiterals;

namespace {
bool updatePragmas(Fooyin::DbConnection* connection)
{
    QSqlQuery foreignKeys{connection->db()};
    if(!foreignKeys.exec(u"PRAGMA foreign_keys = ON;"_s)) {
        return false;
    }

    return true;
}
} // namespace

namespace Fooyin {
struct DbConnectionPool::PrivateKey
{
    PrivateKey() { }
    PrivateKey(PrivateKey const&) = default;
};

DbConnectionPool::DbConnectionPool(PrivateKey /*key*/, const DbConnection::DbParams& params,
                                   const QString& connectionName)
    : m_connectionCount{0}
    , m_prototype{params, connectionName}
{ }

DbConnectionPoolPtr DbConnectionPool::create(const DbConnection::DbParams& params, const QString& connectionName)
{
    return std::make_shared<DbConnectionPool>(PrivateKey{}, params, connectionName);
}

bool DbConnectionPool::hasThreadConnection() const
{
    return m_threadConnections.hasLocalData();
}

bool DbConnectionPool::createThreadConnection()
{
    if(m_threadConnections.hasLocalData()) {
        qCWarning(DB_POOL) << "Thread connection already exists:" << m_threadConnections.localData()->name();
        return false;
    }

    const int connectionIndex = m_connectionCount.fetch_add(1, std::memory_order_acquire) + 1;
    const auto connectionName = u"%1-%2"_s.arg(m_prototype.name()).arg(connectionIndex);
    auto connection           = std::make_unique<DbConnection>(m_prototype, connectionName);

    if(!connection->open()) {
        qCWarning(DB_POOL) << "Failed to open thread connection:" << connectionName;
        return false;
    }

    if(!updatePragmas(connection.get())) {
        qCWarning(DB_POOL) << "Failed to set pragmas:" << connectionName;
        return false;
    }

    m_threadConnections.setLocalData(connection.release());

    return true;
}

void DbConnectionPool::destroyThreadConnection()
{
    if(!m_threadConnections.hasLocalData()) {
        qCWarning(DB_POOL) << "Thread connection not found";
    }

    m_threadConnections.setLocalData(nullptr);
}

DbConnection* DbConnectionPool::threadConnection() const
{
    return m_threadConnections.localData();
}
} // namespace Fooyin
