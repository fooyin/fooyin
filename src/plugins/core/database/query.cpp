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

#include "query.h"

#include "module.h"

#include <QSqlError>

namespace Core::DB {
Query::Query(const Module* module)
    : QSqlQuery(module->db())
    , m_success(false)
{ }

Query::~Query()
{
    clear();
}

Query::Query(const Query& other)
    : QSqlQuery()
    , m_queryString(other.m_queryString)
    , m_success(other.m_success)
{ }

bool Query::prepareQuery(const QString& query)
{
    m_queryString = query;
    return QSqlQuery::prepare(query);
}

void Query::bindQueryValue(const QString& placeholder, const QVariant& val, QSql::ParamType paramType)
{
    const QString replaceString = QString("'") + val.toString() + "'";

    m_queryString.replace(placeholder + " ", replaceString + " ");
    m_queryString.replace(placeholder + ",", replaceString + ",");
    m_queryString.replace(placeholder + ";", replaceString + ";");
    m_queryString.replace(placeholder + ")", replaceString + ")");

    QSqlQuery::bindValue(placeholder, val, paramType);
}

bool Query::execQuery()
{
    m_success = QSqlQuery::exec();
    return m_success;
}

void Query::setError(bool b)
{
    m_success = (!b);
}

bool Query::hasError() const
{
    return !m_success;
}

void Query::showQuery() const
{
    qDebug() << m_queryString;
}

void Query::error(const QString& error) const
{
    qDebug() << "SQL ERROR: " << error << ": " << int(this->lastError().type());

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
} // namespace Core::DB
