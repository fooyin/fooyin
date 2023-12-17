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

#pragma once

#include "fycore_export.h"

#include <QtPlugin>

namespace Fooyin {
/*!
 * An abstract interface for fooyin plugins.
 *
 * A plugin must implement this class in order to be recognised as a valid
 * fooyin plugin. It must also use the same IID as this interface like so:
 *
 * @code
 *     Q_PLUGIN_METADATA(IID "com.fooyin.plugin/1.0" FILE "metadata.json")
 * @endcode
 */
class FYCORE_EXPORT Plugin
{
public:
    virtual ~Plugin() = default;

    /*!
     * This is called just before fooyin is closed.
     * Reimplement to handle any needed cleanup, including saving settings.
     * @note The base class implementation of this function does nothing.
     */
    virtual void shutdown(){};
};
} // namespace Fooyin

Q_DECLARE_INTERFACE(Fooyin::Plugin, "com.fooyin.plugin/1.0")
