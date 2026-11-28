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

#include <expected>

namespace Fooyin {
struct HeaderAppearance
{
    struct State
    {
        BrushSource backgroundColour{QPalette::Button};
        int backgroundOpacity{100};
        ColourSource tintColour{QPalette::Highlight};
        int tintOpacity{0};
        ColourSource foregroundColour{QPalette::ButtonText};
        bool tintIcon{false};
        ColourSource iconColour{QPalette::ButtonText};

        bool operator==(const State& other) const = default;
    };

    struct Border
    {
        double width{1.0};
        ColourSource colour{QPalette::Mid};
        int opacity{65};
        int shadingOpacity{0};

        bool operator==(const Border& other) const = default;
    };

    struct Separator
    {
        double width{1.0};
        double inset{4.0};
        ColourSource colour{QPalette::Mid};
        int opacity{50};

        bool operator==(const Separator& other) const = default;
    };

    struct Arrow
    {
        int extent{12};
        double size{6.0};
        double width{1.5};
        ColourSource colour{QPalette::Highlight};

        bool operator==(const Arrow& other) const = default;
    };

    struct Focus
    {
        double width{1.0};
        double inset{1.5};
        double radius{0.0};
        ColourSource colour{QPalette::Highlight};
        bool keyboardOnly{false};

        bool operator==(const Focus& other) const = default;
    };

    int horizontalPadding{8};
    int verticalPadding{4};
    int minimumThickness{24};
    int spacing{4};
    int iconSize{16};
    BrushSource emptyColour{QPalette::Button};
    State normal;
    State hovered{.tintOpacity = 8};
    State pressed{.tintOpacity = 20};
    State sorted;
    State selected{.tintOpacity = 12};
    State dragTarget{.tintOpacity = 20};
    State disabled;
    Border border;
    Separator separator;
    Arrow arrow;
    Focus focus;

    bool operator==(const HeaderAppearance& other) const = default;
};

std::expected<HeaderAppearance, QString> resolveHeaderAppearance(const QJsonObject& appearance);
} // namespace Fooyin
