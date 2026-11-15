/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <luket@pm.me>
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

#include "core/library/libraryscanutils.h"

#include <utils/database/dbquery.h>

#include <QDataStream>
#include <QIODevice>
#include <QStringList>

using namespace Qt::StringLiterals;

namespace Fooyin {
namespace {
QByteArray serialisePaths(const std::set<QString>& paths)
{
    QByteArray data;
    QDataStream stream{&data, QIODevice::WriteOnly};
    stream.setVersion(QDataStream::Qt_6_0);
    stream << QStringList{paths.cbegin(), paths.cend()};
    return data;
}

bool readPaths(const QByteArray& data, std::set<QString>& paths)
{
    if(data.isEmpty()) {
        return true;
    }

    QDataStream stream{data};
    stream.setVersion(QDataStream::Qt_6_0);

    QStringList storedPaths;
    stream >> storedPaths;
    if(stream.status() != QDataStream::Ok || !stream.atEnd()) {
        return false;
    }

    const auto normalised = normalisePaths(storedPaths);
    paths.insert(normalised.cbegin(), normalised.cend());
    return true;
}
} // namespace

bool LibraryDatabase::getAllLibraries(LibraryInfoMap& libraries)
{
    static const QString statement = u"SELECT LibraryID, Name, Path FROM Libraries;"_s;

    DbQuery query{db(), statement};

    if(!query.exec()) {
        return false;
    }

    while(query.next()) {
        const int id       = query.value(0).toInt();
        const QString name = query.value(1).toString();
        const QString path = query.value(2).toString();

        libraries.emplace(id, LibraryInfo{.name = name, .path = path, .id = id});
    }

    return true;
}

int LibraryDatabase::insertLibrary(const QString& path, const QString& name)
{
    if(name.isEmpty() || path.isEmpty()) {
        return -1;
    }

    static const QString statement = u"INSERT INTO Libraries (Name, Path) VALUES (:name, :path);"_s;

    DbQuery query{db(), statement};

    query.bindValue(u":name"_s, name);
    query.bindValue(u":path"_s, path);

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

    static const QString statement = u"DELETE FROM Libraries WHERE LibraryID = :id;"_s;

    DbQuery query{db(), statement};

    query.bindValue(u":id"_s, id);

    return query.exec();
}

bool LibraryDatabase::renameLibrary(int id, const QString& name)
{
    if(name.isEmpty()) {
        return false;
    }

    static const QString statement = u"UPDATE Libraries SET Name = :name WHERE LibraryId = :id;"_s;

    DbQuery query{db(), statement};

    query.bindValue(u":name"_s, name);
    query.bindValue(u":id"_s, id);

    return query.exec();
}

std::optional<LibraryExclusions> LibraryDatabase::exclusions(int id) const
{
    DbQuery query{db(), u"SELECT ExcludedFiles, ExcludedFolders FROM Libraries WHERE LibraryID = :id;"_s};
    query.bindValue(u":id"_s, id);
    if(!query.exec() || !query.next()) {
        return {};
    }

    LibraryExclusions exclusions;
    if(!readPaths(query.value(0).toByteArray(), exclusions.files)
       || !readPaths(query.value(1).toByteArray(), exclusions.folders)) {
        return {};
    }
    return exclusions;
}

bool LibraryDatabase::setExclusions(int id, const LibraryExclusions& exclusions)
{
    DbQuery query{db(), u"UPDATE Libraries SET ExcludedFiles = :files, ExcludedFolders = :folders "
                        "WHERE LibraryID = :id;"_s};
    query.bindValue(u":id"_s, id);
    query.bindValue(u":files"_s, serialisePaths(exclusions.files));
    query.bindValue(u":folders"_s, serialisePaths(exclusions.folders));
    return query.exec() && query.numRowsAffected() == 1;
}
} // namespace Fooyin
