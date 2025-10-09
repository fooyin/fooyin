/*
 * Fooyin
 * Copyright © 2022, Luke Taylor <LukeT1@proton.me>
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

#include <utils/fypaths.h>

#include <utils/fileutils.h>

#include <QApplication>
#include <QDir>
#include <QLoggingCategory>
#include <QStandardPaths>

Q_LOGGING_CATEGORY(UTILS_PATHS, "fy.paths")

using namespace Qt::StringLiterals;

namespace {
QString operator/(const QString& first, const QString& second)
{
    using Fooyin::Utils::File::cleanPath;
    return (second.isEmpty()) ? cleanPath(first) : cleanPath(first + QDir::separator() + second);
}
} // namespace

namespace Fooyin::Utils {
bool isPortable()
{
    return QFile::exists(QCoreApplication::applicationDirPath() + "/PORTABLE"_L1);
}

QString createPath(const QString& path, const QString& appendPath)
{
    if(!QFileInfo::exists(path)) {
        if(!QDir().mkpath(path)) {
            qCWarning(UTILS_PATHS) << "Cannot create path:" << path;
        }
    }

    const QString fullPath = path / appendPath;

    if(!QFileInfo::exists(fullPath)) {
        if(!QDir().mkpath(fullPath)) {
            qCWarning(UTILS_PATHS) << "Cannot create path:" << fullPath;
        }
    }

    return fullPath;
}

QString configPath(const QString& appendPath)
{
    const auto path = isPortable() ? QCoreApplication::applicationDirPath() + "/config"_L1
                                   : QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    return createPath(path, appendPath);
}

QString statePath(const QString& appendPath)
{
    QString path;

    if(isPortable()) {
        path = QCoreApplication::applicationDirPath() + "/data"_L1;
        return createPath(path, appendPath);
    }

#if defined(Q_OS_UNIX) && !defined(Q_OS_MAC)
    QString stateHome = QFile::decodeName(qgetenv("XDG_STATE_HOME"));
    if(!stateHome.startsWith(u'/')) {
        stateHome.clear();
    }
    if(stateHome.isEmpty()) {
        stateHome = QDir::homePath() + "/.local/state"_L1;
    }
    path = stateHome + "/fooyin"_L1;
#else
    path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
#endif

    return createPath(path, appendPath);
}

QString sharePath(const QString& appendPath)
{
    static const auto path = isPortable() ? QCoreApplication::applicationDirPath() + "/data"_L1
                                          : QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return createPath(path, appendPath);
}

QString cachePath(const QString& appendPath)
{
    static const auto path = isPortable() ? QCoreApplication::applicationDirPath() + "/cache"_L1
                                          : QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    return createPath(path, appendPath);
}
} // namespace Fooyin::Utils
