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
#include <utils/fypaths.h>
#include <utils/settings/settingsmanager.h>

#include <QFileInfo>

using namespace Qt::StringLiterals;

constexpr auto CurrentSchemaVersion = 14;

namespace {
Fooyin::DbConnection::DbParams dbConnectionParams()
{
    Fooyin::DbConnection::DbParams params;
    params.type           = u"QSQLITE"_s;
    params.connectOptions = u"QSQLITE_OPEN_URI"_s;
    params.filePath       = Fooyin::Utils::sharePath() + u"/fooyin.db"_s;

    return params;
}
} // namespace

namespace Fooyin {
Database::Database(QObject* parent)
    : QObject{parent}
    , m_dbPool(DbConnectionPool::create(dbConnectionParams(), u"fooyin"_s))
    , m_connectionHandler{m_dbPool}
    , m_status{Status::Ok}
    , m_previousRevision{0}
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

int Database::currentRevision() const
{
    const DbConnectionProvider dbProvider{m_dbPool};
    const DbSchema schema{dbProvider};
    return schema.currentVersion();
}

int Database::previousRevision() const
{
    return m_previousRevision;
}

bool Database::initSchema()
{
    const DbConnectionProvider dbProvider{m_dbPool};
    DbSchema schema{dbProvider};
    m_previousRevision = schema.currentVersion();

    const auto upgradeResult = schema.upgradeDatabase(CurrentSchemaVersion, u"://dbschema.xml"_s);

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
