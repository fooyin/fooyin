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

#include "checkappearance.h"

#include "appearanceutils.h"

#include <QCoreApplication>

#include <array>
#include <tuple>

using namespace Qt::StringLiterals;

namespace Fooyin {
using namespace Appearance;
namespace {
std::expected<CheckAppearance::State, QString> readState(const QJsonValue& value, const QString& path,
                                                         const CheckAppearance::State& defaults)
{
    auto resolved{defaults};
    const auto object = readObject(value, path,
                                   {"borderWidth"_L1, "backgroundColour"_L1, "checkedBackgroundColour"_L1,
                                    "borderColour"_L1, "checkedBorderColour"_L1, "markColour"_L1, "tintColour"_L1,
                                    "checkedTintColour"_L1, "tintOpacity"_L1, "haloColour"_L1, "haloOpacity"_L1});
    if(!object) {
        return std::unexpected{object.error()};
    }

    const auto borderWidth = readNumber(*object, "borderWidth"_L1, path, resolved.borderWidth, 0, 32);
    if(!borderWidth) {
        return std::unexpected{borderWidth.error()};
    }
    resolved.borderWidth = *borderWidth;

    const std::array colours{std::pair{"backgroundColour"_L1, &resolved.backgroundColour},
                             std::pair{"checkedBackgroundColour"_L1, &resolved.checkedBackgroundColour},
                             std::pair{"borderColour"_L1, &resolved.borderColour},
                             std::pair{"checkedBorderColour"_L1, &resolved.checkedBorderColour},
                             std::pair{"markColour"_L1, &resolved.markColour},
                             std::pair{"tintColour"_L1, &resolved.tintColour},
                             std::pair{"checkedTintColour"_L1, &resolved.checkedTintColour},
                             std::pair{"haloColour"_L1, &resolved.haloColour}};
    for(const auto& [name, colour] : colours) {
        const auto source = readColourSource(object->value(name), path + u'.' + name, *colour);
        if(!source) {
            return std::unexpected{source.error()};
        }
        *colour = *source;
    }

    for(const auto& [name, target] : std::array{std::pair{"tintOpacity"_L1, &resolved.tintOpacity},
                                                std::pair{"haloOpacity"_L1, &resolved.haloOpacity}}) {
        const auto field = readNumber(*object, name, path, *target, 0, 100);
        if(!field) {
            return std::unexpected{field.error()};
        }
        *target = *field;
    }

    return resolved;
}

std::expected<CheckAppearance, QString> readAppearance(const QJsonObject& appearance, bool radio)
{
    auto resolved      = radio ? defaultRadioButtonAppearance() : CheckAppearance{};
    const auto key     = radio ? "radioButton"_L1 : "checkBox"_L1;
    const QString path = u"appearance."_s + key;
    const auto object
        = radio ? readObject(appearance.value(key), path,
                             {"indicatorExtent"_L1, "labelSpacing"_L1, "inset"_L1, "markSize"_L1, "normal"_L1,
                              "hovered"_L1, "pressed"_L1, "disabled"_L1, "focus"_L1})
                : readObject(appearance.value(key), path,
                             {"indicatorExtent"_L1, "labelSpacing"_L1, "inset"_L1, "radius"_L1, "markSize"_L1,
                              "markWidth"_L1, "normal"_L1, "hovered"_L1, "pressed"_L1, "disabled"_L1, "focus"_L1});
    if(!object) {
        return std::unexpected{object.error()};
    }

    for(const auto& [name, target, minimum, maximum] :
        std::array{std::tuple{"indicatorExtent"_L1, &resolved.indicatorExtent, 1, 256},
                   std::tuple{"labelSpacing"_L1, &resolved.labelSpacing, 0, 128}}) {
        const auto field = readNumber(*object, name, path, *target, minimum, maximum);
        if(!field) {
            return std::unexpected{field.error()};
        }
        *target = *field;
    }

    for(const auto& [name, target] :
        std::array{std::pair{"inset"_L1, &resolved.inset}, std::pair{"markSize"_L1, &resolved.markSize}}) {
        const auto field = readNumber(*object, name, path, *target, 0, 128);
        if(!field) {
            return std::unexpected{field.error()};
        }
        *target = *field;
    }

    if(!radio) {
        for(const auto& [name, target, minimum, maximum] :
            std::array{std::tuple{"radius"_L1, &resolved.radius, 0, 128},
                       std::tuple{"markWidth"_L1, &resolved.markWidth, 0, 32}}) {
            const auto field = readNumber(*object, name, path, *target, minimum, maximum);
            if(!field) {
                return std::unexpected{field.error()};
            }
            *target = *field;
        }
    }

    const std::array states{std::pair{"normal"_L1, &resolved.normal}, std::pair{"hovered"_L1, &resolved.hovered},
                            std::pair{"pressed"_L1, &resolved.pressed}, std::pair{"disabled"_L1, &resolved.disabled}};
    for(const auto& [name, state] : states) {
        const auto value = readState(stateOverrides(*object, name), path + u'.' + name, *state);
        if(!value) {
            return std::unexpected{value.error()};
        }
        *state = *value;
    }

    const auto focus = readButtonFocus(object->value("focus"_L1), path + u".focus"_s, resolved.focus);
    if(!focus) {
        return std::unexpected{focus.error()};
    }
    resolved.focus = *focus;

    if(resolved.inset * 2 >= resolved.indicatorExtent) {
        return std::unexpected{
            errorWithPath(QCoreApplication::translate("ThemeAppearance",
                                                      "The indicator inset must be less than half the indicator size."),
                          path + u".inset"_s)};
    }

    const double border     = std::max({resolved.normal.borderWidth, resolved.hovered.borderWidth,
                                        resolved.pressed.borderWidth, resolved.disabled.borderWidth});
    const double focusSpace = resolved.focus.borderWidth > 0 ? resolved.focus.borderWidth + resolved.focus.spacing : 0;
    const double bodyInset  = std::max(resolved.inset, (border / 2.0) + focusSpace);
    if((bodyInset * 2) + border >= resolved.indicatorExtent) {
        return std::unexpected{errorWithPath(
            QCoreApplication::translate("ThemeAppearance",
                                        "The indicator size must allow room for its border and focus outline."),
            path + u".indicatorExtent"_s)};
    }

    return resolved;
}
} // namespace

CheckAppearance defaultRadioButtonAppearance()
{
    CheckAppearance appearance;
    appearance.markSize            = 6.0;
    appearance.hovered.borderWidth = 1.0;
    appearance.hovered.haloOpacity = 0;
    appearance.pressed.borderWidth = 1.0;
    appearance.pressed.haloOpacity = 0;
    return appearance;
}

std::expected<CheckAppearance, QString> resolveCheckBoxAppearance(const QJsonObject& appearance)
{
    return readAppearance(appearance, false);
}

std::expected<CheckAppearance, QString> resolveRadioButtonAppearance(const QJsonObject& appearance)
{
    return readAppearance(appearance, true);
}
} // namespace Fooyin
