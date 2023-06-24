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

bool Library::getAllLibraries(Core::Library::LibraryInfoList& libraries)
{
    const QString query = "SELECT LibraryID, Name, Path FROM Libraries;";

    Query q(this);
    q.prepareQuery(query);

    if(!q.execQuery()) {
        q.error("Cannot fetch all libraries");
        return false;
    }

    while(q.next()) {
        const int id       = q.value(0).toInt();
        const QString name = q.value(1).toString();
        const QString path = q.value(2).toString();

        libraries.emplace_back(std::make_unique<Core::Library::LibraryInfo>(name, path, id));
    }
    return true;
}

int Library::insertLibrary(const QString& path, const QString& name)
{
    if(name.isEmpty() || path.isEmpty()) {
        return -1;
    }

    auto q = module()->insert("Libraries", {{"Name", name}, {"Path", path}},
                              QString{"Cannot insert library (name: %1, path: %2)"}.arg(name, path));

    return (q.hasError()) ? -1 : q.lastInsertId().toInt();
}

bool Library::removeLibrary(int id)
{
    auto q = module()->remove("Libraries", {{"LibraryID", QString::number(id)}},
                              QString{"Cannot remove library %1"}.arg(id));
    return !q.hasError();
}

bool Library::renameLibrary(int id, const QString& name)
{
    if(name.isEmpty()) {
        return false;
    }

    auto q = module()->update("Libraries", {{"Name", name}}, {"LibraryID", QString::number(id)},
                              QString{"Cannot update library %1"}.arg(id));
    return !q.hasError();
}
} // namespace Fy::Core::DB
