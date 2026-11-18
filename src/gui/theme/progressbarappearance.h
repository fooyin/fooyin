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
struct ProgressBarAppearance
{
    struct Track
    {
        double inset{2.5};
        double radius{4.0};
        double borderWidth{1.0};
        int compactThickness{6};
        double compactRadius{3.0};
        double compactBorderWidth{0.0};

        bool operator==(const Track& other) const = default;
    };

    struct Fill
    {
        double inset{0.0};
        double radius{4.0};
        double compactRadius{3.0};
        bool segmented{false};
        int segmentLength{8};
        int segmentSpacing{2};

        bool operator==(const Fill& other) const = default;
    };

    struct Colours
    {
        BrushSource trackColour{QPalette::Base};
        BrushSource compactTrackColour{QPalette::Mid};
        ColourSource borderColour{QPalette::Mid};
        BrushSource fillColour{QPalette::Highlight};
        ColourSource textColour{QPalette::Text};
        ColourSource filledTextColour{QPalette::HighlightedText};

        bool operator==(const Colours& other) const = default;
    };

    struct Text
    {
        enum class Alignment : uint8_t
        {
            Widget = 0,
            Leading,
            Centre,
            Trailing,
        };

        enum class Position : uint8_t
        {
            Inside = 0,
            Left,
            Right,
        };

        bool visible{true};
        int padding{6};
        Alignment alignment{Alignment::Widget};
        Position position{Position::Inside};
        int spacing{6};

        bool operator==(const Text& other) const = default;
    };

    struct Busy
    {
        bool animated{true};
        int duration{1600};
        int segmentSize{30};

        bool operator==(const Busy& other) const = default;
    };

    int minimumThickness{0};
    Track track;
    Fill fill;
    Colours normal;
    Colours disabled;
    Text text;
    Busy busy;

    bool operator==(const ProgressBarAppearance& other) const = default;
};

std::expected<ProgressBarAppearance, QString> resolveProgressBarAppearance(const QJsonObject& appearance);
} // namespace Fooyin
