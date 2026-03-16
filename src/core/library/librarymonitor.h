/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
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

#include "librarywatcher.h"

#include <core/library/libraryinfo.h>

#include <QObject>

#include <unordered_map>

namespace Fooyin {
class LibraryMonitor : public QObject
{
    Q_OBJECT

public:
    explicit LibraryMonitor(QObject* parent = nullptr);

signals:
    void statusChanged(const Fooyin::LibraryInfo& library);
    void directoriesChanged(const Fooyin::LibraryInfo& library, const QStringList& dirs);

public slots:
    void setupWatchers(const Fooyin::LibraryInfoMap& libraries, bool enabled);

private:
    void addWatcher(const LibraryInfo& library);

    std::unordered_map<int, LibraryWatcher> m_watchers;
};
} // namespace Fooyin
