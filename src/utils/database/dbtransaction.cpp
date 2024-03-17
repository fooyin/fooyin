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

#include <utils/database/dbtransaction.h>

namespace {
bool beginTransaction(QSqlDatabase& database)
{
    if(!database.isOpen()) {
        qWarning() << "[DB] Failed to begin transaction on" << database.connectionName() << ": No open connection";
        return false;
    }

    if(!database.transaction()) {
        qWarning() << "[DB] Failed to begin transaction on" << database.connectionName();
        return false;
    }

    return true;
}
} // namespace

namespace Fooyin {
DbTransaction::DbTransaction(const QSqlDatabase& database)
    : m_database(database)
    , m_isActive(beginTransaction(m_database))
{ }

DbTransaction::~DbTransaction()
{
    if(m_database.isOpen() && m_isActive) {
        rollback();
    }
}

DbTransaction::DbTransaction(DbTransaction&& other) noexcept
    : m_database(std::move(other.m_database))
    , m_isActive(other.m_isActive)
{
    other.release();
}

DbTransaction::operator bool() const
{
    return m_isActive;
}

bool DbTransaction::commit()
{
    if(!m_database.isOpen()) {
        qWarning() << "[DB] Failed to commit transaction on" << m_database.connectionName() << ": No open connection";
        return false;
    }

    if(!m_database.commit()) {
        qWarning() << "[DB] Failed to commit transaction on" << m_database.connectionName();
        return false;
    }

    release();
    return true;
}

bool DbTransaction::rollback()
{
    if(!m_database.isOpen()) {
        qWarning() << "[DB] Failed to rollback transaction on" << m_database.connectionName() << ": No open connection";
        return false;
    }

    if(!m_database.rollback()) {
        qWarning() << "[DB] Failed to rollback transaction on" << m_database.connectionName();
        return false;
    }

    release();
    return false;
}

void DbTransaction::release()
{
    m_isActive = false;
}
} // namespace Fooyin
