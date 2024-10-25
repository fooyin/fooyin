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

#include <QFontMetrics>
#include <QJsonArray>
#include <QJsonValue>
#include <QKeySequence>

#include <unicode/ucsdet.h>

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

QString elideTextWithBreaks(const QString& text, const QFontMetrics& fontMetrics, int maxWidth, Qt::TextElideMode mode)
{
    QStringList lines = text.split(QChar::LineSeparator);
    for(auto i{0}; i < lines.size(); ++i) {
        lines[i] = fontMetrics.elidedText(lines[i], mode, maxWidth);
    }
    return lines.join(QChar::LineSeparator);
}

QString capitalise(const QString& str)
{
    QStringList parts = str.split(u' ', Qt::SkipEmptyParts);

    for(auto& part : parts) {
        part.replace(0, 1, part[0].toUpper());
    }

    return parts.join(u' ');
}

QByteArray detectEncoding(const QByteArray& content)
{
    QByteArray encoding;
    UErrorCode status{U_ZERO_ERROR};

    UCharsetDetector* csd = ucsdet_open(&status);
    ucsdet_setText(csd, content.constData(), static_cast<int32_t>(content.length()), &status);

    const UCharsetMatch* ucm = ucsdet_detect(csd, &status);
    if(U_SUCCESS(status) && ucm) {
        const char* cname = ucsdet_getName(ucm, &status);
        if(U_SUCCESS(status)) {
            encoding = QByteArray{cname};
        }
    }

    ucsdet_close(csd);

    return encoding;
}

QString msToString(std::chrono::milliseconds ms, bool includeMs)
{
    const auto weeks = duration_cast<std::chrono::weeks>(ms);
    ms -= duration_cast<std::chrono::milliseconds>(weeks);

    const auto days = duration_cast<std::chrono::days>(ms);
    ms -= duration_cast<std::chrono::milliseconds>(days);

    const auto hours = duration_cast<std::chrono::hours>(ms);
    ms -= duration_cast<std::chrono::milliseconds>(hours);

    const auto minutes = duration_cast<std::chrono::minutes>(ms);
    ms -= duration_cast<std::chrono::milliseconds>(minutes);

    const auto seconds = duration_cast<std::chrono::seconds>(ms);
    ms -= duration_cast<std::chrono::milliseconds>(seconds);

    const auto milliseconds = ms.count();

    QString formattedTime;

    if(weeks.count() > 0) {
        formattedTime += QStringLiteral("%1wk ").arg(weeks.count());
    }
    if(days.count() > 0) {
        formattedTime += QStringLiteral("%1d ").arg(days.count());
    }
    if(hours.count() > 0) {
        formattedTime += QStringLiteral("%1:").arg(hours.count(), 2, 10, QLatin1Char{'0'});
    }

    formattedTime += QStringLiteral("%1:%2")
                         .arg(minutes.count(), 2, 10, QLatin1Char{'0'})
                         .arg(seconds.count(), 2, 10, QLatin1Char{'0'});

    if(includeMs) {
        formattedTime += QStringLiteral(".%1").arg(milliseconds, 3, 10, QLatin1Char{'0'});
    }

    return formattedTime;
}

QString msToString(uint64_t ms)
{
    return msToString(static_cast<std::chrono::milliseconds>(ms), false);
}

QString formatFileSize(uint64_t bytes, bool includeBytes)
{
    static const QStringList units = {QStringLiteral("bytes"), QStringLiteral("KB"), QStringLiteral("MB"),
                                      QStringLiteral("GB"), QStringLiteral("TB")};
    auto size                      = static_cast<double>(bytes);
    int unitIndex{0};

    while(size >= 1024 && unitIndex < units.size() - 1) {
        size /= 1024;
        ++unitIndex;
    }

    if(unitIndex >= units.size()) {
        return {};
    }

    QString formattedSize = QStringLiteral("%1 %2").arg(QString::number(size, 'f', 1), units.at(unitIndex));

    if(unitIndex == 0) {
        return formattedSize;
    }

    if(includeBytes) {
        formattedSize.append(QStringLiteral(" (%3 bytes)").arg(bytes));
    }

    return formattedSize;
}

QString addLeadingZero(int number, int leadingCount)
{
    return QStringLiteral("%1").arg(number, leadingCount, 10, QLatin1Char{'0'});
}

QString appendShortcut(const QString& str, const QKeySequence& shortcut)
{
    QString string = str;
    string.remove(QChar::fromLatin1('&'));
    return QString::fromLatin1("<div style=\"white-space:pre\">%1 "
                               "<span style=\"color: gray; font-size: small\">%2</span></div>")
        .arg(string, shortcut.toString(QKeySequence::NativeText));
}

int levenshteinDistance(const QString& first, const QString& second, Qt::CaseSensitivity cs)
{
    // Modified from https://qgis.org/api/qgsstringutils_8cpp_source.html

    int firstLength  = static_cast<int>(first.length());
    int secondLength = static_cast<int>(second.length());

    if(first.isEmpty()) {
        return secondLength;
    }
    if(second.isEmpty()) {
        return firstLength;
    }

    QString a = (cs == Qt::CaseSensitive ? first : first.toLower());
    QString b = (cs == Qt::CaseSensitive ? second : second.toLower());

    const QChar* aChar = a.constData();
    const QChar* bChar = b.constData();

    int commonPrefixLen{0};
    while(firstLength > 0 && secondLength > 0 && *aChar == *bChar) {
        ++commonPrefixLen;
        --firstLength;
        --secondLength;
        ++aChar;
        ++bChar;
    }

    while(firstLength > 0 && secondLength > 0
          && (a.at(commonPrefixLen + firstLength - 1) == b.at(commonPrefixLen + secondLength - 1))) {
        firstLength--;
        secondLength--;
    }

    if(firstLength == 0) {
        return secondLength;
    }
    if(secondLength == 0) {
        return firstLength;
    }

    if(firstLength > secondLength) {
        std::swap(a, b);
        std::swap(firstLength, secondLength);
    }

    std::vector<int> col(secondLength + 1, 0);
    std::vector<int> prevCol(secondLength + 1);
    std::iota(prevCol.begin(), prevCol.end(), 0);

    const QChar* bStart{bChar};
    for(int i{0}; i < firstLength; ++i) {
        col[0] = i + 1;
        bChar  = bStart;
        for(int j{0}; j < secondLength; ++j) {
            col[j + 1]
                = std::min(std::min(1 + col.at(j), 1 + prevCol.at(1 + j)), prevCol[j] + ((*aChar == *bChar) ? 0 : 1));
            ++bChar;
        }
        col.swap(prevCol);
        ++aChar;
    }

    return prevCol.at(secondLength);
}

int similarityRatio(const QString& first, const QString& second, Qt::CaseSensitivity cs)
{
    const auto maxLength = std::max(first.length(), second.length());

    if(maxLength == 0) {
        return 100;
    }

    const int distance = levenshteinDistance(first, second, cs);

    const double similarity = (1.0 - static_cast<double>(distance) / static_cast<double>(maxLength)) * 100;
    return static_cast<int>(similarity);
}

} // namespace Fooyin::Utils
