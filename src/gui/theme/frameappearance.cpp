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

#include "frameappearance.h"

#include "appearanceutils.h"

#include <array>
#include <tuple>

using namespace Qt::StringLiterals;

namespace Fooyin {
using namespace Appearance;
namespace {
std::expected<FrameAppearance::Separator, QString> readSeparator(const QJsonValue& value, const QString& path,
                                                                 const FrameAppearance::Separator& defaults)
{
    auto resolved{defaults};
    const auto object = readObject(value, path, {"width"_L1, "inset"_L1, "colour"_L1, "opacity"_L1});
    if(!object) {
        return std::unexpected{object.error()};
    }

    for(const auto& [name, target, maximum] :
        std::array{std::tuple{"width"_L1, &resolved.width, 32}, std::tuple{"inset"_L1, &resolved.inset, 128}}) {
        const auto number = readNumber(*object, name, path, *target, 0, maximum);
        if(!number) {
            return std::unexpected{number.error()};
        }
        *target = *number;
    }

    const auto colour = readColourSource(object->value("colour"_L1), path + u".colour"_s, resolved.colour);
    if(!colour) {
        return std::unexpected{colour.error()};
    }
    resolved.colour = *colour;

    const auto opacity = readNumber(*object, "opacity"_L1, path, resolved.opacity, 0, 100);
    if(!opacity) {
        return std::unexpected{opacity.error()};
    }
    resolved.opacity = *opacity;

    return resolved;
}
} // namespace

std::expected<FrameAppearance, QString> resolveFrameAppearance(const QJsonObject& appearance)
{
    FrameAppearance resolved;
    const QString path = u"appearance.frame"_s;
    const auto object
        = readObject(appearance.value("frame"_L1), path,
                     {"borderWidth"_L1, "radius"_L1, "borderColour"_L1, "borderOpacity"_L1, "separator"_L1});
    if(!object) {
        return std::unexpected{object.error()};
    }

    for(const auto& [name, target, maximum] : std::array{std::tuple{"borderWidth"_L1, &resolved.borderWidth, 32},
                                                         std::tuple{"radius"_L1, &resolved.radius, 128}}) {
        const auto number = readNumber(*object, name, path, *target, 0, maximum);
        if(!number) {
            return std::unexpected{number.error()};
        }
        *target = *number;
    }

    const auto colour
        = readColourSource(object->value("borderColour"_L1), path + u".borderColour"_s, resolved.borderColour);
    if(!colour) {
        return std::unexpected{colour.error()};
    }
    resolved.borderColour = *colour;

    const auto opacity = readNumber(*object, "borderOpacity"_L1, path, resolved.borderOpacity, 0, 100);
    if(!opacity) {
        return std::unexpected{opacity.error()};
    }
    resolved.borderOpacity = *opacity;

    const auto separator = readSeparator(object->value("separator"_L1), path + u".separator"_s, resolved.separator);
    if(!separator) {
        return std::unexpected{separator.error()};
    }
    resolved.separator = *separator;

    return resolved;
}
} // namespace Fooyin
