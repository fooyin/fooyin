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
struct SliderAppearance
{
    struct GrooveState
    {
        int thickness{4};
        double radius{2.0};
        BrushSource backgroundColour{QPalette::Mid};
        BrushSource filledColour{QPalette::Highlight};

        bool operator==(const GrooveState& other) const = default;
    };

    struct Groove
    {
        GrooveState normal;
        GrooveState hovered;
        GrooveState pressed;
        GrooveState disabled;

        bool operator==(const Groove& other) const = default;
    };

    struct Handle
    {
        double inset{3.0};
        double borderWidth{2.0};
        int cornerRadius{100};
        BrushSource backgroundColour{QPalette::Button};
        ColourSource borderColour{QPalette::Mid};

        bool operator==(const Handle& other) const = default;
    };

    struct Focus
    {
        double borderWidth{2.0};
        ColourSource borderColour{QPalette::Highlight};

        bool operator==(const Focus& other) const = default;
    };

    int handleExtent{20};
    int tickmarkOffset{4};
    Groove groove;
    Handle normal;
    Handle hovered{.inset = 2.0, .borderColour = QPalette::Highlight};
    Handle pressed{.inset = 4.0, .backgroundColour = QPalette::Highlight, .borderColour = QPalette::Highlight};
    Handle disabled{.borderColour = QPalette::Mid};
    Focus focus;

    bool operator==(const SliderAppearance& other) const = default;
};

std::expected<SliderAppearance, QString> resolveSliderAppearance(const QJsonObject& appearance);
} // namespace Fooyin
