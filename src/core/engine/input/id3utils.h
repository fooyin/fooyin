/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
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

#include "fycore_export.h"

#include <QByteArrayView>
#include <QString>
#include <QStringList>

#include <optional>

namespace Fooyin::Id3Utils {
struct TimedMetadata
{
    QString title;
    QString artist;
    QString station;
};

[[nodiscard]] FYCORE_EXPORT std::optional<TimedMetadata> parseTimedMetadata(QByteArrayView data);
[[nodiscard]] QStringList splitStandardField(const QString& field, const QStringList& values,
                                             bool splitSemicolonSeparated);
[[nodiscard]] QStringList splitExtraField(const QString& field, const QStringList& values,
                                          bool splitSemicolonSeparated);
} // namespace Fooyin::Id3Utils
