/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <LukeT1@proton.me>
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

#include <core/engine/audiodecoder.h>

#include <QtPlugin>

namespace Fooyin {
/*!
 * An abstract interface for audio decoder plugins.
 */
class DecoderPlugin
{
public:
    virtual ~DecoderPlugin() = default;

    [[nodiscard]] virtual QString decoderName() const             = 0;
    [[nodiscard]] virtual QStringList supportedExtensions() const = 0;
    [[nodiscard]] virtual DecoderCreator decoderCreator() const   = 0;
};
} // namespace Fooyin

Q_DECLARE_INTERFACE(Fooyin::DecoderPlugin, "org.fooyin.fooyin.plugin.engine.decocder")
