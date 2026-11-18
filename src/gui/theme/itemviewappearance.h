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
struct ItemViewAppearance
{
    struct State
    {
        BrushSource backgroundColour{QPalette::Base};
        int backgroundOpacity{0};
        int backgroundLightness{100};
        double borderWidth{0.0};
        ColourSource borderColour{QPalette::Base};
        int borderOpacity{0};

        bool operator==(const State& other) const = default;
    };

    struct Focus
    {
        double width{0.0};
        double inset{0.0};
        ColourSource colour{QPalette::Highlight};
        int opacity{100};
        int selectedOpacity{65};
        int selectedLightness{125};
        bool keyboardOnly{true};

        bool operator==(const Focus& other) const = default;
    };

    struct Branch
    {
        double lineWidth{1.0};
        ColourSource lineColour{QPalette::Text};
        int lineOpacity{25};
        ColourSource selectedLineColour{QPalette::HighlightedText};
        int selectedLineOpacity{45};
        int arrowExtent{12};
        bool filledArrows{false};
        double arrowSize{6.0};
        double arrowWidth{1.5};
        ColourSource arrowColour{QPalette::Text};
        ColourSource hoveredArrowColour{QPalette::Highlight};
        ColourSource selectedArrowColour{QPalette::HighlightedText};

        bool operator==(const Branch& other) const = default;
    };

    int verticalPadding{5};
    int textPadding{0};
    int treeTextPadding{3};
    double horizontalInset{1.0};
    double verticalInset{1.0};
    double radius{4.0};
    double customBorderWidth{1.0};
    State normal;
    State alternate{.backgroundColour = QPalette::AlternateBase, .backgroundOpacity = 100};
    State hovered{.backgroundColour    = QPalette::Highlight,
                  .backgroundOpacity   = 30,
                  .backgroundLightness = 100,
                  .borderWidth         = 1.0,
                  .borderColour        = QPalette::Highlight,
                  .borderOpacity       = 80};
    State selected{.backgroundColour    = QPalette::Highlight,
                   .backgroundOpacity   = 100,
                   .backgroundLightness = 100,
                   .borderWidth         = 1.0,
                   .borderColour        = QPalette::Highlight,
                   .borderOpacity       = 100};
    State selectedHovered{.backgroundColour    = QPalette::Highlight,
                          .backgroundOpacity   = 100,
                          .backgroundLightness = 110,
                          .borderWidth         = 1.0,
                          .borderColour        = QPalette::Highlight,
                          .borderOpacity       = 100};
    Focus focus;
    Branch branch;

    bool operator==(const ItemViewAppearance& other) const = default;
};

std::expected<ItemViewAppearance, QString> resolveItemViewAppearance(const QJsonObject& appearance);
} // namespace Fooyin
