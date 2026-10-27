/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <luket@pm.me>
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

#include <utils/database/dbquery.h>

#include <QLoggingCategory>

#include <atomic>
#include <cstdint>

Q_LOGGING_CATEGORY(DB_TRANSACTION, "fy.db")

using namespace Qt::StringLiterals;

namespace Fooyin {
namespace {
QString nextSavepointName()
{
    static std::atomic_uint64_t nextId{0};
    return u"fooyin_transaction_%1"_s.arg(static_cast<qulonglong>(nextId.fetch_add(1, std::memory_order_relaxed)));
}
} // namespace

DbTransaction::DbTransaction(const QSqlDatabase& database)
    : m_database{database}
    , m_savepoint{nextSavepointName()}
    , m_isActive{execute(u"SAVEPOINT %1"_s.arg(m_savepoint))}
{ }

DbTransaction::~DbTransaction()
{
    if(m_isActive) {
        rollback();
    }
}

DbTransaction::DbTransaction(DbTransaction&& other) noexcept
    : m_database{other.m_database}
    , m_savepoint{std::move(other.m_savepoint)}
    , m_lastError{std::move(other.m_lastError)}
    , m_isActive{std::exchange(other.m_isActive, false)}
{ }

DbTransaction::operator bool() const
{
    return m_isActive;
}

QSqlError DbTransaction::lastError() const
{
    return m_lastError;
}

bool DbTransaction::commit()
{
    if(!checkActive() || !execute(u"RELEASE SAVEPOINT %1"_s.arg(m_savepoint))) {
        return false;
    }

    m_isActive = false;
    return true;
}

bool DbTransaction::rollback()
{
    if(!checkActive() || !execute(u"ROLLBACK TO SAVEPOINT %1"_s.arg(m_savepoint))
       || !execute(u"RELEASE SAVEPOINT %1"_s.arg(m_savepoint))) {
        return false;
    }

    m_isActive = false;
    return true;
}

bool DbTransaction::execute(const QString& statement)
{
    if(!m_database.isOpen()) {
        m_lastError = QSqlError{u"No open database connection"_s, {}, QSqlError::ConnectionError};
        qCWarning(DB_TRANSACTION) << "Failed to execute" << statement << "on" << m_database.connectionName()
                                  << m_lastError;
        return false;
    }

    DbQuery query{m_database, statement};
    if(query.status() != DbQuery::Status::Prepared || !query.exec()) {
        m_lastError = query.lastError();
        return false;
    }

    m_lastError = {};
    return true;
}

bool DbTransaction::checkActive()
{
    if(!m_isActive) {
        m_lastError = QSqlError{u"Transaction is not active"_s, {}, QSqlError::TransactionError};
        return false;
    }

    return true;
}
} // namespace Fooyin
