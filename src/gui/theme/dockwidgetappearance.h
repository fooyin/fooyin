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
struct DockWidgetAppearance
{
    struct Border
    {
        int width{1};
        ColourSource colour{QPalette::Mid};
        ColourSource tintColour{QPalette::WindowText};
        int tintOpacity{22};
        int opacity{60};

        bool operator==(const Border& other) const = default;
    };

    struct Title
    {
        BrushSource backgroundColour{QPalette::Button};
        int backgroundOpacity{100};
        ColourSource foregroundColour{QPalette::WindowText};
        ColourSource iconColour{QPalette::WindowText};
        Border border;

        bool operator==(const Title& other) const = default;
    };

    int titleMargin{4};
    int buttonMargin{2};
    int separatorExtent{6};
    Title title;
    Border frame;

    bool operator==(const DockWidgetAppearance& other) const = default;
};

std::expected<DockWidgetAppearance, QString> resolveDockWidgetAppearance(const QJsonObject& appearance);
} // namespace Fooyin
