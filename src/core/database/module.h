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

#pragma once

#include <QSqlDatabase>
#include <QVariant>

namespace Core::DB {
class Query;
class Module
{
public:
    explicit Module(QString connectionName);
    virtual ~Module() = default;

    [[nodiscard]] QSqlDatabase db() const;
    [[nodiscard]] QString connectionName() const;

    [[nodiscard]] DB::Query runQuery(const QString& query, const QString& errorText) const;
    [[nodiscard]] DB::Query runQuery(const QString& query, const QPair<QString, QVariant>& bindings,
                                     const QString& errorText) const;
    [[nodiscard]] DB::Query runQuery(const QString& query, const QMap<QString, QVariant>& bindings,
                                     const QString& errorText) const;

    DB::Query update(const QString& tableName, const QMap<QString, QVariant>& fieldBindings,
                     const QPair<QString, QVariant>& whereBinding, const QString& errorMessage);
    DB::Query remove(const QString& tableName, const QList<QPair<QString, QVariant>>& whereBinding,
                     const QString& errorMessage);
    DB::Query insert(const QString& tableName, const QMap<QString, QVariant>& fieldBindings,
                     const QString& errorMessage);

private:
    QString m_connectionName;
};
} // namespace Core::DB
