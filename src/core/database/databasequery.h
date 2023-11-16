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

#include <QSqlQuery>

namespace Fooyin {
class DatabaseModule;

class DatabaseQuery : public QSqlQuery
{
public:
    explicit DatabaseQuery(const DatabaseModule* module);

    explicit DatabaseQuery(QSqlResult* result)                           = delete;
    explicit DatabaseQuery(const QString& query, const QSqlDatabase& db) = delete;
    DatabaseQuery(const DatabaseQuery& other)                            = delete;

    DatabaseQuery(DatabaseQuery&& other) noexcept = default;

    bool prepareQuery(const QString& query);
    void bindQueryValue(const QString& placeholder, const QVariant& val, QSql::ParamType paramType = QSql::In);
    bool execQuery();
    void setError(bool b);
    [[nodiscard]] bool hasError() const;

    void showQuery() const;
    void error(const QString& error) const;

private:
    QString m_queryString;
    bool m_success;
};
} // namespace Fooyin
