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

#include "databasequery.h"

#include "databasemodule.h"

#include <QSqlError>

using namespace Qt::Literals::StringLiterals;

namespace Fooyin {
DatabaseQuery::DatabaseQuery(const DatabaseModule* module)
    : QSqlQuery{module->db()}
    , m_success{false}
{
    setForwardOnly(true);
}

bool DatabaseQuery::prepareQuery(const QString& query)
{
    m_queryString = query;
    return QSqlQuery::prepare(query);
}

void DatabaseQuery::bindQueryValue(const QString& placeholder, const QVariant& val, QSql::ParamType paramType)
{
    const QString replaceString = u"'"_s + val.toString() + u"'"_s;

    m_queryString.replace(placeholder + " "_L1, replaceString + " "_L1);
    m_queryString.replace(placeholder + ","_L1, replaceString + ","_L1);
    m_queryString.replace(placeholder + ";"_L1, replaceString + ";"_L1);
    m_queryString.replace(placeholder + ")"_L1, replaceString + ")"_L1);

    QSqlQuery::bindValue(placeholder, val, paramType);
}

bool DatabaseQuery::execQuery()
{
    m_success = QSqlQuery::exec();
    return m_success;
}

void DatabaseQuery::setError(bool b)
{
    m_success = (!b);
}

bool DatabaseQuery::hasError() const
{
    return !m_success;
}

void DatabaseQuery::showQuery() const
{
    qDebug() << m_queryString;
}

void DatabaseQuery::error(const QString& error) const
{
    qDebug() << "SQL ERROR: " << error << ": " << static_cast<int>(lastError().type());

    const QSqlError e = this->lastError();

    if(!e.text().isEmpty()) {
        qDebug() << e.text();
    }

    if(!e.driverText().isEmpty()) {
        qDebug() << e.driverText();
    }

    if(!e.databaseText().isEmpty()) {
        qDebug() << e.databaseText();
    }

    showQuery();
}
} // namespace Fooyin
