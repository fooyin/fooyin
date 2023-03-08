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

#include "libraryinfo.h"
#include "musiclibrary.h"
#include "unifiedmusiclibrary.h"

#include <QObject>

namespace Fy {

namespace Utils {
class ThreadManager;
class SettingsManager;
} // namespace Utils

namespace Core {
namespace DB {
class Database;
class Library;
} // namespace DB

namespace Library {
class UnifiedTrackStore;
class MusicLibraryContainer;

using LibraryIdMap = std::unordered_map<int, MusicLibraryInternal*>;

class LibraryManager : public QObject
{
    Q_OBJECT

public:
    explicit LibraryManager(Utils::ThreadManager* threadManager, DB::Database* database,
                            Utils::SettingsManager* settings, QObject* parent = nullptr);
    ~LibraryManager() override;

    void reset();

    [[nodiscard]] MusicLibrary* currentLibrary() const;
    [[nodiscard]] MusicLibrary* library(int id) const;
    [[nodiscard]] const LibraryInfoList& allLibrariesInfo() const;
    [[nodiscard]] const LibraryIdMap& allLibraries() const;
    int addLibrary(const QString& path, const QString& name);
    bool removeLibrary(int id);
    bool renameLibrary(int id, const QString& name);
    void changeCurrentLibrary(int id);
    [[nodiscard]] bool hasLibrary() const;
    [[nodiscard]] bool hasLibrary(int id) const;
    [[nodiscard]] LibraryInfo* findLibraryByPath(const QString& path) const;
    [[nodiscard]] LibraryInfo* libraryInfo(int id) const;

signals:
    void libraryAdded(Library::LibraryInfo* info);
    void libraryRemoved(int id);
    void libraryRenamed(int id, const QString& name);

private:
    MusicLibraryInternal* addNewLibrary(LibraryInfo* info);

    Utils::ThreadManager* m_threadManager;
    DB::Database* m_database;
    Utils::SettingsManager* m_settings;

    DB::Library* m_libraryConnector;

    LibraryInfoList m_libraryInfos;
    LibraryIdMap m_libraries;
    UnifiedMusicLibrary* m_unifiedLibrary;
    UnifiedTrackStore* m_unifiedStore;
    MusicLibraryContainer* m_libraryHandler;
};
} // namespace Library
} // namespace Core
} // namespace Fy
