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

#include "corepaths.h"

#include "config.h"

#include <utils/fileutils.h>
#include <utils/paths.h>

#include <QCoreApplication>
#include <QDir>

namespace Fooyin::Core {
QString settingsPath()
{
    return QDir::cleanPath(Utils::configPath().append(QStringLiteral("/fooyin.conf")));
}

QStringList pluginPaths()
{
    QStringList paths;

    QDir appPath{QCoreApplication::applicationDirPath()};

    const QDir systemPath{appPath.absolutePath() + QStringLiteral("/") + QString::fromLatin1(RELATIVE_PLUGIN_PATH)};
    if(systemPath.exists()) {
        paths.append(Utils::File::cleanPath(systemPath.absolutePath()));
    }

    if(appPath.cd(QStringLiteral("plugins"))) {
        paths.append(Utils::File::cleanPath(appPath.absolutePath()));
    }

    paths.append(userPluginsPath());

    return paths;
}

QString userPluginsPath()
{
    return QDir::cleanPath(Utils::sharePath(QStringLiteral("plugins")));
}

QString translationsPath()
{
    const QDir embeddedPath{QStringLiteral("://translations")};
    if(embeddedPath.exists()) {
        return embeddedPath.absolutePath();
    }

    QDir appPath{QCoreApplication::applicationDirPath()};

    const QDir systemPath{appPath.absolutePath() + QStringLiteral("/")
                          + QString::fromLatin1(RELATIVE_TRANSLATION_PATH)};
    if(systemPath.exists()) {
        return systemPath.absolutePath();
    }

    if(appPath.cd(QStringLiteral("../../data"))) {
        return appPath.absolutePath();
    }

    return {};
} // namespace
} // namespace Fooyin::Core
