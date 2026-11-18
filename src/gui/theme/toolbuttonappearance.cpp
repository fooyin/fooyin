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

#include "toolbuttonappearance.h"

#include "appearanceutils.h"

#include <array>
#include <tuple>

using namespace Qt::StringLiterals;

namespace Fooyin {
using namespace Appearance;
std::expected<ToolButtonAppearance, QString> resolveToolButtonAppearance(const QJsonObject& appearance)
{
    ToolButtonAppearance resolved;
    const QString path = u"appearance.toolButton"_s;
    const auto object = readObject(appearance.value("toolButton"_L1), path,
                                   {"minimumWidth"_L1, "minimumHeight"_L1, "horizontalPadding"_L1, "verticalPadding"_L1,
                                    "radius"_L1, "normal"_L1, "hovered"_L1, "pressed"_L1, "disabled"_L1, "checked"_L1,
                                    "flatBackground"_L1, "flatBorder"_L1, "focus"_L1, "menu"_L1});
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

    for(const auto& [name, target] : std::array{std::pair{"flatBackground"_L1, &resolved.flatBackground},
                                                std::pair{"flatBorder"_L1, &resolved.flatBorder}}) {
        const auto field = readBool(*object, name, path, *target);
        if(!field) {
            return std::unexpected{field.error()};
        }
        *target = *field;
    }

    const std::array surfaces{std::pair{"normal"_L1, &resolved.normal}, std::pair{"hovered"_L1, &resolved.hovered},
                              std::pair{"pressed"_L1, &resolved.pressed}, std::pair{"disabled"_L1, &resolved.disabled},
                              std::pair{"checked"_L1, &resolved.checked}};
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

    const QString menuPath = path + u".menu"_s;
    const auto menu
        = readObject(object->value("menu"_L1), menuPath,
                     {"indicatorWidth"_L1, "separatorVisible"_L1, "separatorWidth"_L1, "separatorInset"_L1,
                      "separatorColour"_L1, "normal"_L1, "hovered"_L1, "pressed"_L1, "disabled"_L1, "checked"_L1});
    if(!menu) {
        return std::unexpected{menu.error()};
    }

    const auto indicatorWidth = readNumber(*menu, "indicatorWidth"_L1, menuPath, resolved.menu.indicatorWidth, 8, 128);
    if(!indicatorWidth) {
        return std::unexpected{indicatorWidth.error()};
    }
    resolved.menu.indicatorWidth = *indicatorWidth;

    const auto separatorWidth = readNumber(*menu, "separatorWidth"_L1, menuPath, resolved.menu.separatorWidth, 0, 32);
    if(!separatorWidth) {
        return std::unexpected{separatorWidth.error()};
    }
    resolved.menu.separatorWidth = *separatorWidth;

    const auto separatorInset = readNumber(*menu, "separatorInset"_L1, menuPath, resolved.menu.separatorInset, 0, 128);
    if(!separatorInset) {
        return std::unexpected{separatorInset.error()};
    }
    resolved.menu.separatorInset = *separatorInset;

    const auto separatorVisible = readBool(*menu, "separatorVisible"_L1, menuPath, resolved.menu.separatorVisible);
    if(!separatorVisible) {
        return std::unexpected{separatorVisible.error()};
    }
    resolved.menu.separatorVisible = *separatorVisible;

    const auto separatorColour = readColourSource(menu->value("separatorColour"_L1), menuPath + u".separatorColour"_s,
                                                  resolved.menu.separatorColour);
    if(!separatorColour) {
        return std::unexpected{separatorColour.error()};
    }
    resolved.menu.separatorColour = *separatorColour;

    const std::array menuSurfaces{
        std::pair{"normal"_L1, &resolved.menu.normal}, std::pair{"hovered"_L1, &resolved.menu.hovered},
        std::pair{"pressed"_L1, &resolved.menu.pressed}, std::pair{"disabled"_L1, &resolved.menu.disabled},
        std::pair{"checked"_L1, &resolved.menu.checked}};
    for(size_t index{0}; index < menuSurfaces.size(); ++index) {
        const auto& [name, surface] = menuSurfaces[index];
        const auto state
            = readButtonSurface(stateOverrides(*menu, name), menuPath + u'.' + name, *surfaces[index].second);
        if(!state) {
            return std::unexpected{state.error()};
        }
        *surface = *state;
    }

    return resolved;
}
} // namespace Fooyin
