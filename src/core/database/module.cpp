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

#include "module.h"

#include "query.h"

#include <QApplication>
#include <QSqlError>
#include <QStringBuilder>
#include <QThread>

#include <ranges>

namespace Fy::Core::DB {
void execPragma(const QSqlDatabase& db, const QString& key, const QString& value)
{
    const auto q = QString{"PRAGMA %1 = %2;"}.arg(key, value);
    db.exec(q);
}

Module::Module(QString connectionName)
    : m_connectionName(std::move(connectionName))
{ }

QString Module::connectionName() const
{
    return m_connectionName;
}

Query Module::runQuery(const QString& query, const QString& errorText) const
{
    return runQuery(query, BindingsMap{}, errorText);
}

Query Module::runQuery(const QString& query, const std::pair<QString, QString>& bindings,
                       const QString& errorText) const
{
    return runQuery(query, {{bindings.first, bindings.second}}, errorText);
}

QSqlDatabase Module::db() const
{
    if(!QSqlDatabase::isDriverAvailable("QSQLITE")) {
        return {};
    }

    QThread* currentThread = QThread::currentThread();
    auto threadId          = reinterpret_cast<intptr_t>(currentThread);

    if(QApplication::instance() && (currentThread == QApplication::instance()->thread())) {
        threadId = 0;
    }

    const auto threadConnectionName = QString("%1-%2").arg(m_connectionName).arg(threadId);

    const QStringList connections = QSqlDatabase::connectionNames();
    if(connections.contains(threadConnectionName)) {
        return QSqlDatabase::database(threadConnectionName);
    }

    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", threadConnectionName);
    db.setDatabaseName(m_connectionName);

    if(!db.open()) {
        const QSqlError er = db.lastError();

        qDebug() << "Database cannot be opened! " << m_connectionName;
        qDebug() << er.driverText();
        qDebug() << er.databaseText();
    }

    execPragma(db, "foreign_keys", "ON");
    execPragma(db, "case_sensitive_like", "OFF");

    return db;
}

DB::Query Module::runQuery(const QString& query, const BindingsMap& bindings, const QString& errorText) const
{
    DB::Query q(this);
    q.prepareQuery(query);

    for(const auto& [key, value] : bindings) {
        q.bindQueryValue(key, value);
    }

    if(!q.execQuery()) {
        q.error(errorText);
    }

    return q;
}

DB::Query Module::insert(const QString& tableName, const BindingsMap& fieldBindings, const QString& errorMessage)
{
    QString query = "INSERT INTO " % tableName;

    DB::Query q(this);

    QString fields;
    QString values;
    for(const auto& [field, _] : fieldBindings) {
        if(!fields.isEmpty()) {
            fields = fields % ", ";
        }
        fields = fields % field;
        if(!values.isEmpty()) {
            values = values % ", ";
        }
        values = values % ":" % field;
    }
    query = query % "( " % fields % ")";
    query = query % "VALUES ( " % values % ");";

    q.prepareQuery(query);

    for(const auto& [field, value] : fieldBindings) {
        q.bindQueryValue(":" % field, value);
    }

    if(!q.execQuery()) {
        q.error(errorMessage);
    }

    return q;
}
Query Module::update(const QString& tableName, const BindingsMap& fieldBindings,
                     const std::pair<QString, QString>& whereBinding, const QString& errorMessage)
{
    QString query = "UPDATE " % tableName % " SET ";

    DB::Query q(this);

    QString fields;
    for(const auto& [field, value] : fieldBindings) {
        if(!fields.isEmpty()) {
            fields = fields % ", ";
        }
        fields = fields % field % " = :" % field;
    }
    query = query % fields;

    query = query % " WHERE ";
    query = query % whereBinding.first + " = :W" + whereBinding.first + ";";

    q.prepareQuery(query);

    for(const auto& [field, value] : fieldBindings) {
        q.bindQueryValue(":" % field, value);
    }

    q.bindQueryValue(":W" + whereBinding.first, whereBinding.second);

    if(!q.execQuery() || q.numRowsAffected() == 0) {
        q.setError(true);
        q.error(errorMessage);
    }

    return q;
}

Query Module::remove(const QString& tableName, const std::vector<std::pair<QString, QString>>& whereBinding,
                     const QString& errorMessage)
{
    QString query = "DELETE FROM " % tableName % " WHERE (";

    bool firstIteration{true};
    for(const auto& [field, binding] : whereBinding) {
        if(!firstIteration) {
            query = query % " AND ";
        }
        query          = query % field % " = " % binding;
        firstIteration = false;
    }
    query = query % ");";

    DB::Query q(this);
    q.prepareQuery(query);

    if(!q.execQuery()) {
        q.setError(true);
        q.error(errorMessage);
    }

    return q;
}

Module* Module::module()
{
    return this;
}

const Module* Module::module() const
{
    return this;
}
} // namespace Fy::Core::DB
