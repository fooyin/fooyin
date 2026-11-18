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
struct ToolButtonAppearance
{
    using Surface = ButtonSurface;
    using Focus   = ButtonFocus;

    struct Menu
    {
        int indicatorWidth{18};
        bool separatorVisible{true};
        double separatorWidth{1.0};
        double separatorInset{6.0};
        ColourSource separatorColour{QPalette::Mid};
        Surface normal;
        Surface hovered;
        Surface pressed;
        Surface disabled;
        Surface checked;

        bool operator==(const Menu& other) const = default;
    };

    int minimumWidth{0};
    int minimumHeight{0};
    int horizontalPadding{4};
    int verticalPadding{4};
    double radius{4.0};
    bool flatBackground{false};
    bool flatBorder{false};
    Surface normal;
    Surface hovered{.borderColour = QPalette::Highlight, .tintOpacity = 10};
    Surface pressed{.tintOpacity = 22};
    Surface disabled;
    Surface checked{.backgroundVisible = false,
                    .borderVisible     = false,
                    .tintIcon          = true,
                    .iconColour        = QPalette::Highlight,
                    .borderColour      = QPalette::Highlight,
                    .tintOpacity       = 16};
    Focus focus;
    Menu menu{.normal = normal, .hovered = hovered, .pressed = pressed, .disabled = disabled, .checked = checked};

    bool operator==(const ToolButtonAppearance& other) const = default;
};

std::expected<ToolButtonAppearance, QString> resolveToolButtonAppearance(const QJsonObject& appearance);
} // namespace Fooyin
