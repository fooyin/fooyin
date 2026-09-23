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

#include <utils/fileutils.h>
#include <utils/fypaths.h>

#include <QDesktopServices>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QLoggingCategory>
#include <QPromise>

#ifdef FOOYIN_UTILS_DBUS
#include <QDBusInterface>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusUnixFileDescriptor>

#endif

#include <memory>

Q_LOGGING_CATEGORY(FILE_UTILS, "fy.fileutils")

using namespace Qt::StringLiterals;

namespace {
struct PathParts
{
    QString root;
    QStringList segments;
};

PathParts splitPath(const QString& path)
{
    QString root;
    QString remainder = QDir::fromNativeSeparators(path);

    if(remainder.size() >= 3 && remainder.at(0).isLetter() && remainder.at(1) == ':'_L1 && remainder.at(2) == '/'_L1) {
        root      = remainder.left(3);
        remainder = remainder.mid(3);
    }
    else if(remainder.startsWith("//"_L1)) {
        const qsizetype serverEnd = remainder.indexOf('/'_L1, 2);
        if(serverEnd >= 0) {
            const qsizetype shareEnd = remainder.indexOf('/'_L1, serverEnd + 1);
            if(shareEnd >= 0) {
                root      = remainder.left(shareEnd + 1);
                remainder = remainder.mid(shareEnd + 1);
            }
        }
    }
    else if(remainder.startsWith('/'_L1)) {
        root      = u"/"_s;
        remainder = remainder.mid(1);
    }

    return {.root = root, .segments = remainder.split('/'_L1, Qt::SkipEmptyParts)};
}
} // namespace

namespace Fooyin::Utils::File {
QFuture<bool> moveToTrash(QString path)
{
    auto promise = std::make_shared<QPromise<bool>>();
    promise->start();
    const QFuture<bool> future = promise->future();

#ifdef FOOYIN_UTILS_DBUS
    if(isFlatpak()) {
        auto file = std::make_shared<QFile>(path);
        if(!file->open(QIODevice::ReadWrite | QIODevice::ExistingOnly)) {
            qCWarning(FILE_UTILS) << "Failed to open file for trash portal" << path << file->errorString();
            promise->addResult(false);
            promise->finish();
            return future;
        }

        QDBusInterface portal{u"org.freedesktop.portal.Desktop"_s, u"/org/freedesktop/portal/desktop"_s,
                              u"org.freedesktop.portal.Trash"_s, QDBusConnection::sessionBus()};
        auto* watcher = new QDBusPendingCallWatcher{
            portal.asyncCall(u"TrashFile"_s, QVariant::fromValue(QDBusUnixFileDescriptor{file->handle()}))};
        auto complete = [watcher, promise, file = std::move(file), path = std::move(path)]() {
            const QDBusPendingReply<uint> reply{*watcher};
            bool moved = false;
            if(reply.isError()) {
                qCWarning(FILE_UTILS) << "Trash portal failed for" << path << reply.error().message();
            }
            else if(reply.value() != 1) {
                qCWarning(FILE_UTILS) << "Trash portal did not move" << path << "(result" << reply.value() << ')';
            }
            else {
                moved = true;
            }
            promise->addResult(moved);
            promise->finish();
            watcher->deleteLater();
        };
        if(watcher->isFinished()) {
            complete();
        }
        else {
            QObject::connect(watcher, &QDBusPendingCallWatcher::finished, watcher, std::move(complete));
        }
        return future;
    }
#endif
    promise->addResult(QFile::moveToTrash(path));
    promise->finish();
    return future;
}

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
    const QString cleaned = QDir::fromNativeSeparators(cleanPath(filename));
    const auto index      = cleaned.lastIndexOf('/'_L1);

    return (index > 0) ? cleanPath(cleaned.left(index)) : QDir::rootPath();
}

bool createDirectories(const QString& path)
{
    return QDir().mkpath(path);
}

void openDirectory(const QString& dir)
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
}

uint64_t directorySize(const QString& dir)
{
    qint64 total{0};

    QDirIterator it{dir, QDirIterator::Subdirectories | QDirIterator::FollowSymlinks};

    while(it.hasNext()) {
        it.next();
        if(it.fileInfo().isFile()) {
            total += it.fileInfo().size();
        }
    }

    return static_cast<uint64_t>(total);
}

QStringList getFilesInDir(const QDir& baseDirectory, const QStringList& fileExtensions)
{
    QStringList ret;

    const QFileInfoList files = baseDirectory.entryInfoList(fileExtensions, QDir::Files);
    for(const auto& file : files) {
        ret.append(file.absoluteFilePath());
    }

    return ret;
}

QStringList getFilesInDirRecursive(const QDir& baseDirectory, const QStringList& fileExtensions)
{
    QStringList ret;
    QList<QDir> stack{baseDirectory};

    while(!stack.isEmpty()) {
        const QDir dir = stack.takeFirst();

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

QList<QUrl> getUrlsInDir(const QDir& baseDirectory, const QStringList& fileExtensions)
{
    QList<QUrl> ret;

    const QFileInfoList files = baseDirectory.entryInfoList(fileExtensions, QDir::Files);
    for(const auto& file : files) {
        ret.append(QUrl::fromLocalFile(file.absoluteFilePath()));
    }

    return ret;
}

QList<QUrl> getUrlsInDirRecursive(const QDir& baseDirectory, const QStringList& fileExtensions)
{
    QList<QUrl> ret;
    QList<QDir> stack{baseDirectory};

    while(!stack.isEmpty()) {
        const QDir dir = stack.takeFirst();

        const QFileInfoList subDirs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
        for(const auto& subDir : subDirs) {
            stack.append(QDir{subDir.absoluteFilePath()});
        }

        const QFileInfoList files = dir.entryInfoList(fileExtensions, QDir::Files);
        for(const auto& file : files) {
            ret.append(QUrl::fromLocalFile(file.absoluteFilePath()));
        }
    }

    return ret;
}

QStringList getFiles(const QStringList& paths, const QStringList& fileExtensions)
{
    QStringList files;
    std::vector<QDir> dirs;

    for(const QString& path : paths) {
        const QFileInfo file{path};
        if(file.exists() && file.isFile() && QDir::match(fileExtensions, file.fileName())) {
            files.push_back(path);
        }
        else if(file.isDir()) {
            dirs.emplace_back(path);
        }
    }

    for(const QDir& dir : dirs) {
        files.append(getFilesInDirRecursive(dir, fileExtensions));
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

QStringList getAllSubdirectories(const QDir& dir)
{
    return getAllSubdirectories(dir, {});
}

QStringList getAllSubdirectories(const QDir& dir, const std::stop_token stopToken)
{
    QStringList directories;
    QDirIterator it{dir.absolutePath(), QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories};

    while(!stopToken.stop_requested() && it.hasNext()) {
        const QString subdir = it.next();
        directories.push_back(subdir);
    }

    return directories;
}

bool pathContainsWildcard(const QString& path)
{
    return path.contains('*'_L1);
}

QStringList directoriesFromWildcardPath(const QString& pathPattern)
{
    const QString cleanPath = QDir::fromNativeSeparators(QDir::cleanPath(pathPattern));
    if(!pathContainsWildcard(cleanPath)) {
        return QFileInfo{cleanPath}.isDir() ? QStringList{cleanPath} : QStringList{};
    }

    const auto [root, segments] = splitPath(cleanPath);
    if(segments.empty()) {
        return {};
    }

    int firstWildcardSegment{0};
    while(firstWildcardSegment < segments.size() && !pathContainsWildcard(segments.at(firstWildcardSegment))) {
        ++firstWildcardSegment;
    }

    QString basePath = segments.sliced(0, firstWildcardSegment).join('/'_L1);
    basePath.prepend(root);
    if(basePath.isEmpty()) {
        basePath = root.isEmpty() ? u"."_s : root;
    }

    QStringList dirs{basePath};

    const auto wildcardSegments = segments.sliced(firstWildcardSegment);
    for(const QString& segment : wildcardSegments) {
        QStringList nextDirs;

        for(const QString& dirPath : std::as_const(dirs)) {
            const QDir dir{dirPath};
            if(pathContainsWildcard(segment)) {
                const auto entries = dir.entryList({segment}, QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
                for(const QString& entry : entries) {
                    nextDirs.emplace_back(dir.absoluteFilePath(entry));
                }
            }
            else {
                const QString nextDir = dir.absoluteFilePath(segment);
                if(QFileInfo{nextDir}.isDir()) {
                    nextDirs.emplace_back(nextDir);
                }
            }
        }

        dirs = std::move(nextDirs);
        if(dirs.empty()) {
            break;
        }
    }

    return dirs;
}

QStringList filesFromWildcardPath(const QString& pathPattern, QDir::Filters filters, QDir::SortFlags sort)
{
    const QString cleanPath     = QDir::fromNativeSeparators(QDir::cleanPath(pathPattern));
    const qsizetype slash       = cleanPath.lastIndexOf('/'_L1);
    const QString dirPattern    = slash >= 0 ? cleanPath.left(slash) : u"."_s;
    const QString filenameMatch = slash >= 0 ? cleanPath.mid(slash + 1) : cleanPath;

    QStringList files;

    const auto dirs = directoriesFromWildcardPath(dirPattern);
    for(const QString& dirPath : dirs) {
        const QDir dir{dirPath};
        const auto entries = dir.entryList({filenameMatch}, filters, sort);
        for(const QString& file : entries) {
            files.emplace_back(dir.absoluteFilePath(file));
        }
    }

    return files;
}
} // namespace Fooyin::Utils::File
