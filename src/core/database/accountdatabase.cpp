/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
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

#include "accountdatabase.h"

#include <utils/database/dbquery.h>

#include <QLoggingCategory>
#include <QSqlError>
#include <QSqlQuery>

Q_LOGGING_CATEGORY(ACCOUNT_DB, "fy.accounts.db")

using namespace Qt::StringLiterals;

namespace Fooyin {
bool AccountDatabase::load(const QString& account, std::optional<QString>& value) const
{
    value.reset();

    DbQuery query{db(), u"SELECT Value FROM Settings WHERE Name = :name"_s};

    query.bindValue(u":name"_s, QString{u"Accounts/"_s + account});

    if(!query.exec()) {
        qCWarning(ACCOUNT_DB) << "Unable to read account:" << account;
        return false;
    }

    if(query.next()) {
        value = query.value(0).toString();
        return true;
    }

    if(query.lastError().isValid()) {
        qCWarning(ACCOUNT_DB) << "Unable to read account:" << account;
        return false;
    }

    return true;
}

bool AccountDatabase::save(const QString& account, const QString& value) const
{
    DbQuery query{db(), u"INSERT OR REPLACE INTO Settings (Name, Value) VALUES (:name, :value)"_s};

    query.bindValue(u":name"_s, QString{u"Accounts/"_s + account});
    query.bindValue(u":value"_s, value);

    if(!query.exec()) {
        qCWarning(ACCOUNT_DB) << "Unable to save account:" << account;
        return false;
    }

    return true;
}
} // namespace Fooyin
