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

#include "version.h"

#include <core/application.h>
#include <gui/guiapplication.h>

#include <kdsingleapplication.h>

#include <QApplication>

int main(int argc, char** argv)
{
    Q_INIT_RESOURCE(icons);

    QCoreApplication::setApplicationName("fooyin");
    QCoreApplication::setApplicationVersion(VERSION);

    const QApplication app{argc, argv};

    // Prevent additional instances
    KDSingleApplication instance{QCoreApplication::applicationName()};
    if(!instance.isPrimaryInstance()) {
        qInfo() << "fooyin already running";
        return 0;
    }

    Fy::Core::Application coreApp;
    Fy::Gui::GuiApplication guiApp{coreApp.context()};

    QObject::connect(&app, &QCoreApplication::aboutToQuit, &coreApp, [&coreApp, &guiApp]() {
        guiApp.shutdown();
        coreApp.shutdown();
    });

    return QCoreApplication::exec();
}
