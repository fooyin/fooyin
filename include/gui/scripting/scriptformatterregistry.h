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

#include "fygui_export.h"

#include <gui/scripting/richtext.h>

namespace Fooyin {
/*!
 * Resolves built-in rich-text formatting tags used by `ScriptFormatter`.
 */
class FYGUI_EXPORT ScriptFormatterRegistry
{
public:
    /*!
     * Applies the formatter command `func` with optional `option` to `formatting`.
     *
     * Returns `true` if the formatter command was recognised.
     */
    [[nodiscard]] static bool format(RichFormatting& formatting, const QString& func, const QString& option = {});
};
} // namespace Fooyin
