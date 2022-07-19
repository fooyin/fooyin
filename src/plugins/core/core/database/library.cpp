/*
 * Fooyin
 * Copyright 2022, Luke Taylor <LukeT1@proton.me>
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

#include "library.h"

#include "library/libraryinfo.h"
#include "query.h"

namespace DB {
Library::Library(const QString& connectionName)
    : Module(connectionName)
{ }

QMap<int, ::Library::LibraryInfo> Library::getAllLibraries()
{
    QString query = "SELECT LibraryID, Name, Path FROM Libraries;";

    QMap<int, ::Library::LibraryInfo> libs;

    Query q(this);
    q.prepare(query);

    bool success = q.exec();

    if(!success) {
        q.error("Cannot fetch all libraries");
    }

    while(q.next()) {
        qint8 id = static_cast<qint8>(q.value(0).toInt());
        QString name = q.value(1).toString();
        QString path = q.value(2).toString();

        libs.insert(id, ::Library::LibraryInfo(path, name, id));
    }

    return libs;
}

bool Library::insertLibrary(int id, const QString& path, const QString& name)
{
    if(name.isEmpty() || path.isEmpty()) {
        //        mLog(Log::Warning, this) << "Cannot insert library: Invalid parameters";
        return false;
    }

    QString query = "INSERT INTO Libraries "
                    "(LibraryID, Name, Path) "
                    "VALUES "
                    "(:libraryId, :libraryName, :libraryPath);";

    Query q(this);

    q.prepare(query);
    q.bindValue(":libraryId", id);
    q.bindValue(":libraryName", name);
    q.bindValue(":libraryPath", path);

    bool success = q.exec();

    if(!success) {
        q.error(QString("Cannot insert library (name: %1, path: %2)").arg(name, path));
    }

    return success;
}

bool Library::removeLibrary(int id)
{
    QString query = "DELETE FROM Libraries WHERE LibraryID=:libraryId;";

    Query q(this);

    q.prepare(query);
    q.bindValue(":libraryId", id);

    bool success = q.exec();

    if(!success) {
        q.error(QString("Cannot remove library %1").arg(id));
    }

    return success;
}

Library::~Library() = default;
} // namespace DB
