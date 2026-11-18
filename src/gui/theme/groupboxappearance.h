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

#include <QFont>
#include <QJsonObject>

#include <expected>

namespace Fooyin {
struct GroupBoxAppearance
{
    struct Title
    {
        QString placement{QStringLiteral("inside")};
        QString alignment{QStringLiteral("center")};
        int horizontalMargin{8};
        int inset{4};
        int gap{4};
        QFont font;

        bool operator==(const Title& other) const
        {
            return placement == other.placement && alignment == other.alignment
                && horizontalMargin == other.horizontalMargin && inset == other.inset && gap == other.gap
                && font == other.font && font.resolveMask() == other.font.resolveMask();
        }
    };

    struct Colours
    {
        bool backgroundVisible{true};
        ColourSource backgroundColour{QPalette::Window};
        ColourSource backgroundTintColour{QPalette::WindowText};
        double backgroundTintOpacity{2.5};
        ColourSource borderColour{QPalette::Mid};
        ColourSource borderTintColour{QPalette::WindowText};
        double borderTintOpacity{22.0};
        ColourSource titleColour{QPalette::WindowText};

        bool operator==(const Colours& other) const = default;
    };

    int horizontalPadding{3};
    int verticalPadding{3};
    double radius{4.0};
    double borderWidth{1.0};
    int reliefOpacity{0};
    Title title;
    Colours normal;
    Colours disabled;

    bool operator==(const GroupBoxAppearance& other) const = default;
};

std::expected<GroupBoxAppearance, QString> resolveGroupBoxAppearance(const QJsonObject& appearance);
} // namespace Fooyin
