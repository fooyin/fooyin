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

#include "libraryscantypes.h"

#include <functional>

class QFileInfo;

namespace Fooyin {
class LibraryScanState;

class LibraryFileEnumerator
{
public:
    using EnumeratedFileHandler = std::function<bool(const QFileInfo&, EnumeratedFileType)>;

    LibraryFileEnumerator(LibraryScanState* state, EnumeratedFileHandler handler);

    [[nodiscard]] bool enumerateFiles(const QStringList& paths, const QStringList& trackExtensions,
                                      const QStringList& playlistExtensions);

private:
    [[nodiscard]] bool processDirectory(const QString& path, const QStringList& trackExtensions,
                                        const QStringList& playlistExtensions, std::set<QString>& visitedDirs);

    LibraryScanState* m_state;
    EnumeratedFileHandler m_handler;
};
} // namespace Fooyin
