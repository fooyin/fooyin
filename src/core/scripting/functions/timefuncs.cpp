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

#include <QDateTime>
#include <QStringList>

using namespace Qt::StringLiterals;

namespace {
QDateTime dateTimeArg(const QStringList& vec)
{
    if(vec.size() != 1 || vec.at(0).isEmpty()) {
        return {};
    }
    return Fooyin::Utils::dateStringToDate(vec.at(0));
}

bool hasSecondsPrecision(const QString& value)
{
    return QDateTime::fromString(value, "yyyy-MM-dd hh:mm:ss"_L1).isValid();
}

bool hasTimePrecision(const QString& value)
{
    return hasSecondsPrecision(value) || QDateTime::fromString(value, "yyyy-MM-dd hh:mm"_L1).isValid()
        || QDateTime::fromString(value, "yyyy-MM-dd hh"_L1).isValid();
}
} // namespace

namespace Fooyin::Scripting {
QString msToString(const QStringList& vec)
{
    if(vec.size() > 1 || vec.at(0).isEmpty()) {
        return {};
    }
    return Utils::msToString(vec.at(0).toULongLong());
}

QString year(const QStringList& vec)
{
    const QDateTime dateTime = dateTimeArg(vec);
    return dateTime.isValid() ? dateTime.toString("yyyy"_L1) : QString{};
}

QString month(const QStringList& vec)
{
    const QDateTime dateTime = dateTimeArg(vec);
    return dateTime.isValid() ? dateTime.toString("MM"_L1) : QString{};
}

QString dayOfMonth(const QStringList& vec)
{
    const QDateTime dateTime = dateTimeArg(vec);
    return dateTime.isValid() ? dateTime.toString("dd"_L1) : QString{};
}

QString date(const QStringList& vec)
{
    const QDateTime dateTime = dateTimeArg(vec);
    return dateTime.isValid() ? dateTime.toString("yyyy-MM-dd"_L1) : QString{};
}

QString time(const QStringList& vec)
{
    const QDateTime dateTime = dateTimeArg(vec);
    if(!dateTime.isValid() || !hasTimePrecision(vec.front())) {
        return {};
    }

    return dateTime.toString(hasSecondsPrecision(vec.front()) ? "hh:mm:ss"_L1 : "hh:mm"_L1);
}
} // namespace Fooyin::Scripting
