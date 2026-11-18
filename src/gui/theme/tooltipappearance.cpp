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

#include "tooltipappearance.h"

#include "appearanceutils.h"

#include <array>
#include <tuple>

using namespace Qt::StringLiterals;

namespace Fooyin {
using namespace Appearance;
std::expected<ToolTipAppearance, QString> resolveToolTipAppearance(const QJsonObject& appearance)
{
    ToolTipAppearance resolved;
    const QString path = u"appearance.toolTip"_s;
    const auto object
        = readObject(appearance.value("toolTip"_L1), path,
                     {"padding"_L1, "radius"_L1, "borderWidth"_L1, "backgroundColour"_L1, "backgroundOpacity"_L1,
                      "foregroundColour"_L1, "borderColour"_L1, "borderOpacity"_L1});
    if(!object) {
        return std::unexpected{object.error()};
    }

    const auto padding = readNumber(*object, "padding"_L1, path, resolved.padding, 1, 128);
    if(!padding) {
        return std::unexpected{padding.error()};
    }
    resolved.padding = *padding;

    for(const auto& [name, target, minimum, maximum] :
        std::array{std::tuple{"radius"_L1, &resolved.radius, 0, 128},
                   std::tuple{"borderWidth"_L1, &resolved.borderWidth, 0, 32}}) {
        const auto field = readNumber(*object, name, path, *target, minimum, maximum);
        if(!field) {
            return std::unexpected{field.error()};
        }
        *target = *field;
    }

    for(const auto& [name, target] : std::array{std::pair{"backgroundColour"_L1, &resolved.backgroundColour},
                                                std::pair{"foregroundColour"_L1, &resolved.foregroundColour},
                                                std::pair{"borderColour"_L1, &resolved.borderColour}}) {
        const auto colour = readColourSource(object->value(name), path + u'.' + name, *target);
        if(!colour) {
            return std::unexpected{colour.error()};
        }
        *target = *colour;
    }

    for(const auto& [name, target] : std::array{std::pair{"backgroundOpacity"_L1, &resolved.backgroundOpacity},
                                                std::pair{"borderOpacity"_L1, &resolved.borderOpacity}}) {
        const auto opacity = readNumber(*object, name, path, *target, 0, 100);
        if(!opacity) {
            return std::unexpected{opacity.error()};
        }
        *target = *opacity;
    }

    return resolved;
}
} // namespace Fooyin
