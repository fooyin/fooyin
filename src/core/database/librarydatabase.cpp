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

#include "librarydatabase.h"

#include <utils/database/dbquery.h>

namespace Fooyin {
bool LibraryDatabase::getAllLibraries(LibraryInfoMap& libraries)
{
    const QString statement = QStringLiteral("SELECT LibraryID, Name, Path FROM Libraries;");

    DbQuery query{db(), statement};

    if(!query.exec()) {
        return false;
    }

    while(query.next()) {
        const int id       = query.value(0).toInt();
        const QString name = query.value(1).toString();
        const QString path = query.value(2).toString();

        libraries.emplace(id, LibraryInfo{name, path, id});
    }

    return true;
}

int LibraryDatabase::insertLibrary(const QString& path, const QString& name)
{
    if(name.isEmpty() || path.isEmpty()) {
        return -1;
    }

    const QString statement = QStringLiteral("INSERT INTO Libraries (Name, Path) VALUES (:name, :path);");

    DbQuery query{db(), statement};

    query.bindValue(QStringLiteral(":name"), name);
    query.bindValue(QStringLiteral(":path"), path);

    if(!query.exec()) {
        return -1;
    }

    return query.lastInsertId().toInt();
}

bool LibraryDatabase::removeLibrary(int id)
{
    if(id < 0) {
        return false;
    }

    const QString statement = QStringLiteral("DELETE FROM Libraries WHERE LibraryID = :id;");

    DbQuery query{db(), statement};

    query.bindValue(QStringLiteral(":id"), id);

    return query.exec();
}

bool LibraryDatabase::renameLibrary(int id, const QString& name)
{
    if(name.isEmpty()) {
        return false;
    }

    const QString statement = QStringLiteral("UPDATE Libraries SET Name = :name WHERE LibraryId = :id;");

    DbQuery query{db(), statement};

    query.bindValue(QStringLiteral(":name"), name);
    query.bindValue(QStringLiteral(":id"), id);

    return query.exec();
}
} // namespace Fooyin
