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

#include <gui/coloursource.h>

#include <QRectF>

#include <vector>

namespace Fooyin {
struct LinearGradientSource
{
    struct Stop
    {
        double position;
        ColourSource colour;

        bool operator==(const Stop& other) const = default;
    };

    enum class Direction : uint8_t
    {
        AlongControl = 0,
        AcrossControl,
        Horizontal,
        Vertical,
    };

    Direction direction{Direction::AlongControl};
    std::vector<Stop> stops{{.position = 0.0, .colour = QPalette::Button},
                            {.position = 1.0, .colour = QPalette::Highlight}};

    bool operator==(const LinearGradientSource& other) const = default;
};

using BrushSource = std::variant<QPalette::ColorRole, QColor, LinearGradientSource>;

FYGUI_EXPORT QBrush resolveBrush(const BrushSource& brush, const QPalette& palette, QPalette::ColorGroup group,
                                 const QRectF& bounds, Qt::Orientation controlOrientation = Qt::Horizontal,
                                 bool upsideDown = false);
FYGUI_EXPORT QString brushSourceName(const BrushSource& brush);
FYGUI_EXPORT QJsonObject brushSourceToJson(const BrushSource& brush);
FYGUI_EXPORT std::expected<BrushSource, QString> readBrushSource(const QJsonValue& value, const QString& path,
                                                                 const BrushSource& fallback = QPalette::Window);
} // namespace Fooyin
