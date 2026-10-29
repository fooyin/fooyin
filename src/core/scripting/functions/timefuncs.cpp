/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <luket@pm.me>
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

#include "timefuncs.h"

#include <utils/stringutils.h>
#include <utils/utils.h>

#include <QStringList>

using namespace Qt::StringLiterals;

namespace Fooyin::Scripting {
namespace {
std::optional<Utils::ParsedDateTime> dateTimeArg(const QStringList& vec)
{
    if(vec.size() != 1 || vec.at(0).isEmpty()) {
        return {};
    }
    return Utils::parseDateTime(vec.at(0));
}
} // namespace

QString msToString(const QStringList& vec)
{
    if(vec.size() > 1 || vec.at(0).isEmpty()) {
        return {};
    }
    return Utils::msToString(vec.at(0).toULongLong());
}

QString year(const QStringList& vec)
{
    const auto dateTime = dateTimeArg(vec);
    return dateTime ? dateTime->date.toString("yyyy"_L1) : QString{};
}

QString month(const QStringList& vec)
{
    const auto dateTime = dateTimeArg(vec);
    return dateTime ? dateTime->date.toString("MM"_L1) : QString{};
}

QString dayOfMonth(const QStringList& vec)
{
    const auto dateTime = dateTimeArg(vec);
    return dateTime ? dateTime->date.toString("dd"_L1) : QString{};
}

QString date(const QStringList& vec)
{
    const auto dateTime = dateTimeArg(vec);
    return dateTime ? dateTime->date.toString("yyyy-MM-dd"_L1) : QString{};
}

QString time(const QStringList& vec)
{
    const auto dateTime = dateTimeArg(vec);
    if(!dateTime || dateTime->precision < Utils::DateTimePrecision::Hour) {
        return {};
    }

    return dateTime->time.toString(dateTime->precision == Utils::DateTimePrecision::Second ? "hh:mm:ss"_L1
                                                                                           : "hh:mm"_L1);
}
} // namespace Fooyin::Scripting
