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

#include "headerappearance.h"

#include "appearanceutils.h"

#include <array>

using namespace Qt::StringLiterals;

namespace Fooyin {
using namespace Appearance;
namespace {
std::expected<HeaderAppearance::State, QString> readState(const QJsonValue& value, const QString& path,
                                                          const HeaderAppearance::State& defaults)
{
    auto resolved{defaults};
    const auto object = readObject(value, path,
                                   {"backgroundColour"_L1, "backgroundOpacity"_L1, "tintColour"_L1, "tintOpacity"_L1,
                                    "foregroundColour"_L1, "tintIcon"_L1, "iconColour"_L1});
    if(!object) {
        return std::unexpected{object.error()};
    }

    const auto backgroundColour = readBrushSource(object->value("backgroundColour"_L1), path + u".backgroundColour"_s,
                                                  resolved.backgroundColour);
    if(!backgroundColour) {
        return std::unexpected{backgroundColour.error()};
    }
    resolved.backgroundColour = *backgroundColour;

    for(const auto& [name, target] : std::array{std::pair{"tintColour"_L1, &resolved.tintColour},
                                                std::pair{"foregroundColour"_L1, &resolved.foregroundColour},
                                                std::pair{"iconColour"_L1, &resolved.iconColour}}) {
        const auto field = readColourSource(object->value(name), path + u'.' + name, *target);
        if(!field) {
            return std::unexpected{field.error()};
        }
        *target = *field;
    }

    for(const auto& [name, target] : std::array{std::pair{"backgroundOpacity"_L1, &resolved.backgroundOpacity},
                                                std::pair{"tintOpacity"_L1, &resolved.tintOpacity}}) {
        const auto field = readNumber(*object, name, path, *target, 0, 100);
        if(!field) {
            return std::unexpected{field.error()};
        }
        *target = *field;
    }

    for(const auto& [name, target] : std::array{std::pair{"tintIcon"_L1, &resolved.tintIcon}}) {
        const auto field = readBool(*object, name, path, *target);
        if(!field) {
            return std::unexpected{field.error()};
        }
        *target = *field;
    }

    return resolved;
}

std::expected<HeaderAppearance::Border, QString> readBorder(const QJsonValue& value, const QString& path,
                                                            const HeaderAppearance::Border& defaults)
{
    auto resolved{defaults};
    const auto object = readObject(value, path, {"width"_L1, "colour"_L1, "opacity"_L1, "shadingOpacity"_L1});
    if(!object) {
        return std::unexpected{object.error()};
    }

    for(const auto& [name, target] : std::array{std::pair{"width"_L1, &resolved.width}}) {
        const auto field = readNumber(*object, name, path, *target, 0, 32);
        if(!field) {
            return std::unexpected{field.error()};
        }
        *target = *field;
    }

    for(const auto& [name, target] : std::array{std::pair{"colour"_L1, &resolved.colour}}) {
        const auto field = readColourSource(object->value(name), path + u'.' + name, *target);
        if(!field) {
            return std::unexpected{field.error()};
        }
        *target = *field;
    }

    for(const auto& [name, target] : std::array{std::pair{"opacity"_L1, &resolved.opacity},
                                                std::pair{"shadingOpacity"_L1, &resolved.shadingOpacity}}) {
        const auto field = readNumber(*object, name, path, *target, 0, 100);
        if(!field) {
            return std::unexpected{field.error()};
        }
        *target = *field;
    }

    return resolved;
}

std::expected<HeaderAppearance::Separator, QString> readSeparator(const QJsonValue& value, const QString& path,
                                                                  const HeaderAppearance::Separator& defaults)
{
    auto resolved{defaults};
    const auto object = readObject(value, path, {"width"_L1, "inset"_L1, "colour"_L1, "opacity"_L1});
    if(!object) {
        return std::unexpected{object.error()};
    }

    for(const auto& [name, target] : std::array{std::pair{"width"_L1, &resolved.width}}) {
        const auto field = readNumber(*object, name, path, *target, 0, 32);
        if(!field) {
            return std::unexpected{field.error()};
        }
        *target = *field;
    }

    for(const auto& [name, target] : std::array{std::pair{"inset"_L1, &resolved.inset}}) {
        const auto field = readNumber(*object, name, path, *target, 0, 128);
        if(!field) {
            return std::unexpected{field.error()};
        }
        *target = *field;
    }

    for(const auto& [name, target] : std::array{std::pair{"colour"_L1, &resolved.colour}}) {
        const auto field = readColourSource(object->value(name), path + u'.' + name, *target);
        if(!field) {
            return std::unexpected{field.error()};
        }
        *target = *field;
    }

    for(const auto& [name, target] : std::array{std::pair{"opacity"_L1, &resolved.opacity}}) {
        const auto field = readNumber(*object, name, path, *target, 0, 100);
        if(!field) {
            return std::unexpected{field.error()};
        }
        *target = *field;
    }

    return resolved;
}

std::expected<HeaderAppearance::Arrow, QString> readArrow(const QJsonValue& value, const QString& path,
                                                          const HeaderAppearance::Arrow& defaults)
{
    auto resolved{defaults};
    const auto object = readObject(value, path, {"extent"_L1, "size"_L1, "width"_L1, "colour"_L1});
    if(!object) {
        return std::unexpected{object.error()};
    }

    for(const auto& [name, target] : std::array{std::pair{"extent"_L1, &resolved.extent}}) {
        const auto field = readNumber(*object, name, path, *target, 0, 128);
        if(!field) {
            return std::unexpected{field.error()};
        }
        *target = *field;
    }

    for(const auto& [name, target] : std::array{std::pair{"size"_L1, &resolved.size}}) {
        const auto field = readNumber(*object, name, path, *target, 0, 128);
        if(!field) {
            return std::unexpected{field.error()};
        }
        *target = *field;
    }

    for(const auto& [name, target] : std::array{std::pair{"width"_L1, &resolved.width}}) {
        const auto field = readNumber(*object, name, path, *target, 0, 32);
        if(!field) {
            return std::unexpected{field.error()};
        }
        *target = *field;
    }

    for(const auto& [name, target] : std::array{std::pair{"colour"_L1, &resolved.colour}}) {
        const auto field = readColourSource(object->value(name), path + u'.' + name, *target);
        if(!field) {
            return std::unexpected{field.error()};
        }
        *target = *field;
    }

    return resolved;
}

std::expected<HeaderAppearance::Focus, QString> readFocus(const QJsonValue& value, const QString& path,
                                                          const HeaderAppearance::Focus& defaults)
{
    auto resolved{defaults};
    const auto object = readObject(value, path, {"width"_L1, "inset"_L1, "radius"_L1, "colour"_L1, "keyboardOnly"_L1});
    if(!object) {
        return std::unexpected{object.error()};
    }

    for(const auto& [name, target] : std::array{std::pair{"width"_L1, &resolved.width}}) {
        const auto field = readNumber(*object, name, path, *target, 0, 32);
        if(!field) {
            return std::unexpected{field.error()};
        }
        *target = *field;
    }

    for(const auto& [name, target] :
        std::array{std::pair{"inset"_L1, &resolved.inset}, std::pair{"radius"_L1, &resolved.radius}}) {
        const auto field = readNumber(*object, name, path, *target, 0, 128);
        if(!field) {
            return std::unexpected{field.error()};
        }
        *target = *field;
    }

    for(const auto& [name, target] : std::array{std::pair{"colour"_L1, &resolved.colour}}) {
        const auto field = readColourSource(object->value(name), path + u'.' + name, *target);
        if(!field) {
            return std::unexpected{field.error()};
        }
        *target = *field;
    }

    for(const auto& [name, target] : std::array{std::pair{"keyboardOnly"_L1, &resolved.keyboardOnly}}) {
        const auto field = readBool(*object, name, path, *target);
        if(!field) {
            return std::unexpected{field.error()};
        }
        *target = *field;
    }

    return resolved;
}

} // namespace

std::expected<HeaderAppearance, QString> resolveHeaderAppearance(const QJsonObject& appearance)
{
    HeaderAppearance resolved;
    const QString path = u"appearance.header"_s;
    const auto object
        = readObject(appearance.value("header"_L1), path,
                     {"horizontalPadding"_L1, "verticalPadding"_L1, "minimumThickness"_L1, "spacing"_L1, "iconSize"_L1,
                      "emptyColour"_L1, "normal"_L1, "hovered"_L1, "pressed"_L1, "sorted"_L1, "selected"_L1,
                      "dragTarget"_L1, "disabled"_L1, "border"_L1, "separator"_L1, "arrow"_L1, "focus"_L1});
    if(!object) {
        return std::unexpected{object.error()};
    }

    for(const auto& [name, target] : std::array{std::pair{"horizontalPadding"_L1, &resolved.horizontalPadding},
                                                std::pair{"verticalPadding"_L1, &resolved.verticalPadding},
                                                std::pair{"spacing"_L1, &resolved.spacing}}) {
        const auto field = readNumber(*object, name, path, *target, 0, 128);
        if(!field) {
            return std::unexpected{field.error()};
        }
        *target = *field;
    }

    for(const auto& [name, target] : std::array{std::pair{"minimumThickness"_L1, &resolved.minimumThickness}}) {
        const auto field = readNumber(*object, name, path, *target, 0, 512);
        if(!field) {
            return std::unexpected{field.error()};
        }
        *target = *field;
    }

    for(const auto& [name, target] : std::array{std::pair{"iconSize"_L1, &resolved.iconSize}}) {
        const auto field = readNumber(*object, name, path, *target, 1, 128);
        if(!field) {
            return std::unexpected{field.error()};
        }
        *target = *field;
    }

    for(const auto& [name, target] : std::array{std::pair{"emptyColour"_L1, &resolved.emptyColour}}) {
        const auto field = readBrushSource(object->value(name), path + u'.' + name, *target);
        if(!field) {
            return std::unexpected{field.error()};
        }
        *target = *field;
    }

    for(const auto& [name, target] :
        std::array{std::pair{"normal"_L1, &resolved.normal}, std::pair{"hovered"_L1, &resolved.hovered},
                   std::pair{"pressed"_L1, &resolved.pressed}, std::pair{"sorted"_L1, &resolved.sorted},
                   std::pair{"selected"_L1, &resolved.selected}, std::pair{"dragTarget"_L1, &resolved.dragTarget},
                   std::pair{"disabled"_L1, &resolved.disabled}}) {
        const auto state = readState(stateOverrides(*object, name), path + u'.' + name, *target);
        if(!state) {
            return std::unexpected{state.error()};
        }
        *target = *state;
    }

    const auto border = readBorder(object->value("border"_L1), path + u".border"_s, resolved.border);
    if(!border) {
        return std::unexpected{border.error()};
    }
    resolved.border = *border;

    const auto separator = readSeparator(object->value("separator"_L1), path + u".separator"_s, resolved.separator);
    if(!separator) {
        return std::unexpected{separator.error()};
    }
    resolved.separator = *separator;

    const auto arrow = readArrow(object->value("arrow"_L1), path + u".arrow"_s, resolved.arrow);
    if(!arrow) {
        return std::unexpected{arrow.error()};
    }
    resolved.arrow = *arrow;

    const auto focus = readFocus(object->value("focus"_L1), path + u".focus"_s, resolved.focus);
    if(!focus) {
        return std::unexpected{focus.error()};
    }
    resolved.focus = *focus;

    return resolved;
}
} // namespace Fooyin
