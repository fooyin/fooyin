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

#include "coreplugin.h"

#include "core/app/application.h"

namespace Core {
CorePlugin::CorePlugin() = default;

CorePlugin::~CorePlugin() = default;

void CorePlugin::initialise()
{
    m_app = new Application();
}

void CorePlugin::finalise()
{
    m_app->startup();
}

void CorePlugin::shutdown()
{
    m_app->shutdown();
    delete m_app;
}
} // namespace Core
