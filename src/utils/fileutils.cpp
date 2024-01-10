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

#include <utils/fileutils.h>

#include <QDesktopServices>
#include <QDir>
#include <QFile>

namespace Fooyin::Utils::File {
QString cleanPath(const QString& path)
{
    if(path.trimmed().isEmpty()) {
        return {};
    }
    return QDir::cleanPath(path);
}

bool isSamePath(const QString& filename1, const QString& filename2)
{
    const auto cleaned1 = cleanPath(filename1);
    const auto cleaned2 = cleanPath(filename2);

    return (cleaned1.compare(cleaned2) == 0);
}

bool isSubdir(const QString& dir, const QString& parentDir)
{
    if(isSamePath(dir, parentDir)) {
        return false;
    }

    const auto cleanedDir       = cleanPath(dir);
    const auto cleanedParentDir = cleanPath(parentDir);

    if(cleanedDir.isEmpty() || cleanedParentDir.isEmpty()) {
        return false;
    }

    const QFileInfo info(cleanedDir);

    QDir d1(cleanedDir);
    if(info.exists() && info.isFile()) {
        const auto d1String = getParentDirectory(cleanedDir);
        if(isSamePath(d1String, parentDir)) {
            return true;
        }

        d1.setPath(d1String);
    }

    const QDir d2(cleanedParentDir);

    while(!d1.isRoot()) {
        d1.setPath(getParentDirectory(d1.absolutePath()));
        if(isSamePath(d1.absolutePath(), d2.absolutePath())) {
            return true;
        }
    }

    return false;
}

QString getParentDirectory(const QString& filename)
{
    const auto cleaned = cleanPath(filename);
    const auto index   = cleaned.lastIndexOf(QDir::separator());

    return (index > 0) ? cleanPath(cleaned.left(index)) : QDir::rootPath();
}

bool createDirectories(const QString& path)
{
    return QDir().mkpath(path);
}

void openDirectory(const QString& dir)
{
    const QUrl url = QUrl::fromLocalFile(QDir::toNativeSeparators(dir));
    QDesktopServices::openUrl(url);
}

QStringList getFilesInDir(const QDir& baseDirectory, const QStringList& fileExtensions)
{
    QStringList ret;
    QList<QDir> stack{baseDirectory};

    while(!stack.isEmpty()) {
        const QDir dir              = stack.takeFirst();
        const QFileInfoList subDirs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
        for(const auto& subDir : subDirs) {
            stack.append(QDir{subDir.absoluteFilePath()});
        }
        const QFileInfoList files = dir.entryInfoList(fileExtensions, QDir::Files);
        for(const auto& file : files) {
            ret.append(file.absoluteFilePath());
        }
    }
    return ret;
}

QStringList getFiles(const QStringList& paths, const QStringList& fileExtensions)
{
    QStringList files;
    std::vector<QDir> dirs;

    for(const QString& path : paths) {
        QFileInfo file{path};
        if(file.exists() && file.isFile()) {
            files.push_back(path);
        }
        else if(file.isDir()) {
            dirs.push_back({path});
        }
    }

    for(const QDir& dir : dirs) {
        files.append(getFilesInDir(dir, fileExtensions));
    }

    return files;
}

QStringList getFiles(const QList<QUrl>& urls, const QStringList& fileExtensions)
{
    QStringList paths;
    std::ranges::transform(std::as_const(urls), std::back_inserter(paths),
                           [](const QUrl& url) { return url.toLocalFile(); });
    return getFiles(paths, fileExtensions);
}
} // namespace Fooyin::Utils::File
