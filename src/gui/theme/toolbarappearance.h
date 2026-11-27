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
struct ToolBarAppearance
{
    struct Border
    {
        int width{1};
        ColourSource colour{QPalette::Mid};
        ColourSource tintColour{QPalette::WindowText};
        int tintOpacity{22};
        int opacity{60};
        int floatingOpacity{100};
        double floatingRadius{4.0};

        bool operator==(const Border& other) const = default;
    };

    struct Handle
    {
        int extent{10};
        double dotRadius{0.75};
        double columnSpacing{3.0};
        double rowSpacing{4.0};
        ColourSource colour{QPalette::WindowText};
        int opacity{45};

        bool operator==(const Handle& other) const = default;
    };

    struct Separator
    {
        int extent{8};
        double length{16.0};
        double margin{4.0};
        double width{1.0};
        ColourSource colour{QPalette::Mid};
        ColourSource tintColour{QPalette::WindowText};
        int tintOpacity{22};
        int opacity{60};

        bool operator==(const Separator& other) const = default;
    };

    struct Extension
    {
        int extent{22};
        double glyphSize{12.0};
        double glyphWidth{1.5};
        ColourSource colour{QPalette::ButtonText};

        bool operator==(const Extension& other) const = default;
    };

    int itemMargin{2};
    int itemSpacing{2};
    BrushSource backgroundColour{QPalette::Window};
    int backgroundOpacity{100};
    Border border;
    Handle handle;
    Separator separator;
    Extension extension;

    bool operator==(const ToolBarAppearance& other) const = default;
};

std::expected<ToolBarAppearance, QString> resolveToolBarAppearance(const QJsonObject& appearance);
} // namespace Fooyin
