/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
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

#include <utils/database/dbquery.h>

#include <QLoggingCategory>
#include <QSqlError>

Q_LOGGING_CATEGORY(DB_QRY, "fy.db")

namespace {
bool prepareQuery(QSqlQuery& query, const QString& statement)
{
    if(query.isActive()) {
        return false;
    }

    query.setForwardOnly(true);

    return query.prepare(statement);
}
} // namespace

namespace Fooyin {
DbQuery::DbQuery()
    : m_status{Status::None}
{ }

DbQuery::DbQuery(const QSqlDatabase& database, const QString& statement)
    : m_query{database}
    , m_status{Status::None}
{
    if(prepareQuery(m_query, statement)) {
        m_status = Status::Prepared;
    }
    else if(lastError().isValid() && lastError().type() != QSqlError::NoError) {
        if(lastError().databaseText().startsWith(QStringLiteral("duplicate column name: "))) {
            // Re-applying previous migration
            m_status = Status::Ignored;
        }
        else {
            qCWarning(DB_QRY) << "Failed to prepare" << statement << ":" << lastError();
            m_status = Status::Error;
        }
    }
}

DbQuery::Status DbQuery::status() const
{
    return m_status;
}

QSqlError DbQuery::lastError() const
{
    return m_query.lastError();
}

void DbQuery::bindValue(const QString& placeholder, const QVariant& value)
{
    m_query.bindValue(placeholder, value);
}

QString DbQuery::executedQuery() const
{
    return m_query.executedQuery();
}

bool DbQuery::exec()
{
    if(m_query.exec()) {
        m_status = Status::Success;
        return true;
    }

    qCWarning(DB_QRY) << "Failed to execute" << m_query.lastQuery() << ":" << lastError();
    m_status = Status::Error;
    return false;
}

int DbQuery::numRowsAffected() const
{
    return m_query.numRowsAffected();
}

QVariant DbQuery::lastInsertId() const
{
    return m_query.lastInsertId();
}

bool DbQuery::next()
{
    return m_query.next();
}

QVariant DbQuery::value(int index) const
{
    return m_query.value(index);
}
} // namespace Fooyin
