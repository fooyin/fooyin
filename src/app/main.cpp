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

#include "application.h"
#include "singleinstance.h"
#include "version.h"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

int main(int argc, char* argv[])
{
    Q_INIT_RESOURCE(icons);

    qApp->setApplicationName("fooyin");
    qApp->setApplicationVersion(VERSION);

    auto console = spdlog::stdout_color_mt("console");
    spdlog::set_default_logger(console);

    // Prevent additional instances
    SingleInstance instance("fooyin");
    if(!instance.tryRunning()) {
        spdlog::info("Fooyin already running");
        return 0;
    }

    auto* app        = new Application(argc, argv);
    const int result = QCoreApplication::exec();

    delete app;

    return result;
}
