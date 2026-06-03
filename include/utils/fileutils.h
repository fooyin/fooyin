/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <luket@pm.me>
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

#include "fyutils_export.h"

#include <QDir>
#include <QFile>
#include <QStringList>
#include <QUrl>

namespace Fooyin::Utils::File {
FYUTILS_EXPORT QString cleanPath(const QString& path);
FYUTILS_EXPORT bool isSamePath(const QString& filename1, const QString& filename2);
FYUTILS_EXPORT bool isSubdir(const QString& dir, const QString& parentDir);
FYUTILS_EXPORT QString getParentDirectory(const QString& filename);

FYUTILS_EXPORT bool createDirectories(const QString& path);
FYUTILS_EXPORT void openDirectory(const QString& dir);
FYUTILS_EXPORT uint64_t directorySize(const QString& dir);

FYUTILS_EXPORT QStringList getFilesInDir(const QDir& baseDirectory, const QStringList& fileExtensions = {});
FYUTILS_EXPORT QStringList getFilesInDirRecursive(const QDir& baseDirectory, const QStringList& fileExtensions = {});
FYUTILS_EXPORT QList<QUrl> getUrlsInDir(const QDir& baseDirectory, const QStringList& fileExtensions = {});
FYUTILS_EXPORT QList<QUrl> getUrlsInDirRecursive(const QDir& baseDirectory, const QStringList& fileExtensions = {});
FYUTILS_EXPORT QStringList getFiles(const QStringList& paths, const QStringList& fileExtensions = {});
FYUTILS_EXPORT QStringList getFiles(const QList<QUrl>& urls, const QStringList& fileExtensions = {});
FYUTILS_EXPORT QStringList getAllSubdirectories(const QDir& dir);

FYUTILS_EXPORT bool pathContainsWildcard(const QString& path);
FYUTILS_EXPORT QStringList directoriesFromWildcardPath(const QString& pathPattern);
FYUTILS_EXPORT QStringList filesFromWildcardPath(const QString& pathPattern, QDir::Filters filters = QDir::Files,
                                                 QDir::SortFlags sort = QDir::Name);
} // namespace Fooyin::Utils::File
