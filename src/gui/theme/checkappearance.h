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

#include "buttonappearance.h"

namespace Fooyin {
struct CheckAppearance
{
    struct State
    {
        double borderWidth{1.0};
        ColourSource backgroundColour{QPalette::Base};
        ColourSource checkedBackgroundColour{QPalette::Highlight};
        ColourSource borderColour{QPalette::Mid};
        ColourSource checkedBorderColour{QPalette::Highlight};
        ColourSource markColour{QPalette::HighlightedText};
        ColourSource tintColour{QPalette::Highlight};
        ColourSource checkedTintColour{QPalette::WindowText};
        int tintOpacity{0};
        ColourSource haloColour{QPalette::Highlight};
        int haloOpacity{0};

        bool operator==(const State& other) const = default;
    };

    int indicatorExtent{20};
    int labelSpacing{8};
    double inset{2.5};
    double radius{3.0};
    double markSize{8.0};
    double markWidth{1.8};
    State normal;
    State hovered{.borderWidth = 1.5, .borderColour = QPalette::Highlight, .tintOpacity = 10, .haloOpacity = 18};
    State pressed{.borderWidth = 1.5, .borderColour = QPalette::Highlight, .tintOpacity = 22, .haloOpacity = 28};
    State disabled;
    ButtonFocus focus{.spacing = 0.25};

    bool operator==(const CheckAppearance& other) const = default;
};

CheckAppearance defaultRadioButtonAppearance();
std::expected<CheckAppearance, QString> resolveCheckBoxAppearance(const QJsonObject& appearance);
std::expected<CheckAppearance, QString> resolveRadioButtonAppearance(const QJsonObject& appearance);
} // namespace Fooyin
