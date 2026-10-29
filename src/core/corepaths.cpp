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

#include "corepaths.h"

#include "config.h"

#include <utils/fileutils.h>
#include <utils/fypaths.h>

#include <QCoreApplication>
#include <QDir>
#include <QStandardPaths>

using namespace Qt::StringLiterals;

namespace Fooyin::Core {
QString settingsPath()
{
    return QDir::cleanPath(Utils::configPath().append("/fooyin.conf"_L1));
}

QString statePath()
{
    return QDir::cleanPath(Utils::statePath().append("/fooyin.state"_L1));
}

QString playlistsPath()
{
    return QDir::cleanPath(Utils::sharePath().append("/playlists"_L1));
}

QStringList pluginPaths()
{
    QStringList paths;

    const auto appendPath = [&paths](const QString& path) {
        const QString cleanPath = Utils::File::cleanPath(path);
        if(!cleanPath.isEmpty() && !paths.contains(cleanPath)) {
            paths.append(cleanPath);
        }
    };

    const QDir appPath{QCoreApplication::applicationDirPath()};

    appendPath(appPath.absolutePath() + u"/"_s + QString::fromLatin1(RELATIVE_PLUGIN_PATH));
    appendPath(appPath.absolutePath() + u"/plugins"_s);

    const QStringList environmentPaths
        = qEnvironmentVariable("FOOYIN_PLUGIN_PATH").split(QDir::listSeparator(), Qt::SkipEmptyParts);
    for(const QString& path : environmentPaths) {
        appendPath(path);
    }

    appendPath(userPluginsPath());

    return paths;
}

QString userPluginsPath()
{
#ifdef Q_OS_WIN
    if(Utils::isPortable()) {
        const QDir appPath{QCoreApplication::applicationDirPath()};
        return Utils::createPath(Utils::File::cleanPath(appPath.absolutePath() + u"/plugins"_s));
    }
    return Utils::createPath(
        Utils::File::cleanPath(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)) + u"/plugins"_s);
#else
    if(Utils::isFlatpak()) {
        return Utils::createPath(Utils::File::cleanPath(
            u"%1/.var/app/org.fooyin.fooyin/.local/lib/fooyin/plugins/"_s.arg(QDir::homePath())));
    }
    return Utils::createPath(Utils::File::cleanPath(u"%1/.local/lib/fooyin/plugins/"_s.arg(QDir::homePath())));
#endif
}

QString translationsPath()
{
    const QDir embeddedPath{u"://translations"_s};
    if(embeddedPath.exists()) {
        return embeddedPath.absolutePath();
    }

    QDir appPath{QCoreApplication::applicationDirPath()};

    const QDir systemPath{appPath.absolutePath() + u"/"_s + QString::fromLatin1(RELATIVE_TRANSLATION_PATH)};
    if(systemPath.exists()) {
        return systemPath.absolutePath();
    }

    if(appPath.cd(u"../../data"_s)) {
        return appPath.absolutePath();
    }

    return {};
}
} // namespace Fooyin::Core
