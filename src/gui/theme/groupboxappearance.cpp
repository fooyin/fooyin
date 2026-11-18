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

#include "groupboxappearance.h"

#include "appearanceutils.h"

#include <QCoreApplication>

#include <array>
#include <tuple>

using namespace Qt::StringLiterals;

namespace Fooyin {
using namespace Appearance;
namespace {
std::expected<QFont, QString> readTitleFont(const QJsonValue& value, const QString& path)
{
    const auto object = readObject(value, path, {"pointSize"_L1, "weight"_L1, "italic"_L1});
    if(!object) {
        return std::unexpected{object.error()};
    }

    QFont font;
    const auto size = readNumber(*object, "pointSize"_L1, path, 0.0, 0, 128);
    if(!size) {
        return std::unexpected{size.error()};
    }
    if(*size > 0) {
        font.setPointSizeF(*size);
    }

    const auto weight = readNumber(*object, "weight"_L1, path, 0, 0, 1000);
    if(!weight) {
        return std::unexpected{weight.error()};
    }
    if(*weight > 0) {
        font.setWeight(static_cast<QFont::Weight>(*weight));
    }

    const auto italic = object->value("italic"_L1);
    if(!italic.isUndefined() && !italic.isNull()) {
        const auto field = readBool(*object, "italic"_L1, path, false);
        if(!field) {
            return std::unexpected{field.error()};
        }
        font.setItalic(*field);
    }
    if(font.resolveMask() & (QFont::WeightResolved | QFont::StyleResolved)) {
        // A named style inherited from the widget can otherwise take precedence over weight and italic.
        font.setStyleName({});
    }
    return font;
}

std::expected<GroupBoxAppearance::Title, QString> readTitle(const QJsonValue& value, const QString& path,
                                                            const GroupBoxAppearance::Title& defaults)
{
    auto resolved{defaults};
    const auto object = readObject(
        value, path, {"placement"_L1, "alignment"_L1, "horizontalMargin"_L1, "inset"_L1, "gap"_L1, "font"_L1});
    if(!object) {
        return std::unexpected{object.error()};
    }

    const auto placement = object->value("placement"_L1);
    if(!placement.isUndefined()) {
        const auto name = placement.toString();
        if(!placement.isString() || (name != "inside"_L1 && name != "above"_L1)) {
            return std::unexpected{errorWithPath(
                QCoreApplication::translate("ThemeAppearance", "Choose a title placement inside or above the frame."),
                path + u".placement"_s)};
        }
        resolved.placement = name;
    }

    const auto alignment = object->value("alignment"_L1);
    if(!alignment.isUndefined()) {
        const auto name = alignment.toString();
        if(!alignment.isString() || (name != "center"_L1 && name != "leading"_L1 && name != "trailing"_L1)) {
            return std::unexpected{
                errorWithPath(QCoreApplication::translate("ThemeAppearance",
                                                          "Choose a leading, centred or trailing title alignment."),
                              path + u".alignment"_s)};
        }
        resolved.alignment = name;
    }

    for(const auto& [name, target, minimum] :
        std::array{std::tuple{"horizontalMargin"_L1, &resolved.horizontalMargin, -128},
                   std::tuple{"inset"_L1, &resolved.inset, 0}, std::tuple{"gap"_L1, &resolved.gap, 0}}) {
        const auto field = readNumber(*object, name, path, *target, minimum, 128);
        if(!field) {
            return std::unexpected{field.error()};
        }
        *target = *field;
    }

    const auto font = readTitleFont(object->value("font"_L1), path + u".font"_s);
    if(!font) {
        return std::unexpected{font.error()};
    }
    resolved.font = *font;

    return resolved;
}

std::expected<GroupBoxAppearance::Colours, QString> readColours(const QJsonValue& value, const QString& path,
                                                                const GroupBoxAppearance::Colours& defaults)
{
    auto resolved{defaults};
    const auto object = readObject(value, path,
                                   {"backgroundVisible"_L1, "backgroundColour"_L1, "backgroundTintColour"_L1,
                                    "backgroundTintOpacity"_L1, "borderColour"_L1, "borderTintColour"_L1,
                                    "borderTintOpacity"_L1, "titleColour"_L1});
    if(!object) {
        return std::unexpected{object.error()};
    }

    const auto visible = readBool(*object, "backgroundVisible"_L1, path, resolved.backgroundVisible);
    if(!visible) {
        return std::unexpected{visible.error()};
    }
    resolved.backgroundVisible = *visible;

    const std::array colours{std::pair{"backgroundColour"_L1, &resolved.backgroundColour},
                             std::pair{"backgroundTintColour"_L1, &resolved.backgroundTintColour},
                             std::pair{"borderColour"_L1, &resolved.borderColour},
                             std::pair{"borderTintColour"_L1, &resolved.borderTintColour},
                             std::pair{"titleColour"_L1, &resolved.titleColour}};
    for(const auto& [name, colour] : colours) {
        const auto source = readColourSource(object->value(name), path + u'.' + name, *colour);
        if(!source) {
            return std::unexpected{source.error()};
        }
        *colour = *source;
    }

    for(const auto& [name, target] : std::array{std::pair{"backgroundTintOpacity"_L1, &resolved.backgroundTintOpacity},
                                                std::pair{"borderTintOpacity"_L1, &resolved.borderTintOpacity}}) {
        const auto field = readNumber(*object, name, path, *target, 0, 100);
        if(!field) {
            return std::unexpected{field.error()};
        }
        *target = *field;
    }

    return resolved;
}
} // namespace

std::expected<GroupBoxAppearance, QString> resolveGroupBoxAppearance(const QJsonObject& appearance)
{
    GroupBoxAppearance resolved;
    const QString path = u"appearance.groupBox"_s;
    const auto object  = readObject(appearance.value("groupBox"_L1), path,
                                    {"horizontalPadding"_L1, "verticalPadding"_L1, "radius"_L1, "borderWidth"_L1,
                                     "reliefOpacity"_L1, "title"_L1, "normal"_L1, "disabled"_L1});
    if(!object) {
        return std::unexpected{object.error()};
    }

    for(const auto& [name, target] : std::array{std::pair{"horizontalPadding"_L1, &resolved.horizontalPadding},
                                                std::pair{"verticalPadding"_L1, &resolved.verticalPadding}}) {
        const auto field = readNumber(*object, name, path, *target, 0, 128);
        if(!field) {
            return std::unexpected{field.error()};
        }
        *target = *field;
    }

    for(const auto& [name, target, minimum, maximum] :
        std::array{std::tuple{"radius"_L1, &resolved.radius, 0, 128},
                   std::tuple{"borderWidth"_L1, &resolved.borderWidth, 0, 32}}) {
        const auto field = readNumber(*object, name, path, *target, minimum, maximum);
        if(!field) {
            return std::unexpected{field.error()};
        }
        *target = *field;
    }

    const auto reliefOpacity = readNumber(*object, "reliefOpacity"_L1, path, resolved.reliefOpacity, 0, 100);
    if(!reliefOpacity) {
        return std::unexpected{reliefOpacity.error()};
    }
    resolved.reliefOpacity = *reliefOpacity;

    const auto title = readTitle(object->value("title"_L1), path + u".title"_s, resolved.title);
    if(!title) {
        return std::unexpected{title.error()};
    }
    resolved.title = *title;

    const std::array states{std::pair{"normal"_L1, &resolved.normal}, std::pair{"disabled"_L1, &resolved.disabled}};
    for(const auto& [name, colours] : states) {
        const auto state = readColours(stateOverrides(*object, name), path + u'.' + name, *colours);
        if(!state) {
            return std::unexpected{state.error()};
        }
        *colours = *state;
    }

    return resolved;
}
} // namespace Fooyin
