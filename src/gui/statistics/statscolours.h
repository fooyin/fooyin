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

#include <QColor>
#include <QPalette>

#include <algorithm>
#include <array>

namespace Fooyin::Stats {
//! Colours used to draw statistics, derived from the current theme so the charts follow light and dark themes.
struct Colours
{
    //! Surface the chart is drawn on.
    QColor card;
    QColor text;
    QColor textMuted;
    //! Gridlines, axes and the unfilled part of meters.
    QColor grid;
    //! Colour of single series marks.
    QColor accent;
    //! Distinct hues for categories, in a fixed order.
    std::array<QColor, 8> series;
    //! Remainder category, deliberately neutral.
    QColor other;

    static Colours fromPalette(const QPalette& palette)
    {
        Colours colours;
        const bool dark = palette.color(QPalette::Window).lightness() < 128;

        colours.card      = palette.color(QPalette::Base);
        colours.text      = palette.color(QPalette::Text);
        colours.textMuted = mix(colours.text, colours.card, 0.65);
        colours.grid      = mix(colours.text, colours.card, 0.12);
        colours.accent    = palette.color(QPalette::Highlight);
        colours.series    = dark ? darkSeries() : lightSeries();
        colours.other     = mix(colours.text, colours.card, 0.45);
        return colours;
    }

    //! Colour for a value between 0 and 1 on a single hue ramp that runs from the surface to the accent.
    [[nodiscard]] QColor ramp(double fraction) const
    {
        if(fraction <= 0.0) {
            return grid;
        }
        return mix(accent, card, 0.25 + 0.75 * std::min(fraction, 1.0));
    }

    [[nodiscard]] QColor seriesColour(size_t index) const
    {
        return series.at(index % series.size());
    }

    //! Returns @p first blended with @p second, @p weight being the proportion of @p first.
    static QColor mix(const QColor& first, const QColor& second, double weight)
    {
        return QColor::fromRgbF(first.redF() * weight + second.redF() * (1.0 - weight),
                                first.greenF() * weight + second.greenF() * (1.0 - weight),
                                first.blueF() * weight + second.blueF() * (1.0 - weight));
    }

private:
    static std::array<QColor, 8> lightSeries()
    {
        return {QColor{0x2a, 0x78, 0xd6}, QColor{0xeb, 0x68, 0x34}, QColor{0x1b, 0xaf, 0x7a}, QColor{0xed, 0xa1, 0x00},
                QColor{0xe8, 0x7b, 0xa4}, QColor{0x00, 0x83, 0x00}, QColor{0x62, 0x50, 0xd6}, QColor{0xe3, 0x49, 0x48}};
    }

    static std::array<QColor, 8> darkSeries()
    {
        return {QColor{0x39, 0x87, 0xe5}, QColor{0xd9, 0x59, 0x26}, QColor{0x19, 0x9e, 0x70}, QColor{0xc9, 0x85, 0x00},
                QColor{0xd5, 0x51, 0x81}, QColor{0x00, 0x83, 0x00}, QColor{0x90, 0x85, 0xe9}, QColor{0xe6, 0x67, 0x67}};
    }
};
} // namespace Fooyin::Stats
