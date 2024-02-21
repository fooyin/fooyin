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

#include <utils/paths.h>

#include <QCoreApplication>
#include <QDir>

namespace Fooyin::Core {
QString settingsPath()
{
    return QDir::cleanPath(Utils::configPath().append(QStringLiteral("/fooyin.conf")));
}

QString pluginsPath()
{
    return QDir::cleanPath(QCoreApplication::applicationDirPath() + QStringLiteral("/")
                           + QString::fromLatin1(RELATIVE_PLUGIN_PATH));
}

QString userPluginsPath()
{
    return QDir::cleanPath(Utils::sharePath(QStringLiteral("plugins")));
}

QString translationsPath()
{
    QDir translations{QString::fromLatin1(FOOYIN_TRANSLATION_PATH)};
    if(translations.exists()) {
        return translations.absolutePath();
    }

    QDir appDir = QCoreApplication::applicationDirPath();
    if(appDir.cd(QStringLiteral("../data"))) {
        return appDir.absolutePath();
    }

    return {};
} // namespace
} // namespace Fooyin::Core
