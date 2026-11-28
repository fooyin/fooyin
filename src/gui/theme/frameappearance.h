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
struct FrameAppearance
{
    struct Separator
    {
        double width{1.0};
        double inset{0.0};
        ColourSource colour{QPalette::Mid};
        int opacity{100};

        bool operator==(const Separator& other) const = default;
    };

    double borderWidth{1.0};
    double radius{4.0};
    ColourSource borderColour{QPalette::Mid};
    int borderOpacity{100};
    Separator separator;

    bool operator==(const FrameAppearance& other) const = default;
};

std::expected<FrameAppearance, QString> resolveFrameAppearance(const QJsonObject& appearance);
} // namespace Fooyin
