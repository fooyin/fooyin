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

#include "buttonappearance.h"

#include "appearanceutils.h"

#include <array>
#include <tuple>

using namespace Qt::StringLiterals;

namespace Fooyin::Appearance {
std::expected<ButtonSurface, QString> readButtonSurface(const QJsonValue& value, const QString& path,
                                                        const ButtonSurface& defaults)
{
    auto resolved{defaults};
    const auto object = readObject(value, path,
                                   {"backgroundVisible"_L1, "borderVisible"_L1, "tintIcon"_L1, "borderWidth"_L1,
                                    "backgroundColour"_L1, "foregroundColour"_L1, "iconColour"_L1, "borderColour"_L1,
                                    "tintColour"_L1, "tintOpacity"_L1});
    if(!object) {
        return std::unexpected{object.error()};
    }

    for(const auto& [name, target] : std::array{std::pair{"backgroundVisible"_L1, &resolved.backgroundVisible},
                                                std::pair{"borderVisible"_L1, &resolved.borderVisible},
                                                std::pair{"tintIcon"_L1, &resolved.tintIcon}}) {
        const auto field = readBool(*object, name, path, *target);
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

    const auto backgroundColour = readBrushSource(object->value("backgroundColour"_L1), path + u".backgroundColour"_s,
                                                  resolved.backgroundColour);
    if(!backgroundColour) {
        return std::unexpected{backgroundColour.error()};
    }
    resolved.backgroundColour = *backgroundColour;

    for(const auto& [name, target] : std::array{std::pair{"foregroundColour"_L1, &resolved.foregroundColour},
                                                std::pair{"iconColour"_L1, &resolved.iconColour},
                                                std::pair{"borderColour"_L1, &resolved.borderColour},
                                                std::pair{"tintColour"_L1, &resolved.tintColour}}) {
        const auto field = readColourSource(object->value(name), path + u'.' + name, *target);
        if(!field) {
            return std::unexpected{field.error()};
        }
        *target = *field;
    }

    const auto tintOpacity = readNumber(*object, "tintOpacity"_L1, path, resolved.tintOpacity, 0, 100);
    if(!tintOpacity) {
        return std::unexpected{tintOpacity.error()};
    }
    resolved.tintOpacity = *tintOpacity;

    return resolved;
}

std::expected<ButtonFocus, QString> readButtonFocus(const QJsonValue& value, const QString& path,
                                                    const ButtonFocus& defaults)
{
    auto resolved{defaults};
    const auto object = readObject(value, path, {"borderWidth"_L1, "spacing"_L1, "borderColour"_L1});
    if(!object) {
        return std::unexpected{object.error()};
    }

    for(const auto& [name, target, minimum, maximum] :
        std::array{std::tuple{"borderWidth"_L1, &resolved.borderWidth, 0, 32},
                   std::tuple{"spacing"_L1, &resolved.spacing, 0, 64}}) {
        const auto field = readNumber(*object, name, path, *target, minimum, maximum);
        if(!field) {
            return std::unexpected{field.error()};
        }
        *target = *field;
    }

    const auto borderColour
        = readColourSource(object->value("borderColour"_L1), path + u".borderColour"_s, resolved.borderColour);
    if(!borderColour) {
        return std::unexpected{borderColour.error()};
    }
    resolved.borderColour = *borderColour;

    return resolved;
}
} // namespace Fooyin::Appearance
