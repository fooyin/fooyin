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
#include <QThread>

namespace Core::DB {
Module::Module(QString connectionName)
    : m_connectionName(std::move(connectionName))
{ }

QString Module::connectionName() const
{
    return m_connectionName;
}

static void execPragma(const QSqlDatabase& db, const QString& key, const QString& value)
{
    const QString q = QString("PRAGMA %1 = %2;").arg(key, value);
    db.exec(q);
}

QSqlDatabase Module::db() const
{
    if(!QSqlDatabase::isDriverAvailable("QSQLITE")) {
        return {};
    }

    QThread* t = QThread::currentThread();

    auto id = quint64(t);
    if(QApplication::instance() && (t == QApplication::instance()->thread())) {
        id = 0;
    }

    const QString threadConnectionName = QString("%1-%2").arg(m_connectionName).arg(id);

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

DB::Query Module::runQuery(const QString& query, const QString& errorText) const
{
    return runQuery(query, QMap<QString, QVariant>(), errorText);
}

DB::Query Module::runQuery(const QString& query, const QPair<QString, QVariant>& bindings,
                           const QString& errorText) const
{
    return runQuery(query, {{bindings.first, bindings.second}}, errorText);
}

DB::Query Module::runQuery(const QString& query, const QMap<QString, QVariant>& bindings,
                           const QString& errorText) const
{
    DB::Query q(this);
    q.prepareQuery(query);

    const QList<QString> keys = bindings.keys();
    for(const auto& k : keys) {
        q.bindQueryValue(k, bindings[k]);
    }

    if(!q.execQuery()) {
        q.error(errorText);
    }

    return q;
}

DB::Query Module::insert(const QString& tableName, const QMap<QString, QVariant>& fieldBindings,
                         const QString& errorMessage)
{
    const QStringList fieldNames = fieldBindings.keys();
    const QString fields         = fieldNames.join(", ");
    const QString bindings       = ":" + fieldNames.join(", :");

    QString query = "INSERT INTO " + tableName;
    query += " (" + fields + ") ";
    query += "VALUES (" + bindings + ");";

    DB::Query q(this);
    q.prepareQuery(query);

    for(const auto& field : fieldNames) {
        q.bindQueryValue(":" + field, fieldBindings[field]);
    }

    if(!q.execQuery()) {
        q.error(errorMessage);
    }

    return q;
}

DB::Query Module::update(const QString& tableName, const QMap<QString, QVariant>& fieldBindings,
                         const QPair<QString, QVariant>& whereBinding, const QString& errorMessage)
{
    const QStringList fieldNames = fieldBindings.keys();

    QStringList updateCommands;
    for(const auto& field : fieldNames) {
        updateCommands << field + " = :" + field;
    }

    QString query = "UPDATE " + tableName + " SET ";
    query += updateCommands.join(", ");
    query += " WHERE ";
    query += whereBinding.first + " = :W" + whereBinding.first + ";";

    DB::Query q(this);
    q.prepareQuery(query);

    for(const auto& field : fieldNames) {
        q.bindQueryValue(":" + field, fieldBindings.value(field));
    }

    q.bindQueryValue(":W" + whereBinding.first, whereBinding.second);

    if(!q.execQuery() || q.numRowsAffected() == 0) {
        q.setError(true);
        q.error(errorMessage);
    }

    return q;
}

Query Module::remove(const QString& tableName, const QList<QPair<QString, QVariant>>& whereBinding,
                     const QString& errorMessage)
{
    QString query = "DELETE FROM " + tableName;
    query += " WHERE (";
    int i = static_cast<int>(whereBinding.size());
    for(const auto& binding : whereBinding) {
        --i;
        query += binding.first + " = " + binding.second.toString();
        query += i != 0 ? " AND " : "";
    }
    query += ");";

    DB::Query q(this);
    q.prepareQuery(query);

    if(!q.execQuery() || q.numRowsAffected() == 0) {
        q.setError(true);
        q.error(errorMessage);
    }

    return q;
}
} // namespace Core::DB
