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

using namespace Qt::Literals::StringLiterals;

namespace Fooyin::Core {
QString settingsPath()
{
    return QDir::cleanPath(Utils::configPath().append(QStringLiteral("/fooyin.conf")));
}

QString pluginsPath()
{
    return QDir::cleanPath(QCoreApplication::applicationDirPath() + '/' + RELATIVE_PLUGIN_PATH);
}

QString userPluginsPath()
{
    return QDir::cleanPath(Utils::sharePath("plugins"));
}

QString translationsPath()
{
    QDir appDir = QCoreApplication::applicationDirPath();

    if(appDir.cd(RELATIVE_TRANSLATION_PATH)) {
        return appDir.absolutePath() + u"/_s" + RELATIVE_TRANSLATION_PATH;
    }

    if(appDir.cd(u"../data"_s)) {
        return appDir.absolutePath() + u"/../data"_s;
    }

    return {};
} // namespace
} // namespace Fooyin::Core
