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

#include "inputappearance.h"

#include "appearanceutils.h"

#include <QCoreApplication>

#include <array>
#include <tuple>

using namespace Qt::StringLiterals;

namespace Fooyin {
using namespace Appearance;
namespace {
std::expected<InputAppearance::Surface, QString> readSurface(const QJsonValue& value, const QString& path,
                                                             const InputAppearance::Surface& defaults, bool combo)
{
    auto resolved{defaults};
    const auto object
        = combo ? readObject(value, path,
                             {"backgroundVisible"_L1, "borderWidth"_L1, "backgroundColour"_L1,
                              "editableBackgroundColour"_L1, "borderColour"_L1})
                : readObject(value, path,
                             {"backgroundVisible"_L1, "borderWidth"_L1, "backgroundColour"_L1, "borderColour"_L1});
    if(!object) {
        return std::unexpected{object.error()};
    }

    const auto visible = readBool(*object, "backgroundVisible"_L1, path, resolved.backgroundVisible);
    if(!visible) {
        return std::unexpected{visible.error()};
    }
    resolved.backgroundVisible = *visible;

    const auto borderWidth = readNumber(*object, "borderWidth"_L1, path, resolved.borderWidth, 0, 16);
    if(!borderWidth) {
        return std::unexpected{borderWidth.error()};
    }
    resolved.borderWidth = *borderWidth;

    for(const auto& [name, target] : std::array{std::pair{"backgroundColour"_L1, &resolved.backgroundColour},
                                                std::pair{"borderColour"_L1, &resolved.borderColour}}) {
        const auto field = readColourSource(object->value(name), path + u'.' + name, *target);
        if(!field) {
            return std::unexpected{field.error()};
        }
        *target = *field;
    }

    if(combo) {
        const auto editableBackgroundColour
            = readColourSource(object->value("editableBackgroundColour"_L1), path + u".editableBackgroundColour"_s,
                               resolved.editableBackgroundColour);
        if(!editableBackgroundColour) {
            return std::unexpected{editableBackgroundColour.error()};
        }
        resolved.editableBackgroundColour = *editableBackgroundColour;
    }

    return resolved;
}

std::expected<InputAppearance::ActionState, QString>
readActionState(const QJsonValue& value, const QString& path, const InputAppearance::ActionState& defaults, bool combo)
{
    auto resolved{defaults};
    const auto object = combo ? readObject(value, path,
                                           {"backgroundColour"_L1, "backgroundOpacity"_L1, "borderWidth"_L1,
                                            "borderColour"_L1, "foregroundColour"_L1, "editableForegroundColour"_L1})
                              : readObject(value, path,
                                           {"backgroundColour"_L1, "backgroundOpacity"_L1, "borderWidth"_L1,
                                            "borderColour"_L1, "foregroundColour"_L1});
    if(!object) {
        return std::unexpected{object.error()};
    }

    const auto backgroundOpacity
        = readNumber(*object, "backgroundOpacity"_L1, path, resolved.backgroundOpacity, 0, 100);
    if(!backgroundOpacity) {
        return std::unexpected{backgroundOpacity.error()};
    }
    resolved.backgroundOpacity = *backgroundOpacity;

    const auto borderWidth = readNumber(*object, "borderWidth"_L1, path, resolved.borderWidth, 0, 16);
    if(!borderWidth) {
        return std::unexpected{borderWidth.error()};
    }
    resolved.borderWidth = *borderWidth;

    for(const auto& [name, target] : std::array{std::pair{"backgroundColour"_L1, &resolved.backgroundColour},
                                                std::pair{"borderColour"_L1, &resolved.borderColour},
                                                std::pair{"foregroundColour"_L1, &resolved.foregroundColour}}) {
        const auto field = readColourSource(object->value(name), path + u'.' + name, *target);
        if(!field) {
            return std::unexpected{field.error()};
        }
        *target = *field;
    }

    if(combo) {
        const auto editableForegroundColour
            = readColourSource(object->value("editableForegroundColour"_L1), path + u".editableForegroundColour"_s,
                               resolved.editableForegroundColour);
        if(!editableForegroundColour) {
            return std::unexpected{editableForegroundColour.error()};
        }
        resolved.editableForegroundColour = *editableForegroundColour;
    }

    return resolved;
}

std::expected<InputAppearance::Action, QString> readAction(const QJsonValue& value, const QString& path,
                                                           const InputAppearance::Action& defaults, InputType type)
{
    auto resolved{defaults};
    const auto object = type == InputType::SpinBox
                          ? readObject(value, path,
                                       {"layout"_L1, "filledArrows"_L1, "width"_L1, "radius"_L1, "inset"_L1,
                                        "glyphSize"_L1, "glyphWidth"_L1})
                          : readObject(value, path,
                                       {"indicatorDesign"_L1, "filledArrows"_L1, "width"_L1, "radius"_L1, "inset"_L1,
                                        "glyphSize"_L1, "glyphWidth"_L1});
    if(!object) {
        return std::unexpected{object.error()};
    }

    const auto indicator = object->value("indicatorDesign"_L1);
    if(!indicator.isUndefined()) {
        const auto name = indicator.toString();
        if(!indicator.isString() || (name != "single"_L1 && name != "paired"_L1)) {
            return std::unexpected{errorWithPath(
                QCoreApplication::translate("ThemeAppearance", "Choose a single or paired arrow indicator."),
                path + u".indicatorDesign"_s)};
        }
        resolved.indicatorDesign = name == "paired"_L1 ? InputAppearance::Action::IndicatorDesign::PairedArrows
                                                       : InputAppearance::Action::IndicatorDesign::SingleArrow;
    }

    const auto layout = object->value("layout"_L1);
    if(!layout.isUndefined()) {
        const auto name = layout.toString();
        if(!layout.isString() || (name != "stacked"_L1 && name != "sideBySide"_L1)) {
            return std::unexpected{errorWithPath(
                QCoreApplication::translate("ThemeAppearance", "Choose stacked or side-by-side step buttons."),
                path + u".layout"_s)};
        }
        resolved.layout = name == "sideBySide"_L1 ? InputAppearance::Action::Layout::SideBySide
                                                  : InputAppearance::Action::Layout::Stacked;
    }

    const auto filledArrows = readBool(*object, "filledArrows"_L1, path, resolved.filledArrows);
    if(!filledArrows) {
        return std::unexpected{filledArrows.error()};
    }
    resolved.filledArrows = *filledArrows;

    const auto width = readNumber(*object, "width"_L1, path, resolved.width, 1, 128);
    if(!width) {
        return std::unexpected{width.error()};
    }
    resolved.width = *width;

    for(const auto& [name, target, minimum, maximum] :
        std::array{std::tuple{"radius"_L1, &resolved.radius, 0, 128}, std::tuple{"inset"_L1, &resolved.inset, 0, 64},
                   std::tuple{"glyphSize"_L1, &resolved.glyphSize, 0, 64},
                   std::tuple{"glyphWidth"_L1, &resolved.glyphWidth, 0, 16}}) {
        const auto field = readNumber(*object, name, path, *target, minimum, maximum);
        if(!field) {
            return std::unexpected{field.error()};
        }
        *target = *field;
    }

    return resolved;
}
} // namespace

QString inputAppearanceKey(InputType type)
{
    switch(type) {
        case InputType::LineEdit:
            return u"lineEdit"_s;
        case InputType::ComboBox:
            return u"comboBox"_s;
        case InputType::SpinBox:
            return u"spinBox"_s;
    }
    return {};
}

InputAppearance defaultInputAppearance(InputType type)
{
    InputAppearance appearance;
    if(type == InputType::ComboBox) {
        for(auto* state : {&appearance.normal, &appearance.hovered, &appearance.focused, &appearance.readOnly,
                           &appearance.disabled}) {
            state->backgroundColour = QPalette::Button;
        }
        for(auto* state : {&appearance.actionNormal, &appearance.actionHovered, &appearance.actionPressed,
                           &appearance.actionDisabled}) {
            state->foregroundColour = QPalette::ButtonText;
        }
    }
    return appearance;
}

std::expected<InputAppearance, QString> resolveInputAppearance(const QJsonObject& appearance, InputType type)
{
    auto resolved      = defaultInputAppearance(type);
    const auto key     = inputAppearanceKey(type);
    const QString path = u"appearance."_s + key;
    const auto object
        = type == InputType::LineEdit
            ? readObject(appearance.value(key), path,
                         {"minimumHeight"_L1, "horizontalPadding"_L1, "verticalPadding"_L1, "radius"_L1, "normal"_L1,
                          "hovered"_L1, "focused"_L1, "readOnly"_L1, "disabled"_L1, "focus"_L1})
            : readObject(appearance.value(key), path,
                         {"minimumHeight"_L1, "horizontalPadding"_L1, "verticalPadding"_L1, "radius"_L1, "normal"_L1,
                          "hovered"_L1, "focused"_L1, "readOnly"_L1, "disabled"_L1, "focus"_L1, "action"_L1,
                          "actionNormal"_L1, "actionHovered"_L1, "actionPressed"_L1, "actionDisabled"_L1});
    if(!object) {
        return std::unexpected{object.error()};
    }

    for(const auto& [name, target, minimum, maximum] :
        std::array{std::tuple{"minimumHeight"_L1, &resolved.minimumHeight, 0, 512},
                   std::tuple{"horizontalPadding"_L1, &resolved.horizontalPadding, 0, 128},
                   std::tuple{"verticalPadding"_L1, &resolved.verticalPadding, 0, 128}}) {
        const auto field = readNumber(*object, name, path, *target, minimum, maximum);
        if(!field) {
            return std::unexpected{field.error()};
        }
        *target = *field;
    }

    const auto radius = readNumber(*object, "radius"_L1, path, resolved.radius, 0, 128);
    if(!radius) {
        return std::unexpected{radius.error()};
    }
    resolved.radius = *radius;

    const bool combo = type == InputType::ComboBox;
    const std::array surfaces{std::pair{"normal"_L1, &resolved.normal}, std::pair{"hovered"_L1, &resolved.hovered},
                              std::pair{"focused"_L1, &resolved.focused}, std::pair{"readOnly"_L1, &resolved.readOnly},
                              std::pair{"disabled"_L1, &resolved.disabled}};
    for(const auto& [name, surface] : surfaces) {
        const auto state = readSurface(stateOverrides(*object, name), path + u'.' + name, *surface, combo);
        if(!state) {
            return std::unexpected{state.error()};
        }
        *surface = *state;
    }

    const auto focus = readButtonFocus(object->value("focus"_L1), path + u".focus"_s, resolved.focus);
    if(!focus) {
        return std::unexpected{focus.error()};
    }
    resolved.focus = *focus;

    if(type != InputType::LineEdit) {
        const auto action = readAction(object->value("action"_L1), path + u".action"_s, resolved.action, type);
        if(!action) {
            return std::unexpected{action.error()};
        }
        resolved.action = *action;

        const std::array states{std::pair{"actionNormal"_L1, &resolved.actionNormal},
                                std::pair{"actionHovered"_L1, &resolved.actionHovered},
                                std::pair{"actionPressed"_L1, &resolved.actionPressed},
                                std::pair{"actionDisabled"_L1, &resolved.actionDisabled}};
        for(const auto& [name, surface] : states) {
            const auto state = readActionState(stateOverrides(*object, name, "actionNormal"_L1), path + u'.' + name,
                                               *surface, combo);
            if(!state) {
                return std::unexpected{state.error()};
            }
            *surface = *state;
        }
    }
    return resolved;
}
} // namespace Fooyin
