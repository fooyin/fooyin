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

#include "module.h"

class QSqlDatabase;

namespace Fy {

namespace Utils {
class SettingsManager;
}

namespace Core::DB {
class Database : public Module
{
public:
    explicit Database(Utils::SettingsManager* settings);
    Database(const QString& directory, const QString& filename, Utils::SettingsManager* settings);

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
    Utils::SettingsManager* m_settings;

    bool m_initialized;
};
} // namespace Core::DB
} // namespace Fy
