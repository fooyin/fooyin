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
struct TabAppearance
{
    enum class Design : uint8_t
    {
        Flat = 0,
        Connected,
    };

    enum class Alignment : uint8_t
    {
        Leading = 0,
        Centre,
        Trailing,
    };

    struct Surface
    {
        BrushSource backgroundColour{QPalette::Highlight};
        int backgroundOpacity{0};
        double borderWidth{0.0};
        ColourSource borderColour{QPalette::Highlight};
        ColourSource foregroundColour{QPalette::WindowText};
        bool tintIcon{false};
        ColourSource iconColour{QPalette::WindowText};

        bool operator==(const Surface& other) const = default;
    };

    struct Accent
    {
        enum class Edge : uint8_t
        {
            Page = 0,
            Outer,
        };

        double width{2.0};
        double margin{8.0};
        double inset{2.0};
        Edge edge{Edge::Page};
        bool rounded{true};
        ColourSource colour{QPalette::Highlight};

        bool operator==(const Accent& other) const = default;
    };

    struct Focus
    {
        double borderWidth{1.0};
        double inset{2.5};
        double radius{3.0};
        double pageGap{2.0};
        bool keyboardOnly{true};
        ColourSource borderColour{QPalette::Highlight};

        bool operator==(const Focus& other) const = default;
    };

    struct CloseState
    {
        BrushSource backgroundColour{QPalette::WindowText};
        int backgroundOpacity{0};
        ColourSource foregroundColour{QPalette::WindowText};

        bool operator==(const CloseState& other) const = default;
    };

    struct Close
    {
        int extent{16};
        double inset{1.0};
        double radius{4.0};
        double glyphSize{6.4};
        double glyphWidth{1.5};
        CloseState normal;
        CloseState hovered{.backgroundOpacity = 10};
        CloseState pressed{.backgroundOpacity = 18};
        CloseState disabled;

        bool operator==(const Close& other) const = default;
    };

    struct Pane
    {
        enum class ReliefStyle : uint8_t
        {
            Classic = 0,
            Raised,
            Sunken,
        };

        double borderWidth{0.0};
        double radius{4.0};
        ColourSource borderColour{QPalette::Mid};
        int borderOpacity{100};
        BrushSource backgroundColour{QPalette::Window};
        int backgroundOpacity{0};
        int reliefOpacity{0};
        ReliefStyle reliefStyle{ReliefStyle::Raised};

        bool operator==(const Pane& other) const = default;
    };

    struct Strip
    {
        BrushSource backgroundColour{QPalette::WindowText};
        int backgroundOpacity{0};
        double radius{0.0};
        int leadingPadding{0};
        int trailingPadding{0};
        int outerPadding{0};

        bool operator==(const Strip& other) const = default;
    };

    Design design{Design::Flat};
    Alignment alignment{Alignment::Leading};
    int horizontalPadding{12};
    int verticalPadding{6};
    int minimumThickness{0};
    double inset{1.0};
    double radius{4.0};
    Surface normal;
    Surface hovered{.backgroundOpacity = 8};
    Surface pressed{.backgroundOpacity = 20};
    Surface selected{.backgroundOpacity = 12};
    Surface selectedHovered{.backgroundOpacity = 16};
    Surface disabled;
    Surface selectedDisabled{.backgroundOpacity = 12};
    Accent accent;
    Focus focus;
    Close close;
    Pane pane;
    Strip strip;

    bool operator==(const TabAppearance& other) const = default;
};

std::expected<TabAppearance, QString> resolveTabAppearance(const QJsonObject& appearance);
} // namespace Fooyin
