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

#include <utils/stringutils.h>

#include <QJsonArray>
#include <QJsonValue>

namespace Fooyin::Utils {
QString readMultiLineString(const QJsonValue& value)
{
    if(value.isString()) {
        return value.toString();
    }

    if(!value.isArray()) {
        return {};
    }

    const QJsonArray array = value.toArray();

    QStringList lines;
    for(const auto& val : array) {
        if(!val.isString()) {
            return {};
        }
        lines.append(val.toString());
    }
    return lines.join(u'\n');
}
} // namespace Fooyin::Utils
