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

#include "library.h"

#include "query.h"

namespace Fy::Core::DB {
Library::Library(const QString& connectionName)
    : Module{connectionName}
{ }

Core::Library::LibraryList Library::getAllLibraries()
{
    const QString query = "SELECT LibraryID, Name, Path FROM Libraries;";

    Core::Library::LibraryList libraries;

    Query q(this);
    q.prepare(query);

    if(!q.execQuery()) {
        q.error("Cannot fetch all libraries");
    }

    while(q.next()) {
        const int id       = q.value(0).toInt();
        const QString name = q.value(1).toString();
        const QString path = q.value(2).toString();

        libraries.emplace_back(std::make_unique<Core::Library::LibraryInfo>(name, path, id));
    }
    return libraries;
}

int Library::insertLibrary(const QString& path, const QString& name)
{
    if(name.isEmpty() || path.isEmpty()) {
        return -1;
    }

    const QString query = "INSERT INTO Libraries "
                          "(Name, Path) "
                          "VALUES "
                          "(:libraryName, :libraryPath);";

    Query q(this);

    q.prepare(query);
    q.bindValue(":libraryName", name);
    q.bindValue(":libraryPath", path);

    if(!q.execQuery()) {
        q.error(QString("Cannot insert library (name: %1, path: %2)").arg(name, path));
        return -1;
    }
    return q.lastInsertId().toInt();
}

bool Library::removeLibrary(int id)
{
    const QString query = "DELETE FROM Libraries WHERE LibraryID=:libraryId;";

    Query q(this);

    q.prepare(query);
    q.bindValue(":libraryId", id);

    if(!q.execQuery()) {
        q.error(QString{"Cannot remove library %1"}.arg(id));
        return false;
    }
    return true;
}

bool Library::renameLibrary(int id, const QString& name)
{
    if(name.isEmpty()) {
        return false;
    }

    const QString query = "UPDATE Libraries "
                          "SET Name = :libraryName "
                          "WHERE LibraryID=:libraryId;";

    Query q(this);

    q.prepare(query);
    q.bindValue(":libraryId", id);
    q.bindValue(":libraryName", name);

    if(!q.execQuery()) {
        q.error(QString{"Cannot update library (name: %1)"}.arg(name));
        return false;
    }
    return true;
}
} // namespace Fy::Core::DB
