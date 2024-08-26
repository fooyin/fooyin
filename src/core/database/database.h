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

#pragma once

#include "fycore_export.h"

#include <utils/database/dbconnectionhandler.h>
#include <utils/database/dbconnectionpool.h>

#include <QObject>

namespace Fooyin {
class FYCORE_EXPORT Database : public QObject
{
    Q_OBJECT

public:
    enum class Status
    {
        Ok,
        Incompatible,
        SchemaError,
        DbError,
        ConnectionError,
    };

    explicit Database(QObject* parent = nullptr);

    [[nodiscard]] DbConnectionPoolPtr connectionPool() const;

    [[nodiscard]] Status status() const;
    [[nodiscard]] int currentRevision() const;
    [[nodiscard]] int previousRevision() const;

signals:
    void statusChanged(Status status);

private:
    bool initSchema();
    void changeStatus(Status status);

    DbConnectionPoolPtr m_dbPool;
    DbConnectionHandler m_connectionHandler;
    Status m_status;
    int m_previousRevision;
};
} // namespace Fooyin
