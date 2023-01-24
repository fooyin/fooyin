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

#include "paths.h"

#include "utils.h"

#include <QApplication>
#include <QDir>
#include <QStandardPaths>

namespace Utils {
QString operator/(const QString& first, const QString& second)
{
    return (second.isEmpty()) ? Utils::File::cleanPath(first)
                              : Utils::File::cleanPath(first + QDir::separator() + second);
}

QString createPath(const QString& path, const QString& appendPath)
{
    if(!Utils::File::exists(path)) {
        if(!QDir().mkpath(path)) {
            qDebug() << "Cannot create path: " << path;
        }
    }

    auto fullPath = path / appendPath;

    if(!Utils::File::exists(fullPath)) {
        if(!QDir().mkpath(fullPath)) {
            qDebug() << "Cannot create path: " << fullPath;
        }
    }

    return fullPath;
}

QString configPath(const QString& appendPath)
{
    static const auto path = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    return createPath(path, appendPath);
}

QString sharePath(const QString& appendPath)
{
    static const auto path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return createPath(path, appendPath);
}

QString cachePath(const QString& appendPath)
{
    static const auto path = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    return createPath(path, appendPath);
}

QString coverPath()
{
    return cachePath("covers").append("/");
}

QString settingsPath()
{
    return configPath().append("/fooyin.conf");
}
} // namespace Utils
