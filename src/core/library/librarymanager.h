/*
 * Fooyin
 * Copyright © 2022, Luke Taylor <LukeT1@proton.me>
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

#include <core/library/libraryinfo.h>
#include <utils/database/dbconnectionpool.h>

#include <QObject>

#include <set>

namespace Fooyin {
class LibraryManagerPrivate;
class SettingsManager;

class FYCORE_EXPORT LibraryManager : public QObject
{
    Q_OBJECT

public:
    LibraryManager(DbConnectionPoolPtr dbPool, SettingsManager* settings, QObject* parent = nullptr);
    ~LibraryManager() override;

    void reset();

    [[nodiscard]] const LibraryInfoMap& allLibraries() const;

    int addLibrary(const QString& path, const QString& name);
    bool removeLibrary(int id);
    bool renameLibrary(int id, const QString& name);
    void updateLibraryStatus(const LibraryInfo& library);

    [[nodiscard]] bool hasLibrary() const;
    [[nodiscard]] bool hasLibrary(int id) const;

    [[nodiscard]] std::optional<LibraryInfo> findLibraryByPath(const QString& path) const;
    [[nodiscard]] std::optional<LibraryInfo> libraryInfo(int id) const;

signals:
    void libraryAdded(const Fooyin::LibraryInfo& library);
    void libraryAboutToBeRemoved(const Fooyin::LibraryInfo& library);
    void libraryRemoved(const Fooyin::LibraryInfo& library, const std::set<int>& tracksRemoved);
    void libraryRenamed(const Fooyin::LibraryInfo& library);
    void libraryStatusChanged(const Fooyin::LibraryInfo& info);

private:
    std::unique_ptr<LibraryManagerPrivate> p;
};
} // namespace Fooyin
