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

#pragma once

#include "fycore_export.h"

#include <core/plugins/coreplugincontext.h>

#include <QtPlugin>

namespace Fy::Core {
class FYCORE_EXPORT CorePlugin
{
public:
    virtual ~CorePlugin() = default;

    virtual void initialise(const CorePluginContext& context) = 0;
};
} // namespace Fy::Core

Q_DECLARE_INTERFACE(Fy::Core::CorePlugin, "com.fooyin.plugin.core")
