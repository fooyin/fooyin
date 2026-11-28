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
enum class InputType : uint8_t
{
    LineEdit = 0,
    ComboBox,
    SpinBox,
};

struct InputAppearance
{
    struct Surface
    {
        bool backgroundVisible{true};
        double borderWidth{1.0};
        ColourSource backgroundColour{QPalette::Base};
        ColourSource editableBackgroundColour{QPalette::Base};
        ColourSource borderColour{QPalette::Mid};

        bool operator==(const Surface& other) const = default;
    };

    struct Action
    {
        enum class Layout : uint8_t
        {
            Stacked = 0,
            SideBySide,
        };

        enum class IndicatorDesign : uint8_t
        {
            SingleArrow = 0,
            PairedArrows,
        };

        Layout layout{Layout::Stacked};
        IndicatorDesign indicatorDesign{IndicatorDesign::SingleArrow};
        bool filledArrows{false};
        int width{24};
        double radius{3.0};
        double inset{1.0};
        double glyphSize{6.0};
        double glyphWidth{1.5};

        bool operator==(const Action& other) const = default;
    };

    struct ActionState
    {
        ColourSource backgroundColour{QPalette::Highlight};
        int backgroundOpacity{0};
        double borderWidth{0.0};
        ColourSource borderColour{QPalette::Mid};
        ColourSource foregroundColour{QPalette::Text};
        ColourSource editableForegroundColour{QPalette::Text};

        bool operator==(const ActionState& other) const = default;
    };

    int minimumHeight{24};
    int horizontalPadding{6};
    int verticalPadding{4};
    double radius{4.0};
    Surface normal;
    Surface hovered{.borderColour = QPalette::Highlight};
    Surface focused{.borderColour = QPalette::Highlight};
    Surface readOnly;
    Surface disabled;
    ButtonFocus focus{.spacing = 0.0};
    Action action;
    ActionState actionNormal;
    ActionState actionHovered{.backgroundOpacity = 10};
    ActionState actionPressed{.backgroundOpacity = 22};
    ActionState actionDisabled;

    bool operator==(const InputAppearance& other) const = default;
};

QString inputAppearanceKey(InputType type);
InputAppearance defaultInputAppearance(InputType type);
std::expected<InputAppearance, QString> resolveInputAppearance(const QJsonObject& appearance, InputType type);
} // namespace Fooyin
