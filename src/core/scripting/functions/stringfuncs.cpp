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
#include "core/constants.h"

#include <utils/utils.h>

#include <QDir>
#include <QRegularExpression>

namespace Fooyin::Scripting {
QString num(const QStringList& vec)
{
    const qsizetype count = vec.size();

    if(vec.empty() || count > 2) {
        return {};
    }

    bool isInt{false};
    const int number = vec.at(0).toInt(&isInt);

    if(!isInt || count == 1) {
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

    if(count < 1) {
        return {};
    }

    if(count < 3) {
        return vec.front();
    }

    if(count == 3) {
        // Single replace call
        QString origStr{vec.front()};
        return origStr.replace(vec.at(1), vec.at(2));
    }

    // Arbitrary replacements
    // Much slower as we need to match all and then rebuild

    const QString& origStr = vec.front();
    std::map<qsizetype, QString, std::greater<>> replacements;

    for(qsizetype i{1}; i < count - 1; i += 2) {
        const QString& search  = vec[i];
        const QString& replace = vec[i + 1];
        const QRegularExpression regex{QRegularExpression::escape(search),
                                       QRegularExpression::UseUnicodePropertiesOption};
        QRegularExpressionMatchIterator matches = regex.globalMatch(origStr);

        while(matches.hasNext()) {
            const QRegularExpressionMatch match = matches.next();
            const qsizetype start               = match.capturedStart();
            replacements[start]                 = replace;
        }
    }

    QString result;
    qsizetype lastIndex = origStr.size();
    for(const auto& [pos, replace] : replacements) {
        if(pos >= lastIndex) {
            continue;
        }

        const QString& replacement = replacements.at(pos);
        const QRegularExpression regex{QRegularExpression::escape(origStr.mid(pos, replacement.length())),
                                       QRegularExpression::UseUnicodePropertiesOption};
        result.prepend(origStr.mid(pos, lastIndex - pos).replace(regex, replacement));
        lastIndex = pos;
    }
    result.prepend(origStr.left(lastIndex));

    return result;
}

QString ascii(const QStringList& vec)
{
    if(vec.empty()) {
        return {};
    }

    const QString normalizedStr = vec.front().normalized(QString::NormalizationForm_D);

    QString result;
    for(QChar ch : normalizedStr) {
        if(ch.unicode() < 128) {
            result.append(ch);
        }
        else if(ch.category() != QChar::Mark_NonSpacing) {
            const char asciiChar = ch.toLatin1();
            if(asciiChar != 0) {
                result.append(QChar::fromLatin1(asciiChar));
            }
        }
    }

    return result;
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

QString insert(const QStringList& vec)
{
    const qsizetype count = vec.size();

    if(count != 3) {
        return {};
    }

    bool numSuccess{false};
    const int index = vec.at(2).toInt(&numSuccess);

    if(numSuccess) {
        QString ret{vec.front()};
        ret.insert(index, vec.at(1));
        return ret;
    }

    return {};
}

QString substr(const QStringList& vec)
{
    const qsizetype count = vec.size();

    if(count != 3) {
        return {};
    }

    bool fromSuccess{false};
    bool toSuccess{false};
    const int from = vec.at(1).toInt(&fromSuccess);
    const int to   = vec.at(2).toInt(&toSuccess);

    if(fromSuccess && toSuccess && to >= from) {
        return vec.front().mid(from, to - from + 1);
    }

    return {};
}

QString len(const QStringList& vec)
{
    if(vec.empty()) {
        return {};
    }

    return QString::number(vec.front().length());
}

QString longest(const QStringList& vec)
{
    if(vec.empty()) {
        return {};
    }

    return *std::max_element(vec.cbegin(), vec.cend());
}

ScriptResult strcmp(const QStringList& vec)
{
    const qsizetype count = vec.size();

    if(count != 2) {
        return {};
    }

    return {.value = {}, .cond = QString::compare(vec.at(0), vec.at(1), Qt::CaseSensitive) == 0};
}

ScriptResult stricmp(const QStringList& vec)
{
    const qsizetype count = vec.size();

    if(count != 2) {
        return {};
    }

    return {.value = {}, .cond = QString::compare(vec.at(0), vec.at(1), Qt::CaseInsensitive) == 0};
}

ScriptResult longer(const QStringList& vec)
{
    const qsizetype count = vec.size();

    if(count != 2) {
        return {};
    }

    return {.value = {}, .cond = vec.at(0).length() > vec.at(1).length()};
}

QString sep()
{
    return QDir::separator();
}

QString crlf()
{
    return QStringLiteral("\n");
}

QString tab(const QStringList& vec)
{
    if(vec.empty()) {
        return QStringLiteral("\t");
    }

    bool numSuccess{false};
    const int num = vec.front().toInt(&numSuccess);

    if(numSuccess) {
        return QStringLiteral("\t").repeated(num);
    }

    return QStringLiteral("\t");
}

QString swapPrefix(const QStringList& vec)
{
    const qsizetype count = vec.size();

    if(count < 1) {
        return {};
    }

    QStringList result;

    const auto strings = vec.front().split(QLatin1String{Constants::UnitSeparator}, Qt::SkipEmptyParts);
    for(const QString& str : strings) {
        QStringList words = str.split(QStringLiteral(" "), Qt::SkipEmptyParts);

        if(words.empty()) {
            result.append(vec.front());
            continue;
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
            result.append(words.join(QStringLiteral(" ")));
        }
        else {
            result.append(str);
        }
    }

    return result.join(QLatin1String{Constants::UnitSeparator});
}

QString stripPrefix(const QStringList& vec)
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
        return words.join(QStringLiteral(" "));
    }

    return vec.front();
}

QString pad(const QStringList& vec)
{
    const qsizetype count = vec.size();

    if(count < 2 || count > 3) {
        return {};
    }

    const QString& str = vec.at(0);
    bool numSuccess{false};
    const int len = vec.at(1).toInt(&numSuccess);

    if(!numSuccess) {
        return {};
    }

    if(count == 3) {
        const QString& padChar = vec.at(2);
        if(!padChar.isEmpty()) {
            return str.leftJustified(len, padChar.front());
        }
    }

    return str.leftJustified(len);
}

QString padRight(const QStringList& vec)
{
    const qsizetype count = vec.size();

    if(count < 2 || count > 3) {
        return {};
    }

    const QString& str = vec.at(0);
    bool numSuccess{false};
    const int len = vec.at(1).toInt(&numSuccess);

    if(!numSuccess) {
        return {};
    }

    if(count == 3) {
        const QString& padChar = vec.at(2);
        if(!padChar.isEmpty()) {
            return str.rightJustified(len, padChar.front());
        }
    }

    return str.rightJustified(len);
}

QString repeat(const QStringList& vec)
{
    const qsizetype count = vec.size();

    if(count != 2) {
        return {};
    }

    bool numSuccess{false};
    const int num = vec.at(1).toInt(&numSuccess);

    if(numSuccess) {
        return vec.at(0).repeated(num);
    }

    return {};
}

QString trim(const QStringList& vec)
{
    if(vec.empty()) {
        return {};
    }

    return vec.front().trimmed();
}

QString upper(const QStringList& vec)
{
    if(vec.empty()) {
        return {};
    }

    return vec.front().toUpper();
}

QString lower(const QStringList& vec)
{
    if(vec.empty()) {
        return {};
    }

    return vec.front().toLower();
}

QString abbr(const QStringList& vec)
{
    const qsizetype count = vec.size();

    if(count < 1) {
        return {};
    }

    if(count == 2) {
        bool numSuccess{false};
        const int len = vec.at(1).toInt(&numSuccess);

        if(numSuccess && vec.front().length() <= len) {
            return vec.front();
        }
    }

    QString str{vec.front()};
    static const QRegularExpression stripRegex{QStringLiteral("[()]")};
    str.replace(stripRegex, QStringLiteral(" "));

    static const QRegularExpression regex{QStringLiteral(R"((?<!\S)[^\s])"),
                                          QRegularExpression::UseUnicodePropertiesOption};
    const auto matches = regex.globalMatch(str);

    QString abbreviated;
    for(const auto& match : matches) {
        abbreviated.append(match.captured(0));
    }

    return abbreviated;
}

QString caps(const QStringList& vec)
{
    if(vec.empty()) {
        return {};
    }

    return Utils::capitalise(vec.front());
}

QString directory(const QStringList& vec)
{
    const qsizetype count = vec.size();

    if(count < 2) {
        return {};
    }

    QDir dir{vec.front()};

    if(count == 2) {
        bool numSuccess{false};
        int level = vec.at(1).toInt(&numSuccess);
        if(numSuccess) {
            while(level > 0) {
                --level;
                dir.cdUp();
            }
        }
    }

    return dir.dirName();
}

QString directoryPath(const QStringList& vec)
{
    if(vec.empty()) {
        return {};
    }

    const QDir dir{vec.front()};
    return dir.absolutePath();
}

QString ext(const QStringList& vec)
{
    if(vec.empty()) {
        return {};
    }

    const QFileInfo file{vec.front()};
    return file.suffix();
}

QString filename(const QStringList& vec)
{
    if(vec.empty()) {
        return {};
    }

    const QFileInfo file{vec.front()};
    return file.completeBaseName();
}

QString progress(const QStringList& vec)
{
    if(vec.size() != 5) {
        return {};
    }

    const int pos   = vec.at(0).toInt();
    const int range = vec.at(1).toInt();
    const int len   = vec.at(2).toInt();

    const auto ratio = static_cast<double>(pos) / range;

    const auto char1Pos = static_cast<int>(ratio * len);

    const QString& char1 = vec.at(3);
    const QString& char2 = vec.at(4);

    QString progressBar;

    for(int i{0}; i < len; ++i) {
        if(i == char1Pos) {
            progressBar.append(char1);
        }
        else {
            progressBar.append(char2);
        }
    }

    return progressBar;
}

QString progress2(const QStringList& vec)
{
    if(vec.size() != 5) {
        return {};
    }

    const int pos   = vec.at(0).toInt();
    const int range = vec.at(1).toInt();
    const int len   = vec.at(2).toInt();

    const auto ratio = static_cast<double>(pos) / range;

    const int char1Count = static_cast<int>(ratio * len);
    const int char2Count = len - char1Count;

    const QString& char1 = vec.at(3);
    const QString& char2 = vec.at(4);

    QString progressBar;

    for(int i{0}; i < char1Count; ++i) {
        progressBar.append(char1);
    }
    for(int i{0}; i < char2Count; ++i) {
        progressBar.append(char2);
    }

    return progressBar;
}
} // namespace Fooyin::Scripting
