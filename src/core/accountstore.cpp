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

#include <core/accountstore.h>

#include "database/accountdatabase.h"

#include <core/coresettings.h>

#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(ACCOUNT_STORE, "fy.accounts")

using namespace Qt::StringLiterals;

namespace Fooyin {
AccountStore::AccountStore(DbConnectionPoolPtr pool)
    : m_database{std::make_unique<AccountDatabase>()}
{
    m_database->initialise(DbConnectionProvider{std::move(pool)});
}

AccountStore::~AccountStore() = default;

std::optional<AccountStore::Values> AccountStore::load(const QString& account, const std::vector<QString>& fields) const
{
    std::optional<QString> storedValue;
    if(!m_database->load(account, storedValue)) {
        return {};
    }

    Values values;
    if(storedValue) {
        const auto document = QJsonDocument::fromJson(storedValue->toUtf8());
        if(!document.isObject()) {
            qCWarning(ACCOUNT_STORE) << "Invalid database account:" << account;
            return {};
        }

        const auto object = document.object();
        for(const auto& field : fields) {
            const auto value = object.value(field);
            if(!value.isUndefined() && !value.isString()) {
                qCWarning(ACCOUNT_STORE) << "Invalid database account field:" << account << field;
                return {};
            }
            values.emplace(field, value.toString());
        }

        removeLegacyValues(account, fields);
        return values;
    }

    const FySettings legacy;

    bool hasLegacyValues{false};
    for(const auto& field : fields) {
        const QString key = account + '/'_L1 + field;
        hasLegacyValues |= legacy.contains(key);
        values.emplace(field, legacy.value(key).toString());
    }
    if(!hasLegacyValues) {
        return Values{};
    }

    if(!save(account, values)) {
        // Keep the imported values usable in memory even when persistence fails
        qCWarning(ACCOUNT_STORE) << "Account migration failed; retaining legacy config:" << account;
    }

    return values;
}

bool AccountStore::save(const QString& account, const Values& values) const
{
    QJsonObject object;
    std::vector<QString> fields;

    for(const auto& [field, value] : values) {
        object.insert(field, value);
        fields.emplace_back(field);
    }

    if(!m_database->save(account, QString::fromUtf8(QJsonDocument{object}.toJson(QJsonDocument::Compact)))) {
        return false;
    }

    return removeLegacyValues(account, fields);
}

bool AccountStore::remove(const QString& account, const std::vector<QString>& fields) const
{
    // Retain an empty account so leftover config credentials can't be imported again
    Values values;
    for(const auto& field : fields) {
        values.emplace(field, QString{});
    }
    return save(account, values);
}

bool AccountStore::removeLegacyValues(const QString& account, const std::vector<QString>& fields) const
{
    FySettings legacy;

    bool removedValues{false};
    for(const auto& field : fields) {
        const QString key = account + '/'_L1 + field;
        if(legacy.contains(key)) {
            legacy.remove(key);
            removedValues = true;
        }
    }

    if(removedValues) {
        legacy.sync();
    }
    return true;
}
} // namespace Fooyin
