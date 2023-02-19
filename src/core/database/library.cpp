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

namespace Core::DB {
Library::Library(const QString& connectionName)
    : Module{connectionName}
{ }

Core::Library::IdLibraryMap Library::getAllLibraries()
{
    const QString query = "SELECT LibraryID, Name, Path FROM Libraries;";

    Core::Library::IdLibraryMap libraries;

    Query q(this);
    q.prepare(query);

    if(!q.execQuery()) {
        q.error("Cannot fetch all libraries");
    }

    while(q.next()) {
        const int id       = q.value(0).toInt();
        const QString name = q.value(1).toString();
        const QString path = q.value(2).toString();

        libraries.emplace(id, Core::Library::LibraryInfo{name, path, id});
    }
    return libraries;
}

bool Library::insertLibrary(int id, const QString& path, const QString& name)
{
    if(name.isEmpty() || path.isEmpty()) {
        return false;
    }

    const QString query = "INSERT INTO Libraries "
                          "(LibraryID, Name, Path) "
                          "VALUES "
                          "(:libraryId, :libraryName, :libraryPath);";

    Query q(this);

    q.prepare(query);
    q.bindValue(":libraryId", id);
    q.bindValue(":libraryName", name);
    q.bindValue(":libraryPath", path);

    if(!q.execQuery()) {
        q.error(QString("Cannot insert library (name: %1, path: %2)").arg(name, path));
        return false;
    }
    return true;
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
} // namespace Core::DB
