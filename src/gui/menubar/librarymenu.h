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

#pragma once

#include <core/database/generaldatabase.h>
#include <core/library/musiclibrary.h>

#include <QObject>

namespace Fooyin {
class Application;
class ActionManager;
class MusicLibrary;

class LibraryMenu : public QObject
{
    Q_OBJECT

public:
    LibraryMenu(Application* core, ActionManager* actionManager, QObject* parent = nullptr);

signals:
    void requestSearch();
    void requestQuickSearch();

private:
    void removeUnavailbleTracks();
    void optimiseDatabase();
    void cleanupDatabase();

    DbConnectionPoolPtr m_database;
    MusicLibrary* m_library;

    WriteRequest m_deleteRequest;
};
} // namespace Fooyin
