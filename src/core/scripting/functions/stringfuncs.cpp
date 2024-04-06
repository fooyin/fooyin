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

#include "stringfuncs.h"

#include <utils/utils.h>

#include <QDir>

namespace Fooyin::Scripting {
QString num(const QStringList& vec)
{
    const qsizetype count = vec.size();

    if(vec.empty() || count > 2) {
        return {};
    }

    bool isInt{false};
    const int number = vec.at(0).toInt(&isInt);

    if(!isInt) {
        return {};
    }

    if(count == 1) {
        return vec.at(0);
    }

    isInt            = false;
    const int prefix = vec.at(1).toInt(&isInt);

    if(!isInt) {
        return vec.at(0);
    }

    return Utils::addLeadingZero(number, prefix);
}

QString replace(const QStringList& vec)
{
    const qsizetype count = vec.size();

    if(count != 3) {
        return {};
    }

    QString origStr{vec.at(0)};
    return origStr.replace(vec.at(1), vec.at(2));
}

QString slice(const QStringList& vec)
{
    const qsizetype count = vec.size();

    if(count < 2 || count > 3 || vec.at(0).isEmpty()) {
        return {};
    }

    bool posSuccess{false};

    const int pos = vec.at(1).toInt(&posSuccess);

    if(posSuccess) {
        if(count == 2) {
            return vec.at(0).sliced(pos);
        }
        bool numSuccess{false};
        const int num = vec.at(2).toInt(&numSuccess);
        if(numSuccess) {
            return vec.at(0).sliced(pos, num);
        }
    }
    return {};
}

QString chop(const QStringList& vec)
{
    const qsizetype count = vec.size();

    if(count != 2 || vec.at(0).isEmpty()) {
        return {};
    }

    bool numSuccess{false};

    const int num = vec.at(1).toInt(&numSuccess);

    if(numSuccess) {
        return vec.at(0).chopped(num);
    }

    return {};
}

QString left(const QStringList& vec)
{
    const qsizetype count = vec.size();

    if(count != 2) {
        return {};
    }

    bool numSuccess{false};

    const int num = vec.at(1).toInt(&numSuccess);

    if(numSuccess) {
        const QStringView str = vec.at(0);
        if(num >= 0 && num <= str.size()) {
            return str.first(num).toString();
        }
    }

    return {};
}

QString right(const QStringList& vec)
{
    const qsizetype count = vec.size();

    if(count != 2) {
        return {};
    }

    bool numSuccess{false};

    const int num = vec[1].toInt(&numSuccess);

    if(numSuccess) {
        const QStringView str = vec.at(0);
        if(num >= 0 && num <= str.size()) {
            return str.last(num).toString();
        }
    }

    return {};
}

ScriptResult strcmp(const QStringList& vec)
{
    const qsizetype count = vec.size();

    if(count != 2) {
        return {};
    }

    return {.value = {}, .cond = QString::compare(vec.at(0), vec.at(1), Qt::CaseSensitive) == 0};
}

ScriptResult strcmpi(const QStringList& vec)
{
    const qsizetype count = vec.size();

    if(count != 2) {
        return {};
    }

    return {.value = {}, .cond = QString::compare(vec.at(0), vec.at(1), Qt::CaseInsensitive) == 0};
}

QString sep()
{
    return QDir::separator();
}

QString swapPrefix(const QStringList& vec)
{
    const qsizetype count = vec.size();

    if(count < 1) {
        return {};
    }

    QStringList words = vec.front().split(QStringLiteral(" "), Qt::SkipEmptyParts);

    if(words.empty()) {
        return vec.front();
    }

    QStringList prefixes = vec.mid(1);

    if(prefixes.empty()) {
        prefixes = {QStringLiteral("A"), QStringLiteral("The")};
    }

    const QString firstWord = words.first();
    if(prefixes.contains(firstWord, Qt::CaseInsensitive)) {
        words.removeFirst();
        words.last().append(QStringLiteral(","));
        words.append(firstWord);
        return words.join(QStringLiteral(" "));
    }

    return vec.front();
}
} // namespace Fooyin::Scripting
