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

#include "mathfuncs.h"

namespace Fy::Core::Scripting {
QString baseOperation(const StringList& vec, const QChar op)
{
    if(vec.size() < 2) {
        return {};
    }
    bool success;
    const double num = vec.front().toDouble(&success);
    if(!success) {
        return {};
    }
    double total = num;
    for(int i = 1; i < static_cast<int>(vec.size()); ++i) {
        bool success;
        const double num = vec[i].toDouble(&success);
        if(!success) {
            return {};
        }
        switch(op.cell()) {
            case '-':
                total -= num;
                break;
            case '*':
                total *= num;
                break;
            case '/':
                total /= num;
                break;
        }
    }
    return QString::number(total);
}
QString add(const StringList& vec)
{
    if(vec.size() < 2) {
        return {};
    }
    double total = 0;
    for(const auto& numStr : vec) {
        bool success;
        const double num = numStr.toDouble(&success);
        if(!success) {
            return {};
        }
        total += num;
    }
    return QString::number(total);
}

QString sub(const StringList& vec)
{
    return baseOperation(vec, '-');
}

QString mul(const StringList& vec)
{
    return baseOperation(vec, '*');
}

QString div(const StringList& vec)
{
    return baseOperation(vec, '/');
}

QString min(const StringList& vec)
{
    if(vec.size() < 2) {
        return {};
    }
    auto result = std::min_element(vec.cbegin(), vec.cend(), [](const QString& str1, const QString& str2) {
        return (str1.toInt() < str2.toInt());
    });
    return *result;
}

QString max(const StringList& vec)
{
    if(vec.size() < 2) {
        return {};
    }
    auto result = std::max_element(vec.cbegin(), vec.cend(), [](const QString& str1, const QString& str2) {
        return (str1.toInt() < str2.toInt());
    });
    return *result;
}

} // namespace Fy::Core::Scripting
