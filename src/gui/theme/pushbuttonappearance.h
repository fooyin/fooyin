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
struct PushButtonAppearance
{
    using Surface = ButtonSurface;
    using Focus   = ButtonFocus;

    int minimumWidth{80};
    int minimumHeight{0};
    int horizontalPadding{12};
    int verticalPadding{7};
    double radius{4.0};
    Surface normal;
    Surface hovered{.borderColour = QPalette::Highlight, .tintOpacity = 10};
    Surface pressed{.tintOpacity = 22};
    Surface disabled;
    Surface checked{.borderColour = QPalette::Highlight, .tintOpacity = 16};
    Surface defaultButton{.borderWidth = 1.5, .borderColour = QPalette::Highlight, .tintOpacity = 6};
    Focus focus;

    bool operator==(const PushButtonAppearance& other) const = default;
};

std::expected<PushButtonAppearance, QString> resolvePushButtonAppearance(const QJsonObject& appearance);
} // namespace Fooyin
