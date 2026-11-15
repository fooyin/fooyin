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

#include "libraryexclusions.h"

#include "libraryscanutils.h"

namespace Fooyin {
bool LibraryExclusions::contains(const QString& path) const
{
    if(files.empty() && folders.empty()) {
        return false;
    }

    const QString normalisedPath = normalisePath(path);
    if(files.contains(normalisedPath)) {
        return true;
    }

    return std::ranges::any_of(folders, [&normalisedPath](const QString& folder) {
        const QString prefix = folder.endsWith(u'/') ? folder : folder + u'/';
        return normalisedPath == folder || normalisedPath.startsWith(prefix);
    });
}
} // namespace Fooyin
