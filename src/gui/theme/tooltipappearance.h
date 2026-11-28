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

#include <QJsonObject>

#include <expected>

namespace Fooyin {
struct ToolTipAppearance
{
    int padding{5};
    double radius{4.0};
    double borderWidth{1.0};
    ColourSource backgroundColour{QPalette::ToolTipBase};
    int backgroundOpacity{100};
    ColourSource foregroundColour{QPalette::ToolTipText};
    ColourSource borderColour{QPalette::ToolTipText};
    int borderOpacity{25};

    bool operator==(const ToolTipAppearance& other) const = default;
};

std::expected<ToolTipAppearance, QString> resolveToolTipAppearance(const QJsonObject& appearance);
} // namespace Fooyin
