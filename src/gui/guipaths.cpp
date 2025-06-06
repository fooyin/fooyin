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

#include <gui/guipaths.h>

#include <utils/fypaths.h>

#include <QString>

using namespace Qt::StringLiterals;

namespace Fooyin::Gui {
QString layoutsPath()
{
    return Utils::configPath(u"layouts"_s).append(u"/"_s);
}

QString activeLayoutPath()
{
    return Utils::configPath().append(u"/layout.fyl"_s);
}

QString coverPath()
{
    return Utils::cachePath(u"covers"_s).append(u"/"_s);
}
} // namespace Fooyin::Gui
