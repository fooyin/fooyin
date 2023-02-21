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

#include "core/library/libraryinfo.h"

#include <QObject>

namespace Core::DB {
class Database;
class Library;
} // namespace Core::DB

namespace Core::Library {
class LibraryManager : public QObject
{
    Q_OBJECT

public:
    explicit LibraryManager(DB::Database* database, QObject* parent = nullptr);
    ~LibraryManager() override;

    void reset();

    [[nodiscard]] const LibraryList& allLibraries() const;
    int addLibrary(const QString& path, const QString& name);
    bool removeLibrary(int id);
    bool renameLibrary(int id, const QString& name);
    [[nodiscard]] bool hasLibrary() const;
    [[nodiscard]] bool hasLibrary(int id) const;
    [[nodiscard]] LibraryInfo* findLibraryByPath(const QString& path) const;
    [[nodiscard]] LibraryInfo* libraryInfo(int id) const;

signals:
    void libraryAdded(Library::LibraryInfo* info);
    void libraryRemoved(int id);
    void libraryRenamed(int id, const QString& name);

private:
    DB::Database* m_database;
    DB::Library* m_libraryConnector;

    LibraryList m_libraries;
};
} // namespace Core::Library
