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

#include <expected>

namespace Fooyin {
struct SplitterAppearance
{
    struct Grip
    {
        double length{24.0};
        double thickness{2.0};
        double margin{4.0};
        int cornerRadius{100};

        bool operator==(const Grip& other) const = default;
    };

    struct State
    {
        ColourSource backgroundColour{QPalette::Highlight};
        int backgroundOpacity{0};
        ColourSource gripColour{QPalette::WindowText};
        int gripOpacity{40};

        bool operator==(const State& other) const = default;
    };

    int width{6};
    double backgroundRadius{2.0};
    Grip grip;
    State normal;
    State hovered{.backgroundOpacity = 10, .gripColour = QPalette::Highlight, .gripOpacity = 100};
    State pressed{.backgroundOpacity = 18, .gripColour = QPalette::Highlight, .gripOpacity = 100};
    State disabled;

    bool operator==(const SplitterAppearance& other) const = default;
};

std::expected<SplitterAppearance, QString> resolveSplitterAppearance(const QJsonObject& appearance);
} // namespace Fooyin
