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

#include "database.h"

#include "dbschema.h"

#include <core/coresettings.h>
#include <utils/fileutils.h>
#include <utils/paths.h>
#include <utils/settings/settingsmanager.h>

#include <QFileInfo>

constexpr auto CurrentSchemaVersion = 7;

namespace {
Fooyin::DbConnection::DbParams dbConnectionParams()
{
    Fooyin::DbConnection::DbParams params;
    params.type           = QStringLiteral("QSQLITE");
    params.connectOptions = QStringLiteral("QSQLITE_OPEN_URI");
    params.filePath       = Fooyin::Utils::sharePath() + QStringLiteral("/fooyin.db");

    return params;
}
} // namespace

namespace Fooyin {
Database::Database(QObject* parent)
    : QObject{parent}
    , m_dbPool(DbConnectionPool::create(dbConnectionParams(), QStringLiteral("fooyin")))
    , m_connectionHandler{m_dbPool}
    , m_status{Status::Ok}
{
    if(!m_connectionHandler.hasConnection()) {
        changeStatus(Status::ConnectionError);
        return;
    }

    initSchema();
}

DbConnectionPoolPtr Database::connectionPool() const
{
    return m_dbPool;
}

Database::Status Database::status() const
{
    return m_status;
}

bool Database::initSchema()
{
    const DbConnectionProvider dbProvider{m_dbPool};
    DbSchema schema{dbProvider};

    const auto upgradeResult = schema.upgradeDatabase(CurrentSchemaVersion, QStringLiteral("://dbschema.xml"));

    switch(upgradeResult) {
        case(DbSchema::UpgradeResult::Success):
        case(DbSchema::UpgradeResult::IsCurrent):
        case(DbSchema::UpgradeResult::BackwardsCompatible):
            changeStatus(Status::Ok);
            return true;
        case(DbSchema::UpgradeResult::Error):
            changeStatus(Status::SchemaError);
            return false;
        case(DbSchema::UpgradeResult::Failed):
            changeStatus(Status::DbError);
            return false;
        case(DbSchema::UpgradeResult::Incompatible):
            changeStatus(Status::Incompatible);
            return false;
        default:
            return false;
    }
}

void Database::changeStatus(Status status)
{
    m_status = status;
    emit statusChanged(status);
}
} // namespace Fooyin
