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

#include "itemviewappearance.h"

#include "appearanceutils.h"

#include <array>
#include <tuple>

using namespace Qt::StringLiterals;

namespace Fooyin {
using namespace Appearance;
namespace {
std::expected<ItemViewAppearance::State, QString> readState(const QJsonValue& value, const QString& path,
                                                            const ItemViewAppearance::State& defaults)
{
    auto resolved{defaults};
    const auto object = readObject(value, path,
                                   {"backgroundColour"_L1, "backgroundOpacity"_L1, "backgroundLightness"_L1,
                                    "borderWidth"_L1, "borderColour"_L1, "borderOpacity"_L1});
    if(!object) {
        return std::unexpected{object.error()};
    }

    const auto backgroundColour = readBrushSource(object->value("backgroundColour"_L1), path + u".backgroundColour"_s,
                                                  resolved.backgroundColour);
    if(!backgroundColour) {
        return std::unexpected{backgroundColour.error()};
    }
    resolved.backgroundColour = *backgroundColour;

    for(const auto& [name, target, minimum, maximum] :
        std::array{std::tuple{"backgroundOpacity"_L1, &resolved.backgroundOpacity, 0, 100},
                   std::tuple{"backgroundLightness"_L1, &resolved.backgroundLightness, 1, 300}}) {
        const auto field = readNumber(*object, name, path, *target, minimum, maximum);
        if(!field) {
            return std::unexpected{field.error()};
        }
        *target = *field;
    }

    const auto borderWidth = readNumber(*object, "borderWidth"_L1, path, resolved.borderWidth, 0, 32);
    if(!borderWidth) {
        return std::unexpected{borderWidth.error()};
    }
    resolved.borderWidth = *borderWidth;

    const auto borderColour
        = readColourSource(object->value("borderColour"_L1), path + u".borderColour"_s, resolved.borderColour);
    if(!borderColour) {
        return std::unexpected{borderColour.error()};
    }
    resolved.borderColour = *borderColour;

    const auto borderOpacity = readNumber(*object, "borderOpacity"_L1, path, resolved.borderOpacity, 0, 100);
    if(!borderOpacity) {
        return std::unexpected{borderOpacity.error()};
    }
    resolved.borderOpacity = *borderOpacity;

    return resolved;
}

std::expected<ItemViewAppearance::Focus, QString> readFocus(const QJsonValue& value, const QString& path,
                                                            const ItemViewAppearance::Focus& defaults)
{
    auto resolved{defaults};
    const auto object = readObject(value, path,
                                   {"width"_L1, "inset"_L1, "colour"_L1, "opacity"_L1, "selectedOpacity"_L1,
                                    "selectedLightness"_L1, "keyboardOnly"_L1});
    if(!object) {
        return std::unexpected{object.error()};
    }

    for(const auto& [name, target, minimum, maximum] :
        std::array{std::tuple{"width"_L1, &resolved.width, 0, 32}, std::tuple{"inset"_L1, &resolved.inset, 0, 128}}) {
        const auto field = readNumber(*object, name, path, *target, minimum, maximum);
        if(!field) {
            return std::unexpected{field.error()};
        }
        *target = *field;
    }

    const auto colour = readColourSource(object->value("colour"_L1), path + u".colour"_s, resolved.colour);
    if(!colour) {
        return std::unexpected{colour.error()};
    }
    resolved.colour = *colour;

    for(const auto& [name, target, minimum, maximum] :
        std::array{std::tuple{"opacity"_L1, &resolved.opacity, 0, 100},
                   std::tuple{"selectedOpacity"_L1, &resolved.selectedOpacity, 0, 100},
                   std::tuple{"selectedLightness"_L1, &resolved.selectedLightness, 1, 300}}) {
        const auto field = readNumber(*object, name, path, *target, minimum, maximum);
        if(!field) {
            return std::unexpected{field.error()};
        }
        *target = *field;
    }

    const auto keyboardOnly = readBool(*object, "keyboardOnly"_L1, path, resolved.keyboardOnly);
    if(!keyboardOnly) {
        return std::unexpected{keyboardOnly.error()};
    }
    resolved.keyboardOnly = *keyboardOnly;

    return resolved;
}

std::expected<ItemViewAppearance::Branch, QString> readBranch(const QJsonValue& value, const QString& path,
                                                              const ItemViewAppearance::Branch& defaults)
{
    auto resolved{defaults};
    const auto object
        = readObject(value, path,
                     {"lineWidth"_L1, "lineColour"_L1, "lineOpacity"_L1, "selectedLineColour"_L1,
                      "selectedLineOpacity"_L1, "arrowExtent"_L1, "filledArrows"_L1, "arrowSize"_L1, "arrowWidth"_L1,
                      "arrowColour"_L1, "hoveredArrowColour"_L1, "selectedArrowColour"_L1});
    if(!object) {
        return std::unexpected{object.error()};
    }

    const auto filledArrows = readBool(*object, "filledArrows"_L1, path, resolved.filledArrows);
    if(!filledArrows) {
        return std::unexpected{filledArrows.error()};
    }
    resolved.filledArrows = *filledArrows;

    const auto lineWidth = readNumber(*object, "lineWidth"_L1, path, resolved.lineWidth, 0, 32);
    if(!lineWidth) {
        return std::unexpected{lineWidth.error()};
    }
    resolved.lineWidth = *lineWidth;

    const auto lineColour
        = readColourSource(object->value("lineColour"_L1), path + u".lineColour"_s, resolved.lineColour);
    if(!lineColour) {
        return std::unexpected{lineColour.error()};
    }
    resolved.lineColour = *lineColour;

    const auto lineOpacity = readNumber(*object, "lineOpacity"_L1, path, resolved.lineOpacity, 0, 100);
    if(!lineOpacity) {
        return std::unexpected{lineOpacity.error()};
    }
    resolved.lineOpacity = *lineOpacity;

    const auto selectedLineColour = readColourSource(object->value("selectedLineColour"_L1),
                                                     path + u".selectedLineColour"_s, resolved.selectedLineColour);
    if(!selectedLineColour) {
        return std::unexpected{selectedLineColour.error()};
    }
    resolved.selectedLineColour = *selectedLineColour;

    for(const auto& [name, target, minimum, maximum] :
        std::array{std::tuple{"selectedLineOpacity"_L1, &resolved.selectedLineOpacity, 0, 100},
                   std::tuple{"arrowExtent"_L1, &resolved.arrowExtent, 0, 128}}) {
        const auto field = readNumber(*object, name, path, *target, minimum, maximum);
        if(!field) {
            return std::unexpected{field.error()};
        }
        *target = *field;
    }

    for(const auto& [name, target, minimum, maximum] :
        std::array{std::tuple{"arrowSize"_L1, &resolved.arrowSize, 0, 128},
                   std::tuple{"arrowWidth"_L1, &resolved.arrowWidth, 0, 32}}) {
        const auto field = readNumber(*object, name, path, *target, minimum, maximum);
        if(!field) {
            return std::unexpected{field.error()};
        }
        *target = *field;
    }

    for(const auto& [name, target] : std::array{std::pair{"arrowColour"_L1, &resolved.arrowColour},
                                                std::pair{"hoveredArrowColour"_L1, &resolved.hoveredArrowColour},
                                                std::pair{"selectedArrowColour"_L1, &resolved.selectedArrowColour}}) {
        const auto field = readColourSource(object->value(name), path + u'.' + name, *target);
        if(!field) {
            return std::unexpected{field.error()};
        }
        *target = *field;
    }

    return resolved;
}

} // namespace

std::expected<ItemViewAppearance, QString> resolveItemViewAppearance(const QJsonObject& appearance)
{
    ItemViewAppearance resolved;
    const QString path = u"appearance.itemView"_s;
    const auto object
        = readObject(appearance.value("itemView"_L1), path,
                     {"verticalPadding"_L1, "textPadding"_L1, "treeTextPadding"_L1, "horizontalInset"_L1,
                      "verticalInset"_L1, "radius"_L1, "customBorderWidth"_L1, "normal"_L1, "alternate"_L1,
                      "hovered"_L1, "selected"_L1, "selectedHovered"_L1, "focus"_L1, "branch"_L1});
    if(!object) {
        return std::unexpected{object.error()};
    }

    for(const auto& [name, target] : std::array{std::pair{"verticalPadding"_L1, &resolved.verticalPadding},
                                                std::pair{"textPadding"_L1, &resolved.textPadding},
                                                std::pair{"treeTextPadding"_L1, &resolved.treeTextPadding}}) {
        const auto field = readNumber(*object, name, path, *target, 0, 128);
        if(!field) {
            return std::unexpected{field.error()};
        }
        *target = *field;
    }

    for(const auto& [name, target, minimum, maximum] :
        std::array{std::tuple{"horizontalInset"_L1, &resolved.horizontalInset, 0, 128},
                   std::tuple{"verticalInset"_L1, &resolved.verticalInset, 0, 128},
                   std::tuple{"radius"_L1, &resolved.radius, 0, 128},
                   std::tuple{"customBorderWidth"_L1, &resolved.customBorderWidth, 0, 32}}) {
        const auto field = readNumber(*object, name, path, *target, minimum, maximum);
        if(!field) {
            return std::unexpected{field.error()};
        }
        *target = *field;
    }

    for(const auto& [name, target] :
        std::array{std::pair{"normal"_L1, &resolved.normal}, std::pair{"alternate"_L1, &resolved.alternate},
                   std::pair{"hovered"_L1, &resolved.hovered}, std::pair{"selected"_L1, &resolved.selected}}) {
        const auto state = readState(stateOverrides(*object, name), path + u'.' + name, *target);
        if(!state) {
            return std::unexpected{state.error()};
        }
        *target = *state;
    }

    auto states = *object;
    states.insert("selected"_L1, stateOverrides(*object, "selected"_L1));
    const auto selectedHovered = readState(stateOverrides(states, "selectedHovered"_L1, "selected"_L1),
                                           path + u".selectedHovered"_s, resolved.selectedHovered);
    if(!selectedHovered) {
        return std::unexpected{selectedHovered.error()};
    }
    resolved.selectedHovered = *selectedHovered;

    const auto focus = readFocus(object->value("focus"_L1), path + u".focus"_s, resolved.focus);
    if(!focus) {
        return std::unexpected{focus.error()};
    }
    resolved.focus = *focus;

    const auto branch = readBranch(object->value("branch"_L1), path + u".branch"_s, resolved.branch);
    if(!branch) {
        return std::unexpected{branch.error()};
    }
    resolved.branch = *branch;

    return resolved;
}
} // namespace Fooyin
