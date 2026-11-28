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

#include "menuappearance.h"

#include "appearanceutils.h"

#include <array>

using namespace Qt::StringLiterals;

namespace Fooyin {
using namespace Appearance;
namespace {
std::expected<MenuAppearance::Surface, QString> readSurface(const QJsonValue& value, const QString& path,
                                                            const MenuAppearance::Surface& defaults)
{
    auto resolved{defaults};
    const auto object = readObject(value, path,
                                   {"backgroundColour"_L1, "backgroundOpacity"_L1, "borderWidth"_L1, "borderColour"_L1,
                                    "borderOpacity"_L1, "radius"_L1, "reliefOpacity"_L1});
    if(!object) {
        return std::unexpected{object.error()};
    }

    const auto backgroundColour = readBrushSource(object->value("backgroundColour"_L1), path + u".backgroundColour"_s,
                                                  resolved.backgroundColour);
    if(!backgroundColour) {
        return std::unexpected{backgroundColour.error()};
    }
    resolved.backgroundColour = *backgroundColour;

    for(const auto& [name, target] : std::array{std::pair{"borderColour"_L1, &resolved.borderColour}}) {
        const auto field = readColourSource(object->value(name), path + u'.' + name, *target);
        if(!field) {
            return std::unexpected{field.error()};
        }
        *target = *field;
    }

    for(const auto& [name, target] : std::array{std::pair{"backgroundOpacity"_L1, &resolved.backgroundOpacity},
                                                std::pair{"borderOpacity"_L1, &resolved.borderOpacity},
                                                std::pair{"reliefOpacity"_L1, &resolved.reliefOpacity}}) {
        const auto field = readNumber(*object, name, path, *target, 0, 100);
        if(!field) {
            return std::unexpected{field.error()};
        }
        *target = *field;
    }

    for(const auto& [name, target] : std::array{std::pair{"borderWidth"_L1, &resolved.borderWidth}}) {
        const auto field = readNumber(*object, name, path, *target, 0, 32);
        if(!field) {
            return std::unexpected{field.error()};
        }
        *target = *field;
    }

    for(const auto& [name, target] : std::array{std::pair{"radius"_L1, &resolved.radius}}) {
        const auto field = readNumber(*object, name, path, *target, 0, 128);
        if(!field) {
            return std::unexpected{field.error()};
        }
        *target = *field;
    }

    return resolved;
}

std::expected<MenuAppearance::State, QString> readState(const QJsonValue& value, const QString& path,
                                                        const MenuAppearance::State& defaults)
{
    auto resolved{defaults};
    const auto object = readObject(value, path,
                                   {"backgroundColour"_L1, "backgroundOpacity"_L1, "borderWidth"_L1, "borderColour"_L1,
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

    for(const auto& [name, target] : std::array{std::pair{"borderColour"_L1, &resolved.borderColour},
                                                std::pair{"foregroundColour"_L1, &resolved.foregroundColour},
                                                std::pair{"iconColour"_L1, &resolved.iconColour}}) {
        const auto field = readColourSource(object->value(name), path + u'.' + name, *target);
        if(!field) {
            return std::unexpected{field.error()};
        }
        *target = *field;
    }

    for(const auto& [name, target] : std::array{std::pair{"backgroundOpacity"_L1, &resolved.backgroundOpacity}}) {
        const auto field = readNumber(*object, name, path, *target, 0, 100);
        if(!field) {
            return std::unexpected{field.error()};
        }
        *target = *field;
    }

    for(const auto& [name, target] : std::array{std::pair{"borderWidth"_L1, &resolved.borderWidth}}) {
        const auto field = readNumber(*object, name, path, *target, 0, 32);
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

std::expected<MenuAppearance::Separator, QString> readSeparator(const QJsonValue& value, const QString& path,
                                                                const MenuAppearance::Separator& defaults)
{
    auto resolved{defaults};
    const auto object = readObject(value, path,
                                   {"height"_L1, "sectionPadding"_L1, "minimumWidth"_L1, "inset"_L1, "spacing"_L1,
                                    "lineWidth"_L1, "lineColour"_L1, "foregroundColour"_L1, "bold"_L1});
    if(!object) {
        return std::unexpected{object.error()};
    }

    for(const auto& [name, target] :
        std::array{std::pair{"height"_L1, &resolved.height}, std::pair{"sectionPadding"_L1, &resolved.sectionPadding},
                   std::pair{"inset"_L1, &resolved.inset}, std::pair{"spacing"_L1, &resolved.spacing}}) {
        const auto field = readNumber(*object, name, path, *target, 0, 128);
        if(!field) {
            return std::unexpected{field.error()};
        }
        *target = *field;
    }

    for(const auto& [name, target] : std::array{std::pair{"minimumWidth"_L1, &resolved.minimumWidth}}) {
        const auto field = readNumber(*object, name, path, *target, 0, 512);
        if(!field) {
            return std::unexpected{field.error()};
        }
        *target = *field;
    }

    for(const auto& [name, target] : std::array{std::pair{"lineWidth"_L1, &resolved.lineWidth}}) {
        const auto field = readNumber(*object, name, path, *target, 0, 32);
        if(!field) {
            return std::unexpected{field.error()};
        }
        *target = *field;
    }

    for(const auto& [name, target] : std::array{std::pair{"lineColour"_L1, &resolved.lineColour},
                                                std::pair{"foregroundColour"_L1, &resolved.foregroundColour}}) {
        const auto field = readColourSource(object->value(name), path + u'.' + name, *target);
        if(!field) {
            return std::unexpected{field.error()};
        }
        *target = *field;
    }

    for(const auto& [name, target] : std::array{std::pair{"bold"_L1, &resolved.bold}}) {
        const auto field = readBool(*object, name, path, *target);
        if(!field) {
            return std::unexpected{field.error()};
        }
        *target = *field;
    }

    return resolved;
}

std::expected<MenuAppearance::Focus, QString> readFocus(const QJsonValue& value, const QString& path,
                                                        const MenuAppearance::Focus& defaults)
{
    auto resolved{defaults};
    const auto object
        = readObject(value, path, {"borderWidth"_L1, "inset"_L1, "radius"_L1, "borderColour"_L1, "keyboardOnly"_L1});
    if(!object) {
        return std::unexpected{object.error()};
    }

    for(const auto& [name, target] : std::array{std::pair{"borderWidth"_L1, &resolved.borderWidth}}) {
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

    for(const auto& [name, target] : std::array{std::pair{"borderColour"_L1, &resolved.borderColour}}) {
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

std::expected<MenuAppearance::Bar, QString> readBar(const QJsonValue& value, const QString& path,
                                                    const MenuAppearance::Bar& defaults)
{
    auto resolved{defaults};
    const auto object = readObject(value, path,
                                   {"horizontalPadding"_L1, "verticalPadding"_L1, "horizontalMargin"_L1,
                                    "verticalMargin"_L1, "spacing"_L1, "inset"_L1, "radius"_L1, "panel"_L1, "normal"_L1,
                                    "selected"_L1, "pressed"_L1, "disabled"_L1, "focus"_L1});
    if(!object) {
        return std::unexpected{object.error()};
    }

    for(const auto& [name, target] : std::array{std::pair{"horizontalPadding"_L1, &resolved.horizontalPadding},
                                                std::pair{"verticalPadding"_L1, &resolved.verticalPadding},
                                                std::pair{"horizontalMargin"_L1, &resolved.horizontalMargin},
                                                std::pair{"verticalMargin"_L1, &resolved.verticalMargin},
                                                std::pair{"spacing"_L1, &resolved.spacing}}) {
        const auto field = readNumber(*object, name, path, *target, 0, 128);
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

    const auto panel = readSurface(object->value("panel"_L1), path + u".panel"_s, resolved.panel);
    if(!panel) {
        return std::unexpected{panel.error()};
    }
    resolved.panel = *panel;

    for(const auto& [name, target] :
        std::array{std::pair{"normal"_L1, &resolved.normal}, std::pair{"selected"_L1, &resolved.selected},
                   std::pair{"pressed"_L1, &resolved.pressed}, std::pair{"disabled"_L1, &resolved.disabled}}) {
        const auto state = readState(stateOverrides(*object, name), path + u'.' + name, *target);
        if(!state) {
            return std::unexpected{state.error()};
        }
        *target = *state;
    }

    const auto focus = readFocus(object->value("focus"_L1), path + u".focus"_s, resolved.focus);
    if(!focus) {
        return std::unexpected{focus.error()};
    }
    resolved.focus = *focus;

    return resolved;
}

} // namespace

std::expected<MenuAppearance, QString> resolveMenuAppearance(const QJsonObject& appearance)
{
    MenuAppearance resolved;
    const QString path = u"appearance.menu"_s;
    const auto object  = readObject(appearance.value("menu"_L1), path,
                                    {"horizontalPadding"_L1,
                                     "verticalPadding"_L1,
                                     "minimumWidth"_L1,
                                     "minimumHeight"_L1,
                                     "horizontalMargin"_L1,
                                     "verticalMargin"_L1,
                                     "columnSpacing"_L1,
                                     "checkExtent"_L1,
                                     "iconExtent"_L1,
                                     "iconSize"_L1,
                                     "arrowExtent"_L1,
                                     "arrowSize"_L1,
                                     "arrowWidth"_L1,
                                     "shortcutSpacing"_L1,
                                     "auxiliaryExtent"_L1,
                                     "inset"_L1,
                                     "radius"_L1,
                                     "panel"_L1,
                                     "normal"_L1,
                                     "selected"_L1,
                                     "pressed"_L1,
                                     "disabled"_L1,
                                     "separator"_L1,
                                     "bar"_L1});
    if(!object) {
        return std::unexpected{object.error()};
    }

    for(const auto& [name, target] : std::array{
            std::pair{"horizontalPadding"_L1, &resolved.horizontalPadding},
            std::pair{"verticalPadding"_L1, &resolved.verticalPadding},
            std::pair{"horizontalMargin"_L1, &resolved.horizontalMargin},
            std::pair{"verticalMargin"_L1, &resolved.verticalMargin},
            std::pair{"columnSpacing"_L1, &resolved.columnSpacing}, std::pair{"checkExtent"_L1, &resolved.checkExtent},
            std::pair{"iconExtent"_L1, &resolved.iconExtent}, std::pair{"arrowExtent"_L1, &resolved.arrowExtent},
            std::pair{"shortcutSpacing"_L1, &resolved.shortcutSpacing},
            std::pair{"auxiliaryExtent"_L1, &resolved.auxiliaryExtent}}) {
        const auto field = readNumber(*object, name, path, *target, 0, 128);
        if(!field) {
            return std::unexpected{field.error()};
        }
        *target = *field;
    }

    for(const auto& [name, target] : std::array{std::pair{"minimumWidth"_L1, &resolved.minimumWidth},
                                                std::pair{"minimumHeight"_L1, &resolved.minimumHeight}}) {
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

    for(const auto& [name, target] :
        std::array{std::pair{"arrowSize"_L1, &resolved.arrowSize}, std::pair{"inset"_L1, &resolved.inset},
                   std::pair{"radius"_L1, &resolved.radius}}) {
        const auto field = readNumber(*object, name, path, *target, 0, 128);
        if(!field) {
            return std::unexpected{field.error()};
        }
        *target = *field;
    }

    for(const auto& [name, target] : std::array{std::pair{"arrowWidth"_L1, &resolved.arrowWidth}}) {
        const auto field = readNumber(*object, name, path, *target, 0, 32);
        if(!field) {
            return std::unexpected{field.error()};
        }
        *target = *field;
    }

    const auto panel = readSurface(object->value("panel"_L1), path + u".panel"_s, resolved.panel);
    if(!panel) {
        return std::unexpected{panel.error()};
    }
    resolved.panel = *panel;

    for(const auto& [name, target] :
        std::array{std::pair{"normal"_L1, &resolved.normal}, std::pair{"selected"_L1, &resolved.selected},
                   std::pair{"pressed"_L1, &resolved.pressed}, std::pair{"disabled"_L1, &resolved.disabled}}) {
        const auto state = readState(stateOverrides(*object, name), path + u'.' + name, *target);
        if(!state) {
            return std::unexpected{state.error()};
        }
        *target = *state;
    }

    const auto separator = readSeparator(object->value("separator"_L1), path + u".separator"_s, resolved.separator);
    if(!separator) {
        return std::unexpected{separator.error()};
    }
    resolved.separator = *separator;

    const auto bar = readBar(object->value("bar"_L1), path + u".bar"_s, resolved.bar);
    if(!bar) {
        return std::unexpected{bar.error()};
    }
    resolved.bar = *bar;

    return resolved;
}

} // namespace Fooyin
