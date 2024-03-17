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

#pragma once

#include "fyutils_export.h"

#include "dbconnection.h"

#include <QThreadStorage>

namespace Fooyin {
class DbConnectionPool;
using DbConnectionPoolPtr = std::shared_ptr<DbConnectionPool>;

class FYUTILS_EXPORT DbConnectionPool
{
    struct PrivateKey;

public:
    DbConnectionPool(PrivateKey, const DbConnection::DbParams& params, const QString& connectionName);

    DbConnectionPool(const DbConnectionPool& other)  = delete;
    DbConnectionPool(const DbConnectionPool&& other) = delete;

    static DbConnectionPoolPtr create(const DbConnection::DbParams& params, const QString& connectionName);

    [[nodiscard]] bool hasThreadConnection() const;

private:
    friend class DbConnectionProvider;
    friend class DbConnectionHandler;

    [[nodiscard]] DbConnection* threadConnection() const;
    bool createThreadConnection();
    void destroyThreadConnection();

    QThreadStorage<DbConnection*> m_threadConnections;
    std::atomic_int m_connectionCount;
    DbConnection m_prototype;
};
} // namespace Fooyin
