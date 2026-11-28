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
struct MenuAppearance
{
    struct Surface
    {
        BrushSource backgroundColour{QPalette::Window};
        int backgroundOpacity{100};
        double borderWidth{1.0};
        ColourSource borderColour{QPalette::Mid};
        int borderOpacity{100};
        double radius{5.5};
        int reliefOpacity{0};

        bool operator==(const Surface& other) const = default;
    };

    struct State
    {
        BrushSource backgroundColour{QPalette::Highlight};
        int backgroundOpacity{0};
        double borderWidth{0.0};
        ColourSource borderColour{QPalette::Highlight};
        ColourSource foregroundColour{QPalette::Text};
        bool tintIcon{false};
        ColourSource iconColour{QPalette::Text};

        bool operator==(const State& other) const = default;
    };

    struct Separator
    {
        int height{6};
        int sectionPadding{5};
        int minimumWidth{80};
        int inset{4};
        int spacing{4};
        double lineWidth{1.0};
        ColourSource lineColour{QPalette::Mid};
        ColourSource foregroundColour{QPalette::Text};
        bool bold{true};

        bool operator==(const Separator& other) const = default;
    };

    struct Focus
    {
        double borderWidth{1.0};
        double inset{1.5};
        double radius{4.0};
        ColourSource borderColour{QPalette::Highlight};
        bool keyboardOnly{false};

        bool operator==(const Focus& other) const = default;
    };

    struct Bar
    {
        int horizontalPadding{8};
        int verticalPadding{5};
        int horizontalMargin{2};
        int verticalMargin{1};
        int spacing{2};
        double inset{1.5};
        double radius{4.0};
        Surface panel{.borderWidth = 0.0, .radius = 0.0};
        State normal{.foregroundColour = QPalette::WindowText, .iconColour = QPalette::WindowText};
        State selected{.backgroundOpacity = 10,
                       .foregroundColour  = QPalette::WindowText,
                       .iconColour        = QPalette::WindowText};
        State pressed{.backgroundOpacity = 22,
                      .foregroundColour  = QPalette::WindowText,
                      .iconColour        = QPalette::WindowText};
        State disabled{.foregroundColour = QPalette::WindowText, .iconColour = QPalette::WindowText};
        Focus focus;

        bool operator==(const Bar& other) const = default;
    };

    int horizontalPadding{4};
    int verticalPadding{5};
    int minimumWidth{120};
    int minimumHeight{0};
    int horizontalMargin{2};
    int verticalMargin{2};
    int columnSpacing{4};
    int checkExtent{20};
    int iconExtent{20};
    int iconSize{16};
    int arrowExtent{12};
    double arrowSize{6.0};
    double arrowWidth{1.5};
    int shortcutSpacing{12};
    int auxiliaryExtent{16};
    double inset{1.5};
    double radius{4.0};
    Surface panel;
    State normal;
    State selected{.backgroundOpacity = 16};
    State pressed{.backgroundOpacity = 22};
    State disabled;
    Separator separator;
    Bar bar;

    bool operator==(const MenuAppearance& other) const = default;
};

std::expected<MenuAppearance, QString> resolveMenuAppearance(const QJsonObject& appearance);
} // namespace Fooyin
