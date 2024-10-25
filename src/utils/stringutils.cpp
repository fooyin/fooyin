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
