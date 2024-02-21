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

#include "databasemodule.h"

#include "databasequery.h"

#include <QApplication>
#include <QSqlError>
#include <QThread>

namespace Fooyin {
DatabaseModule::DatabaseModule(QString connectionName)
    : m_connectionName{std::move(connectionName)}
{ }

QString DatabaseModule::connectionName() const
{
    return m_connectionName;
}

DatabaseQuery DatabaseModule::runQuery(const QString& query, const QString& errorText) const
{
    return runQuery(query, BindingsMap{}, errorText);
}

QSqlDatabase DatabaseModule::db() const
{
    if(!QSqlDatabase::isDriverAvailable(QStringLiteral("QSQLITE"))) {
        return {};
    }

    QThread* currentThread = QThread::currentThread();
    auto threadId          = reinterpret_cast<intptr_t>(currentThread);

    if(QApplication::instance() && (currentThread == QApplication::instance()->thread())) {
        threadId = 0;
    }

    const auto threadConnectionName = QString(QStringLiteral("%1-%2")).arg(m_connectionName).arg(threadId);

    const QStringList connections = QSqlDatabase::connectionNames();
    if(connections.contains(threadConnectionName)) {
        return QSqlDatabase::database(threadConnectionName);
    }

    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), threadConnectionName);
    db.setDatabaseName(m_connectionName);

    if(!db.open()) {
        const QSqlError er = db.lastError();

        qDebug() << "Database cannot be opened: " << m_connectionName;
        qDebug() << er.driverText();
        qDebug() << er.databaseText();
    }

    runPragma(QStringLiteral("foreign_keys"), QStringLiteral("ON"));

    return db;
}

DatabaseQuery DatabaseModule::runQuery(const QString& query, const BindingsMap& bindings,
                                       const QString& errorText) const
{
    DatabaseQuery q(this);
    q.prepareQuery(query);

    for(const auto& [key, value] : bindings) {
        q.bindQueryValue(key, value);
    }

    if(!q.execQuery()) {
        q.error(errorText);
    }

    return q;
}

DatabaseQuery DatabaseModule::insert(const QString& tableName, const BindingsMap& fieldBindings,
                                     const QString& errorMessage)
{
    QString query = QStringLiteral("INSERT INTO ") + tableName;

    DatabaseQuery q(this);

    QString fields;
    QString values;
    for(const auto& [field, _] : fieldBindings) {
        if(!fields.isEmpty()) {
            fields += QStringLiteral(", ");
        }
        fields = fields + field;
        if(!values.isEmpty()) {
            values += QStringLiteral(", ");
        }
        values += QStringLiteral(":") + field;
    }
    query += QString{QStringLiteral("(%1) VALUES (%2)")}.arg(fields, values);

    q.prepareQuery(query);

    for(const auto& [field, value] : fieldBindings) {
        q.bindQueryValue(QStringLiteral(":") + field, value);
    }

    if(!q.execQuery()) {
        q.error(errorMessage);
    }

    return q;
}

void DatabaseModule::runPragma(const QString& pragma, const QString& value) const
{
    DatabaseQuery q(this);

    const auto query = QString{QStringLiteral("PRAGMA %1 = %2")}.arg(pragma, value);
    q.prepareQuery(query);

    if(!q.execQuery()) {
        q.error(QString{QStringLiteral("Could not set pragma '%1' to '%2'")}.arg(pragma, value));
    }
}

DatabaseQuery DatabaseModule::update(const QString& tableName, const BindingsMap& fieldBindings,
                                     const std::pair<QString, QString>& whereBinding, const QString& errorMessage)
{
    auto query = QString{QStringLiteral("UPDATE %1 SET ")}.arg(tableName);

    DatabaseQuery q(this);

    QString fields;
    for(const auto& [field, value] : fieldBindings) {
        if(!fields.isEmpty()) {
            fields += QStringLiteral(", ");
        }
        fields += field + QStringLiteral(" = :") + field;
    }
    query += fields;

    query += QString{QStringLiteral(" WHERE %1 = :W%1;")}.arg(whereBinding.first);

    q.prepareQuery(query);

    for(const auto& [field, value] : fieldBindings) {
        q.bindQueryValue(QStringLiteral(":") + field, value);
    }

    q.bindQueryValue(QStringLiteral(":W") + whereBinding.first, whereBinding.second);

    if(!q.execQuery() || q.numRowsAffected() == 0) {
        q.setError(true);
        q.error(errorMessage);
    }

    return q;
}

DatabaseQuery DatabaseModule::remove(const QString& tableName,
                                     const std::vector<std::pair<QString, QString>>& whereBinding,
                                     const QString& errorMessage)
{
    auto query = QString{QStringLiteral("DELETE FROM %1 WHERE (")}.arg(tableName);

    bool firstIteration{true};
    for(const auto& [field, binding] : whereBinding) {
        if(!firstIteration) {
            query += QStringLiteral(" AND ");
        }
        query += field + QStringLiteral(" = ") + binding;
        firstIteration = false;
    }
    query += QStringLiteral(");");

    DatabaseQuery q(this);
    q.prepareQuery(query);

    if(!q.execQuery()) {
        q.setError(true);
        q.error(errorMessage);
    }

    return q;
}
} // namespace Fooyin
