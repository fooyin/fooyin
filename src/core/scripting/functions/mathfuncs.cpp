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

#include "mathfuncs.h"

#include <random>

namespace {
    uint32_t getRandomNumber(uint32_t min, uint32_t max)
    {
        std::mt19937 gen{std::random_device{}()};
        std::uniform_int_distribution<uint32_t> dist{min, max};
        return dist(gen);
    }
} // namespace

namespace Fooyin::Scripting {
QString baseOperation(const QStringList& vec, const QChar op)
{
    if(vec.size() < 2) {
        return {};
    }
    double total = vec.front().toDouble();
    for(int i = 1; i < static_cast<int>(vec.size()); ++i) {
        const double num = vec.at(i).toDouble();
        switch(op.unicode()) {
            case('+'):
                total += num;
                break;
            case('-'):
                total -= num;
                break;
            case('*'):
                total *= num;
                break;
            case('/'):
                total /= num;
                break;
            default:
                break;
        }
    }
    return QString::number(total);
}
QString add(const QStringList& vec)
{
    return baseOperation(vec, QChar::fromLatin1('+'));
}

QString sub(const QStringList& vec)
{
    return baseOperation(vec, QChar::fromLatin1('-'));
}

QString mul(const QStringList& vec)
{
    return baseOperation(vec, QChar::fromLatin1('*'));
}

QString div(const QStringList& vec)
{
    return baseOperation(vec, QChar::fromLatin1('/'));
}

QString min(const QStringList& vec)
{
    if(vec.size() < 2) {
        return {};
    }
    auto result = std::min_element(vec.cbegin(), vec.cend(), [](const QString& str1, const QString& str2) {
        return (str1.toInt() < str2.toInt());
    });
    return *result;
}

QString max(const QStringList& vec)
{
    if(vec.size() < 2) {
        return {};
    }
    auto result = std::max_element(vec.cbegin(), vec.cend(), [](const QString& str1, const QString& str2) {
        return (str1.toInt() < str2.toInt());
    });
    return *result;
}

QString mod(const QStringList& vec)
{
    if(vec.size() < 2) {
        return {};
    }
    int total = vec.front().toInt();
    for(int i = 1; i < static_cast<int>(vec.size()); ++i) {
        total %= vec.at(i).toInt();
    }
    return QString::number(total);
}

QString rand()
{
    return QString::number(getRandomNumber(0, std::numeric_limits<uint32_t>::max()));
}

QString rand2(const QStringList& vec)
{
    if(vec.size() < 2) {
        return {};
    }

    return QString::number(getRandomNumber(vec.at(0).toInt(), vec.at(1).toInt()));
}
} // namespace Fooyin::Scripting
