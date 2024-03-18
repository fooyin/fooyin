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

#pragma once

#include "settingsdatabase.h"

#include <QXmlStreamReader>

namespace Fooyin {
class SettingsManager;

class DbSchema : public DbModule
{
public:
    enum class UpgradeResult
    {
        IsCurrent,
        BackwardsCompatible,
        Incompatible,
        Success,
        Failed,
        Error,
    };

    explicit DbSchema(const DbConnectionProvider& dbProvider);

    [[nodiscard]] int currentVersion() const;
    [[nodiscard]] int lastVersion() const;
    [[nodiscard]] int minCompatVersion() const;

    UpgradeResult upgradeDatabase(int targetVersion, const QString& schemaFilename);

private:
    struct Revision
    {
        int version{0};
        int minCompatVersion{0};
        QString description;
        QString sql;
    };

    bool readSchema(const QString& schemaFilename);
    Revision readRevision();
    UpgradeResult applyRevision(int currentRevision, int revisionToApply);

    SettingsDatabase m_settingsDb;
    QXmlStreamReader m_xmlReader;
    std::map<int, Revision> m_revisions;
};
} // namespace Fooyin
