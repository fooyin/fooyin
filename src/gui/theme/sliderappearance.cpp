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

#include "sliderappearance.h"

#include "appearanceutils.h"

#include <QCoreApplication>

#include <array>
#include <tuple>

using namespace Qt::StringLiterals;

namespace Fooyin {
using namespace Appearance;
namespace {
std::expected<SliderAppearance::Handle, QString> readHandle(const QJsonValue& value, const QString& path, int extent,
                                                            const SliderAppearance::Handle& defaults)
{
    auto resolved{defaults};
    const auto object = readObject(
        value, path, {"inset"_L1, "borderWidth"_L1, "cornerRadius"_L1, "backgroundColour"_L1, "borderColour"_L1});
    if(!object) {
        return std::unexpected{object.error()};
    }

    for(const auto& [name, target, minimum, maximum] :
        std::array{std::tuple{"inset"_L1, &resolved.inset, 0, 128},
                   std::tuple{"borderWidth"_L1, &resolved.borderWidth, 0, 32}}) {
        const auto field = readNumber(*object, name, path, *target, minimum, maximum);
        if(!field) {
            return std::unexpected{field.error()};
        }
        *target = *field;
    }

    const auto cornerRadius = readNumber(*object, "cornerRadius"_L1, path, resolved.cornerRadius, 0, 100);
    if(!cornerRadius) {
        return std::unexpected{cornerRadius.error()};
    }
    resolved.cornerRadius = *cornerRadius;

    const auto background = readBrushSource(object->value("backgroundColour"_L1), path + u".backgroundColour"_s,
                                            resolved.backgroundColour);
    if(!background) {
        return std::unexpected{background.error()};
    }
    resolved.backgroundColour = *background;

    const auto border
        = readColourSource(object->value("borderColour"_L1), path + u".borderColour"_s, resolved.borderColour);
    if(!border) {
        return std::unexpected{border.error()};
    }
    resolved.borderColour = *border;

    if(resolved.inset * 2 >= extent) {
        return std::unexpected{
            errorWithPath(QCoreApplication::translate("ThemeAppearance",
                                                      "The handle inset must be less than half the interaction size."),
                          path + u".inset"_s)};
    }
    if(resolved.borderWidth > resolved.inset * 2) {
        return std::unexpected{errorWithPath(
            QCoreApplication::translate("ThemeAppearance", "The handle border width must not exceed twice the inset."),
            path + u".borderWidth"_s)};
    }

    return resolved;
}

std::expected<SliderAppearance::GrooveState, QString> readGroove(const QJsonValue& value, const QString& path,
                                                                 const SliderAppearance::GrooveState& defaults)
{
    auto resolved{defaults};
    const auto object
        = readObject(value, path, {"thickness"_L1, "radius"_L1, "backgroundColour"_L1, "filledColour"_L1});
    if(!object) {
        return std::unexpected{object.error()};
    }

    const auto thickness = readNumber(*object, "thickness"_L1, path, resolved.thickness, 0, 64);
    if(!thickness) {
        return std::unexpected{thickness.error()};
    }
    resolved.thickness = *thickness;

    const auto radius = readNumber(*object, "radius"_L1, path, resolved.radius, 0, 128);
    if(!radius) {
        return std::unexpected{radius.error()};
    }
    resolved.radius = *radius;

    for(const auto& [name, target] : std::array{std::pair{"backgroundColour"_L1, &resolved.backgroundColour},
                                                std::pair{"filledColour"_L1, &resolved.filledColour}}) {
        const auto field = readBrushSource(object->value(name), path + u'.' + name, *target);
        if(!field) {
            return std::unexpected{field.error()};
        }
        *target = *field;
    }

    return resolved;
}

std::expected<SliderAppearance::Focus, QString> readFocus(const QJsonValue& value, const QString& path, int extent,
                                                          const SliderAppearance::Focus& defaults)
{
    auto resolved{defaults};
    const auto object = readObject(value, path, {"borderWidth"_L1, "borderColour"_L1});
    if(!object) {
        return std::unexpected{object.error()};
    }

    const auto borderWidth = readNumber(*object, "borderWidth"_L1, path, resolved.borderWidth, 0, std::min(32, extent));
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

    return resolved;
}
} // namespace

std::expected<SliderAppearance, QString> resolveSliderAppearance(const QJsonObject& appearance)
{
    SliderAppearance resolved;
    const QString path = u"appearance.slider"_s;
    const auto object  = readObject(appearance.value("slider"_L1), path,
                                    {"handleExtent"_L1, "tickmarkOffset"_L1, "groove"_L1, "normal"_L1, "hovered"_L1,
                                     "pressed"_L1, "disabled"_L1, "focus"_L1});
    if(!object) {
        return std::unexpected{object.error()};
    }

    for(const auto& [name, target, minimum, maximum] :
        std::array{std::tuple{"handleExtent"_L1, &resolved.handleExtent, 1, 256},
                   std::tuple{"tickmarkOffset"_L1, &resolved.tickmarkOffset, 0, 64}}) {
        const auto field = readNumber(*object, name, path, *target, minimum, maximum);
        if(!field) {
            return std::unexpected{field.error()};
        }
        *target = *field;
    }

    const auto groove = readObject(object->value("groove"_L1), path + u".groove"_s,
                                   {"normal"_L1, "hovered"_L1, "pressed"_L1, "disabled"_L1});
    if(!groove) {
        return std::unexpected{groove.error()};
    }

    for(const auto& [name, target] : std::array{
            std::pair{"normal"_L1, &resolved.groove.normal}, std::pair{"hovered"_L1, &resolved.groove.hovered},
            std::pair{"pressed"_L1, &resolved.groove.pressed}, std::pair{"disabled"_L1, &resolved.groove.disabled}}) {
        const auto state = readGroove(stateOverrides(*groove, name), path + u".groove."_s + name, *target);
        if(!state) {
            return std::unexpected{state.error()};
        }
        *target = *state;
    }

    for(const auto& [name, target] :
        std::array{std::pair{"normal"_L1, &resolved.normal}, std::pair{"hovered"_L1, &resolved.hovered},
                   std::pair{"pressed"_L1, &resolved.pressed}, std::pair{"disabled"_L1, &resolved.disabled}}) {
        const auto state
            = readHandle(stateOverrides(*object, name), path + u'.' + name, resolved.handleExtent, *target);
        if(!state) {
            return std::unexpected{state.error()};
        }
        *target = *state;
    }

    const auto focus = readFocus(object->value("focus"_L1), path + u".focus"_s, resolved.handleExtent, resolved.focus);
    if(!focus) {
        return std::unexpected{focus.error()};
    }
    resolved.focus = *focus;

    return resolved;
}
} // namespace Fooyin
