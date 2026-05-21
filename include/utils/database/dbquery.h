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

#pragma once

#include "fyutils_export.h"

#include <QSqlQuery>

namespace Fooyin {
class FYUTILS_EXPORT DbQuery
{
public:
    enum class Status
    {
        None,
        Prepared,
        Success,
        Ignored,
        Error,
    };

    DbQuery();
    DbQuery(const QSqlDatabase& database, const QString& statement);

    DbQuery(const DbQuery& other)       = delete;
    DbQuery(DbQuery&& other)            = default;
    DbQuery& operator=(DbQuery&& other) = default;

    [[nodiscard]] Status status() const;
    [[nodiscard]] QSqlError lastError() const;

    void bindValue(const QString& placeholder, const QVariant& value);
    [[nodiscard]] QString executedQuery() const;
    bool exec();

    [[nodiscard]] int numRowsAffected() const;
    [[nodiscard]] QVariant lastInsertId() const;
    [[nodiscard]] bool next();
    [[nodiscard]] QVariant value(int index) const;

private:
    QSqlQuery m_query;
    Status m_status;
};
} // namespace Fooyin
