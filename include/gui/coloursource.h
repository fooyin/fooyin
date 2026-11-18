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

#include "fygui_export.h"

#include <QBrush>
#include <QColor>
#include <QJsonObject>
#include <QPalette>

#include <expected>
#include <variant>

namespace Fooyin {
using ColourSource = std::variant<QPalette::ColorRole, QColor>;

FYGUI_EXPORT QBrush resolveBrush(const ColourSource& colour, const QPalette& palette, QPalette::ColorGroup group);
FYGUI_EXPORT QString colourSourceName(const ColourSource& colour);
FYGUI_EXPORT QJsonObject colourSourceToJson(const ColourSource& colour);
FYGUI_EXPORT std::expected<ColourSource, QString> readColourSource(const QJsonValue& value, const QString& path,
                                                                   const ColourSource& fallback = QPalette::Window);
} // namespace Fooyin
