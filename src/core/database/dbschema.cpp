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

#include "dbschema.h"
#include "trackdatabase.h"

#include <utils/database/dbquery.h>
#include <utils/database/dbtransaction.h>

#include <QFile>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(DB_SCHEMA, "fy.db")

using namespace Qt::StringLiterals;

constexpr auto InitialVersion = 0;

constexpr auto VersionKey          = "SchemaVersion";
constexpr auto LastVersionKey      = "LastSchemaVersion";
constexpr auto MinCompatVersionKey = "MinCompatVersion";

namespace {
int readSchemaVersion(const Fooyin::SettingsDatabase& settings, const QString& key)
{
    const QString settingsValue = settings.value(key);

    if(settingsValue.isNull()) {
        // Not yet created
        return -1;
    }

    bool isInt{false};
    const int schemaVersion = settingsValue.toInt(&isInt);

    if(!isInt || schemaVersion < InitialVersion) {
        qCCritical(DB_SCHEMA) << "Invalid DB schema version" << settingsValue;
        return -1;
    }

    return schemaVersion;
}
} // namespace

namespace Fooyin {
DbSchema::DbSchema(const DbConnectionProvider& dbProvider)
{
    DbModule::initialise(dbProvider);
    m_settingsDb.initialise(dbProvider);
}

int DbSchema::currentVersion() const
{
    int version = readSchemaVersion(m_settingsDb, QString::fromLatin1(VersionKey));

    if(version < 0) {
        version = InitialVersion;
    }

    return version;
}

int DbSchema::lastVersion() const
{
    const int lastVersion = readSchemaVersion(m_settingsDb, QString::fromLatin1(LastVersionKey));

    if(lastVersion < 0) {
        return currentVersion();
    }

    return lastVersion;
}

int DbSchema::minCompatVersion() const
{
    const int minCompatVersion = readSchemaVersion(m_settingsDb, QString::fromLatin1(MinCompatVersionKey));

    if(minCompatVersion < 0) {
        return currentVersion();
    }

    return minCompatVersion;
}

DbSchema::UpgradeResult DbSchema::upgradeDatabase(int targetVersion, const QString& schemaFilename)
{
    int currentVer = currentVersion();
    if(currentVer < InitialVersion) {
        return UpgradeResult::Failed;
    }

    const int lastVer = lastVersion();
    if(lastVer < InitialVersion) {
        return UpgradeResult::Failed;
    }

    if(lastVer > currentVer) {
        m_settingsDb.set(QString::fromLatin1(LastVersionKey), currentVer);
    }

    if(targetVersion == currentVer && lastVer >= targetVersion) {
        if(lastVer > targetVersion) {
            m_settingsDb.set(QString::fromLatin1(LastVersionKey), targetVersion);
        }
        return UpgradeResult::IsCurrent;
    }

    if(targetVersion < currentVer && targetVersion < minCompatVersion()) {
        qCWarning(DB_SCHEMA) << "Current DB schema" << currentVer << "is not backwards compatible with version"
                             << targetVersion;
        return UpgradeResult::Incompatible;
    }

    if(!readSchema(schemaFilename)) {
        return UpgradeResult::Error;
    }

    if(currentVer < targetVersion) {
        qCInfo(DB_SCHEMA) << "Upgrading DB schema from version" << currentVer << "to version" << targetVersion;
    }

    // Update views before migrations in case we've dropped columns
    TrackDatabase::dropViews(db());

    int nextVersion = lastVer;
    while(nextVersion < targetVersion) {
        ++nextVersion;

        if(!m_revisions.contains(nextVersion)) {
            qCCritical(DB_SCHEMA) << "Migration from DB version" << currentVer << "to version" << nextVersion
                                  << "is missing";
            return UpgradeResult::Error;
        }

        if(nextVersion < currentVer) {
            qCInfo(DB_SCHEMA) << "Reapplying DB schema migration to version" << nextVersion;
        }

        auto migrationResult = applyRevision(currentVer, nextVersion);

        if(migrationResult == UpgradeResult::Error) {
            qCCritical(DB_SCHEMA) << "Failed to parse DB schema migrations from" << schemaFilename;
            return UpgradeResult::Error;
        }

        if(migrationResult == UpgradeResult::Failed) {
            qCCritical(DB_SCHEMA) << "Failed to upgrade DB schema from version" << currentVer << "to version"
                                  << nextVersion;
            return UpgradeResult::Failed;
        }

        if(nextVersion > currentVer) {
            currentVer = nextVersion;
        }
    }

    if(targetVersion != lastVer) {
        m_settingsDb.set(QString::fromLatin1(LastVersionKey), targetVersion);
        TrackDatabase::insertViews(db());
    }

    if(targetVersion < currentVer) {
        qCInfo(DB_SCHEMA) << "Current DB schema is newer at version" << currentVer
                          << "and backwards compatible with version" << targetVersion;
        return UpgradeResult::BackwardsCompatible;
    }

    return UpgradeResult::Success;
}

bool DbSchema::readSchema(const QString& schemaFilename)
{
    QFile file{schemaFilename};
    if(!file.open(QFile::ReadOnly | QFile::Text)) {
        qCCritical(DB_SCHEMA) << "Failed to open DB schema file" << schemaFilename;
        return false;
    }

    m_revisions.clear();

    m_xmlReader.setDevice(&file);

    if(m_xmlReader.readNextStartElement()) {
        if(m_xmlReader.name() == "schema"_L1) {
            while(m_xmlReader.readNextStartElement()) {
                if(m_xmlReader.name() == "revision"_L1) {
                    Revision revision = readRevision();
                    m_revisions.emplace(revision.version, revision);
                }
                else {
                    m_xmlReader.skipCurrentElement();
                }
            }
        }
        else {
            m_xmlReader.raiseError(u"Incorrect DB schema file"_s);
        }
    }

    if(m_xmlReader.hasError()) {
        qCCritical(DB_SCHEMA) << "Failed to parse DB schema file" << schemaFilename << ":" << m_xmlReader.errorString();
        return false;
    }

    return true;
}

DbSchema::Revision DbSchema::readRevision()
{
    Revision revision;

    revision.version          = m_xmlReader.attributes().value(QLatin1String{"version"}).toInt();
    revision.minCompatVersion = m_xmlReader.attributes().value(QLatin1String{"minCompatVersion"}).toInt();
    revision.foreignKeys      = m_xmlReader.attributes().value(QLatin1String{"foreignKeys"}).toInt();

    while(m_xmlReader.readNextStartElement()) {
        if(m_xmlReader.name() == "description"_L1) {
            revision.description = m_xmlReader.readElementText().trimmed();
        }
        else if(m_xmlReader.name() == "sql"_L1) {
            revision.sql = m_xmlReader.readElementText().trimmed();
        }
        else {
            m_xmlReader.skipCurrentElement();
        }
    }

    return revision;
}

DbSchema::UpgradeResult DbSchema::applyRevision(int currentRevision, int revisionToApply)
{
    auto revision = m_revisions.at(revisionToApply);

    if(revision.minCompatVersion <= 0) {
        revision.minCompatVersion = revisionToApply;
    }

    if(revision.sql.isEmpty()) {
        return UpgradeResult::Error;
    }

    if(revision.foreignKeys) {
        if(!setForeignKeys(false)) {
            return UpgradeResult::Error;
        }
    }

    qCInfo(DB_SCHEMA) << "Upgrading DB schema to version" << revisionToApply << ":" << revision.description;

    DbTransaction transaction{db()};

    const QStringList statements = revision.sql.split(u";"_s);

    bool result{false};

    for(const QString& statement : statements) {
        const QString sqlStatement = statement.simplified();
        if(sqlStatement.isEmpty()) {
            continue;
        }

        DbQuery query{db(), statement};
        const auto queryStatus = query.status();

        result = queryStatus == DbQuery::Status::Prepared && query.exec();

        if(!result && queryStatus == DbQuery::Status::Ignored) {
            qCInfo(DB_SCHEMA) << "Ignoring statement" << statement << "while re-applying a DB schema migration";
            result = true;
        }
    }

    if(!result) {
        transaction.rollback();
        if(revision.foreignKeys) {
            setForeignKeys(true);
        }
        return UpgradeResult::Failed;
    }

    if(revisionToApply > currentRevision) {
        m_settingsDb.set(QString::fromLatin1(VersionKey), revisionToApply);
        m_settingsDb.set(QString::fromLatin1(LastVersionKey), revisionToApply);
        m_settingsDb.set(QString::fromLatin1(MinCompatVersionKey), revision.minCompatVersion);
        qCInfo(DB_SCHEMA) << "Upgraded DB schema to version" << revisionToApply;
    }
    else {
        qCInfo(DB_SCHEMA) << "Reapplied DB schema migration to version" << revisionToApply;
    }

    transaction.commit();

    if(revision.foreignKeys) {
        if(!setForeignKeys(true)) {
            return UpgradeResult::Error;
        }
    }

    return UpgradeResult::Success;
}

bool DbSchema::setForeignKeys(bool enabled)
{
    QSqlQuery foreignKeys{db()};
    return foreignKeys.exec(u"PRAGMA foreign_keys = %1;"_s.arg(enabled));
}
} // namespace Fooyin
