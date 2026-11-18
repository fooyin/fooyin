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

#include "pushbuttonappearance.h"

#include "appearanceutils.h"

#include <array>
#include <tuple>

using namespace Qt::StringLiterals;

namespace Fooyin {
using namespace Appearance;
std::expected<PushButtonAppearance, QString> resolvePushButtonAppearance(const QJsonObject& appearance)
{
    PushButtonAppearance resolved;
    const QString path = u"appearance.pushButton"_s;
    const auto object = readObject(appearance.value("pushButton"_L1), path,
                                   {"minimumWidth"_L1, "minimumHeight"_L1, "horizontalPadding"_L1, "verticalPadding"_L1,
                                    "radius"_L1, "normal"_L1, "hovered"_L1, "pressed"_L1, "disabled"_L1, "checked"_L1,
                                    "defaultButton"_L1, "focus"_L1});
    if(!object) {
        return std::unexpected{object.error()};
    }

    for(const auto& [name, target, minimum, maximum] :
        std::array{std::tuple{"minimumWidth"_L1, &resolved.minimumWidth, 0, 512},
                   std::tuple{"minimumHeight"_L1, &resolved.minimumHeight, 0, 256},
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

    const std::array surfaces{
        std::pair{"normal"_L1, &resolved.normal},   std::pair{"hovered"_L1, &resolved.hovered},
        std::pair{"pressed"_L1, &resolved.pressed}, std::pair{"disabled"_L1, &resolved.disabled},
        std::pair{"checked"_L1, &resolved.checked}, std::pair{"defaultButton"_L1, &resolved.defaultButton}};
    for(const auto& [name, surface] : surfaces) {
        const auto state = readButtonSurface(stateOverrides(*object, name), path + u'.' + name, *surface);
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

    return resolved;
}
} // namespace Fooyin
