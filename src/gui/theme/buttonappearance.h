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

#include <gui/brushsource.h>

#include <QJsonObject>
#include <QPalette>

#include <expected>

namespace Fooyin {
struct ButtonSurface
{
    bool backgroundVisible{true};
    bool borderVisible{true};
    bool tintIcon{false};
    double borderWidth{1.0};
    BrushSource backgroundColour{QPalette::Button};
    ColourSource foregroundColour{QPalette::ButtonText};
    ColourSource iconColour{QPalette::ButtonText};
    ColourSource borderColour{QPalette::Mid};
    ColourSource tintColour{QPalette::Highlight};
    int tintOpacity{0};

    bool operator==(const ButtonSurface& other) const = default;
};

struct ButtonFocus
{
    double borderWidth{1.5};
    double spacing{1.0};
    ColourSource borderColour{QPalette::Highlight};

    bool operator==(const ButtonFocus& other) const = default;
};

namespace Appearance {
std::expected<ButtonSurface, QString> readButtonSurface(const QJsonValue& value, const QString& path,
                                                        const ButtonSurface& defaults);
std::expected<ButtonFocus, QString> readButtonFocus(const QJsonValue& value, const QString& path,
                                                    const ButtonFocus& defaults);
} // namespace Appearance
} // namespace Fooyin
