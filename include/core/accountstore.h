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

#pragma once

#include "fycore_export.h"

#include <utils/database/dbconnectionpool.h>

#include <map>
#include <memory>
#include <optional>
#include <vector>

namespace Fooyin {
class AccountDatabase;

// Account data is stored unencrypted in the database
// TODO: Add native credential backends (e.g. libsecret), retaining the database as a fallback
class FYCORE_EXPORT AccountStore
{
public:
    using Values = std::map<QString, QString>;

    explicit AccountStore(DbConnectionPoolPtr pool);
    ~AccountStore();

    /** Imports legacy config fields when no database account exists. An error leaves the config intact. */
    [[nodiscard]] std::optional<Values> load(const QString& account, const std::vector<QString>& fields) const;
    [[nodiscard]] bool save(const QString& account, const Values& values) const;
    [[nodiscard]] bool remove(const QString& account, const std::vector<QString>& fields) const;

private:
    bool removeLegacyValues(const QString& account, const std::vector<QString>& fields) const;

    std::unique_ptr<AccountDatabase> m_database;
};
} // namespace Fooyin
