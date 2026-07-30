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

#include "fyutils_export.h"

#include <QJsonValue>
#include <QString>

#include <functional>

namespace Fooyin::Utils {
using JsonArrayItemIdentity = std::function<QString(const QJsonValue&)>;

/**
 * Performs a recursive three-way merge of JSON values.
 *
 * Changes made only in @p first or @p second relative to @p base are retained. When both sides change the same value,
 * @p second wins.
 *
 * Objects are merged by key. Equal-length arrays are merged by position unless @p arrayItemIdentity is supplied.
 * Identified arrays are matched by identity and ordered according to @p second, with items added only in @p first
 * appended. If an identity is empty or duplicated, the conflicting array resolves to @p second.
 */
[[nodiscard]] FYUTILS_EXPORT QJsonValue mergeJsonThreeWay(const QJsonValue& base, const QJsonValue& first,
                                                          const QJsonValue& second,
                                                          const JsonArrayItemIdentity& arrayItemIdentity = {});
} // namespace Fooyin::Utils
