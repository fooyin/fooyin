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

#pragma once

#include "fyutils_export.h"

#include <QString>

#include <chrono>

class QFontMetrics;
class QJsonValue;
class QKeySequence;

namespace Fooyin::Utils {
FYUTILS_EXPORT QString readMultiLineString(const QJsonValue& value);
FYUTILS_EXPORT QString elideTextWithBreaks(const QString& text, const QFontMetrics& fontMetrics, int maxWidth,
                                           Qt::TextElideMode mode);
FYUTILS_EXPORT QString capitalise(const QString& str);
FYUTILS_EXPORT QByteArray detectEncoding(const QByteArray& content);

FYUTILS_EXPORT QString msToString(std::chrono::milliseconds ms, bool includeMs);
FYUTILS_EXPORT QString msToString(uint64_t ms);
FYUTILS_EXPORT QString formatFileSize(uint64_t bytes, bool includeBytes = false);
FYUTILS_EXPORT QString addLeadingZero(int number, int leadingCount);
FYUTILS_EXPORT QString appendShortcut(const QString& str, const QKeySequence& shortcut);

FYUTILS_EXPORT int levenshteinDistance(const QString& first, const QString& second, Qt::CaseSensitivity cs);
FYUTILS_EXPORT int similarityRatio(const QString& first, const QString& second, Qt::CaseSensitivity cs);
} // namespace Fooyin::Utils
