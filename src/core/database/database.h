/*
 * Fooyin
 * Copyright 2022-2023, Luke Taylor <LukeT1@proton.me>
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

#include "databasemodule.h"

class QSqlDatabase;

namespace Fooyin {
class SettingsManager;

class Database : public DatabaseModule
{
public:
    explicit Database(SettingsManager* settings);
    Database(const QString& directory, const QString& filename, SettingsManager* settings);

    virtual bool closeDatabase();
    virtual bool isInitialized();

    virtual void transaction();
    virtual void commit();
    virtual void rollback();

    bool update();

protected:
    bool createDatabase();
    bool checkInsertTable(const QString& tableName, const QString& createString);
    bool checkInsertIndex(const QString& indexName, const QString& createString);

private:
    SettingsManager* m_settings;
    bool m_initialized;
};
} // namespace Fooyin
