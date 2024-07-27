/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <LukeT1@proton.me>
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

#include "settingsdatabase.h"

#include <QLoggingCategory>
#include <QSqlQuery>

namespace Fooyin {
QString SettingsDatabase::value(const QString& name, QString defaultValue) const
{
    const auto statement = QStringLiteral("SELECT Value FROM Settings WHERE Name = :name");

    QSqlQuery query{db()};

    if(!query.prepare(statement)) {
        return defaultValue;
    }

    query.bindValue(QStringLiteral(":name"), name);

    if(query.exec() && query.next()) {
        const QVariant value = query.value(0);
        if(!value.isValid()) {
            QLoggingCategory log{"Settings"};
            qCWarning(log) << "Invalid value:" << value;
        }
        else {
            return value.toString();
        }
    }

    return defaultValue;
}

bool SettingsDatabase::set(const QString& name, const QVariant& value) const
{
    if(!value.canConvert<QString>()) {
        return false;
    }

    const auto statement = QStringLiteral("INSERT OR REPLACE INTO Settings (Name, Value) VALUES (:name, :value)");

    QSqlQuery query{db()};

    if(!query.prepare(statement)) {
        return false;
    }

    query.bindValue(QStringLiteral(":name"), name);
    query.bindValue(QStringLiteral(":value"), value.toString());

    return query.exec();
}
} // namespace Fooyin
