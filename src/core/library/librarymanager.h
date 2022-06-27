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

#pragma once

#include "core/library/libraryinfo.h"
#include "core/scanner/libraryscanner.h"

class LibraryPlaylistInterface;

namespace DB {
class Database;
class Library;
} // namespace DB

namespace Library {
class MusicLibrary;
class LibraryScanner;

class LibraryManager : public QObject
{
    Q_OBJECT

public:
    explicit LibraryManager(QObject* parent = nullptr);
    ~LibraryManager() override;

    void reset();

    [[nodiscard]] QMap<int, LibraryInfo> allLibraries() const;
    int addLibrary(const QString& path, QString& name);
    void removeLibrary(int id);
    [[nodiscard]] bool hasLibrary() const;
    [[nodiscard]] LibraryInfo libraryInfo(int id) const;

signals:
    void libraryAdded(const Library::LibraryInfo& info);
    void libraryRemoved();

private:
    QMap<int, LibraryInfo> m_libraries;
    DB::Database* m_database;
    DB::Library* m_libraryConnector;
};
} // namespace Library
