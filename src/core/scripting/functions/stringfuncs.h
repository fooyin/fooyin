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

#pragma once

#include <core/scripting/scriptvalue.h>

#include <QStringList>

namespace Fooyin::Scripting {
QString num(const QStringList& vec);
QString replace(const QStringList& vec);
QString slice(const QStringList& vec);
QString chop(const QStringList& vec);
QString left(const QStringList& vec);
QString right(const QStringList& vec);
ScriptResult strcmp(const QStringList& vec);
ScriptResult strcmpi(const QStringList& vec);
QString sep();
QString swapPrefix(const QStringList& vec);
QString pad(const QStringList& vec);
QString padRight(const QStringList& vec);
QString repeat(const QStringList& vec);
} // namespace Fooyin::Scripting
