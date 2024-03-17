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
        qCritical() << "[DB] Invalid schema version" << settingsValue;
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
        qWarning() << "[DB] Current schema" << currentVer << "is not backwards compatable with version"
                   << targetVersion;
        return UpgradeResult::Incompatible;
    }

    if(!readSchema(schemaFilename)) {
        return UpgradeResult::Error;
    }

    if(currentVer < targetVersion) {
        qInfo() << "[DB] Upgrading schema from version" << currentVer << "to version" << targetVersion;
    }

    int nextVersion = lastVer;
    while(nextVersion < targetVersion) {
        ++nextVersion;

        if(!m_revisions.contains(nextVersion)) {
            qCritical() << "[DB] Migration from version" << currentVer << "to version" << nextVersion << "is missing";
            return UpgradeResult::Error;
        }

        if(nextVersion < currentVer) {
            qInfo() << "[DB] Reapplying schema migration to version" << nextVersion;
        }

        auto migrationResult = applyRevision(currentVer, nextVersion);

        if(migrationResult == UpgradeResult::Error) {
            qCritical() << "[DB] Failed to parse database schema migrations from" << schemaFilename;
            return UpgradeResult::Error;
        }

        if(migrationResult == UpgradeResult::Failed) {
            qCritical() << "[DB] Failed to upgrade schema from version" << currentVer << "to version" << nextVersion;
            return UpgradeResult::Failed;
        }

        if(nextVersion > currentVer) {
            currentVer = nextVersion;
        }
    }

    if(targetVersion != lastVer) {
        m_settingsDb.set(QString::fromLatin1(LastVersionKey), targetVersion);
        TrackDatabase::updateViews(db());
    }

    if(targetVersion < currentVer) {
        qInfo() << "[DB] Current schema is newer at version" << currentVer << "and backwards compatible with version"
                << targetVersion;
        return UpgradeResult::BackwardsCompatible;
    }

    return UpgradeResult::Success;
}

bool DbSchema::readSchema(const QString& schemaFilename)
{
    QFile file{schemaFilename};
    if(!file.open(QFile::ReadOnly | QFile::Text)) {
        qCritical() << "[DB] Failed to open schema file" << schemaFilename;
        return false;
    }

    m_revisions.clear();

    m_xmlReader.setDevice(&file);

    if(m_xmlReader.readNextStartElement()) {
        if(m_xmlReader.name() == u"schema") {
            while(m_xmlReader.readNextStartElement()) {
                if(m_xmlReader.name() == u"revision") {
                    Revision revision = readRevision();
                    m_revisions.emplace(revision.version, revision);
                }
                else {
                    m_xmlReader.skipCurrentElement();
                }
            }
        }
        else {
            m_xmlReader.raiseError(QStringLiteral("[DB] Incorrect schema file"));
        }
    }

    if(m_xmlReader.hasError()) {
        qCritical() << "[DB] Failed to parse schema file" << schemaFilename << ":" << m_xmlReader.errorString();
        return false;
    }

    return true;
}

DbSchema::Revision DbSchema::readRevision()
{
    Revision revision;

    revision.version          = m_xmlReader.attributes().value("version").toInt();
    revision.minCompatVersion = m_xmlReader.attributes().value("minCompatVersion").toInt();

    while(m_xmlReader.readNextStartElement()) {
        if(m_xmlReader.name() == u"description") {
            revision.description = m_xmlReader.readElementText().trimmed();
        }
        else if(m_xmlReader.name() == u"sql") {
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

    qInfo() << "[DB] Upgrading schema to version" << revisionToApply << ":" << revision.description;

    DbTransaction transaction{db()};

    const QStringList statements = revision.sql.split(QStringLiteral(";"));

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
            qInfo() << "[DB] Ignoring statement" << statement << "while re-applying a schema migration";
            result = true;
        }
    }

    if(!result) {
        transaction.rollback();
        return UpgradeResult::Failed;
    }

    if(revisionToApply > currentRevision) {
        m_settingsDb.set(QString::fromLatin1(VersionKey), revisionToApply);
        m_settingsDb.set(QString::fromLatin1(LastVersionKey), revisionToApply);
        m_settingsDb.set(QString::fromLatin1(MinCompatVersionKey), revision.minCompatVersion);
        qInfo() << "[DB] Upgraded schema to version" << revisionToApply;
    }
    else {
        qInfo() << "[DB] Reapplied schema migration to version" << revisionToApply;
    }

    transaction.commit();

    return UpgradeResult::Success;
}
} // namespace Fooyin
