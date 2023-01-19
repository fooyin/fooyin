/*
 * Fooyin
 * Copyright 2022, Luke Taylor <LukeT1@proton.me>
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

#include "singleinstance.h"
#include "version.h"

#include <pluginsystem/pluginmanager.h>

#include <QApplication>

int main(int argc, char* argv[])
{
    Q_INIT_RESOURCE(icons);

    qApp->setApplicationName("fooyin");
    qApp->setApplicationVersion(VERSION);

    // Prevent additional instances
    SingleInstance instance("fooyin");
    if(!instance.tryRunning()) {
        return 0;
    }

    auto* app           = new QApplication(argc, argv);
    auto* pluginManager = PluginSystem::PluginManager::instance();

    // TODO: Pass down CMake vars
    const QString pluginsPath = QCoreApplication::applicationDirPath() + "/../lib/fooyin/plugins";
    pluginManager->findPlugins(pluginsPath);
    pluginManager->loadPlugins();

    // Shutdown plugins on exit
    // Required to ensure plugins are unloaded before main event loop quits
    QObject::connect(app, &QCoreApplication::aboutToQuit, pluginManager, &PluginSystem::PluginManager::shutdown);

    const int result = QApplication::exec();

    delete app;

    return result;
}
