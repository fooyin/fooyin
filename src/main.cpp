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

#include "app/application.h"
#include "utils/version.h"

#include <QApplication>

int main(int argc, char* argv[])
{
    Q_INIT_RESOURCE(icons);
    Q_INIT_RESOURCE(fonts);

    Application app = Application(argc, argv);
    Application::setApplicationName("fooyin");
    Application::setApplicationVersion(VERSION);

    return Application::exec();
}
