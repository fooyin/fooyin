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
#include <QPalette>

#include <expected>

namespace Fooyin {
struct ScrollBarAppearance
{
    struct TrackState
    {
        bool visible{true};
        ColourSource backgroundColour{QPalette::Window};

        bool operator==(const TrackState& other) const = default;
    };

    struct Track
    {
        TrackState normal;
        TrackState hovered;
        TrackState pressed;
        TrackState disabled;

        bool operator==(const Track& other) const = default;
    };

    struct Thumb
    {
        double sideInset{3.0};
        double endInset{2.0};
        int cornerRadius{100};
        int opacity{45};
        double borderWidth{0.0};
        ColourSource backgroundColour{QPalette::WindowText};
        ColourSource borderColour{QPalette::Highlight};

        bool operator==(const Thumb& other) const = default;
    };

    struct Focus
    {
        double borderWidth{1.0};
        double radius{4.0};
        ColourSource borderColour{QPalette::Highlight};

        bool operator==(const Focus& other) const = default;
    };

    struct AutoHide
    {
        bool enabled{false};
        int hideDelay{1200};
        int fadeDuration{200};

        bool operator==(const AutoHide& other) const = default;
    };

    int width{14};
    int minimumThumbLength{28};
    Track track;
    Thumb normal;
    Thumb hovered{.opacity = 70};
    Thumb pressed{.opacity = 100, .backgroundColour = QPalette::Highlight};
    Thumb disabled;
    Focus focus;
    AutoHide autoHide;

    bool operator==(const ScrollBarAppearance& other) const = default;
};

std::expected<ScrollBarAppearance, QString> resolveScrollBarAppearance(const QJsonObject& appearance);
} // namespace Fooyin
