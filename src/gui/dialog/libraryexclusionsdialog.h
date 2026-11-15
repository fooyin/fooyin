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

#include "core/library/libraryexclusions.h"

#include <core/library/libraryinfo.h>
#include <core/track.h>

#include <QDialog>

class QTreeWidget;

namespace Fooyin {
class MusicLibrary;
class LibraryManager;

class LibraryExclusionsDialog : public QDialog
{
    Q_OBJECT

public:
    LibraryExclusionsDialog(const LibraryInfo& libraryInfo, MusicLibrary* library, LibraryManager* libraryManager,
                            QWidget* parent = nullptr);

    static void excludeTracks(const TrackList& tracks, MusicLibrary* library, LibraryManager* libraryManager,
                              QWidget* parent, bool folders = false);

    void accept() override;

private:
    void addPaths(const QStringList& paths, bool folders);
    void populate();

    LibraryInfo m_libraryInfo;
    MusicLibrary* m_library;
    LibraryManager* m_libraryManager;
    LibraryExclusions m_exclusions;
    QTreeWidget* m_paths;
};
} // namespace Fooyin
