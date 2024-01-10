/*
 * Fooyin
 * Copyright 2022-2024, Luke Taylor <LukeT1@proton.me>
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

#include <QObject>

#include <set>

namespace Fooyin {
class SettingsManager;
class Database;

class FYCORE_EXPORT LibraryManager : public QObject
{
    Q_OBJECT

public:
    LibraryManager(Database* database, SettingsManager* settings, QObject* parent = nullptr);
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
    void libraryAdded(const LibraryInfo& library);
    void removingLibraryTracks(int id);
    void libraryRemoved(int id, const std::set<int> tracksRemoved);
    void libraryRenamed(int id, const QString& name);
    void libraryStatusChanged(const LibraryInfo& info);

private:
    struct Private;
    std::unique_ptr<Private> p;
};
} // namespace Fooyin
