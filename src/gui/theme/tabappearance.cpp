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

#include "tabappearance.h"

#include "appearanceutils.h"

#include <QCoreApplication>

#include <array>
#include <tuple>

using namespace Qt::StringLiterals;

namespace Fooyin {
using namespace Appearance;
namespace {
std::expected<TabAppearance::Surface, QString> readSurface(const QJsonValue& value, const QString& path,
                                                           const TabAppearance::Surface& defaults)
{
    auto resolved{defaults};
    const auto object = readObject(value, path,
                                   {"backgroundOpacity"_L1, "borderWidth"_L1, "tintIcon"_L1, "backgroundColour"_L1,
                                    "borderColour"_L1, "foregroundColour"_L1, "iconColour"_L1});
    if(!object) {
        return std::unexpected{object.error()};
    }

    const auto backgroundOpacity
        = readNumber(*object, "backgroundOpacity"_L1, path, resolved.backgroundOpacity, 0, 100);
    if(!backgroundOpacity) {
        return std::unexpected{backgroundOpacity.error()};
    }
    resolved.backgroundOpacity = *backgroundOpacity;

    const auto borderWidth = readNumber(*object, "borderWidth"_L1, path, resolved.borderWidth, 0, 32);
    if(!borderWidth) {
        return std::unexpected{borderWidth.error()};
    }
    resolved.borderWidth = *borderWidth;

    const auto tintIcon = readBool(*object, "tintIcon"_L1, path, resolved.tintIcon);
    if(!tintIcon) {
        return std::unexpected{tintIcon.error()};
    }
    resolved.tintIcon = *tintIcon;

    const auto backgroundColour = readBrushSource(object->value("backgroundColour"_L1), path + u".backgroundColour"_s,
                                                  resolved.backgroundColour);
    if(!backgroundColour) {
        return std::unexpected{backgroundColour.error()};
    }
    resolved.backgroundColour = *backgroundColour;

    const std::array colours{std::pair{"borderColour"_L1, &resolved.borderColour},
                             std::pair{"foregroundColour"_L1, &resolved.foregroundColour},
                             std::pair{"iconColour"_L1, &resolved.iconColour}};
    for(const auto& [name, target] : colours) {
        const auto colour = readColourSource(object->value(name), path + u'.' + name, *target);
        if(!colour) {
            return std::unexpected{colour.error()};
        }
        *target = *colour;
    }

    return resolved;
}

std::expected<TabAppearance::Accent, QString> readAccent(const QJsonValue& value, const QString& path,
                                                         const TabAppearance::Accent& defaults)
{
    auto resolved{defaults};
    const auto object
        = readObject(value, path, {"width"_L1, "margin"_L1, "inset"_L1, "rounded"_L1, "colour"_L1, "edge"_L1});
    if(!object) {
        return std::unexpected{object.error()};
    }

    for(const auto& [name, target, minimum, maximum] :
        std::array{std::tuple{"width"_L1, &resolved.width, 0, 32}, std::tuple{"margin"_L1, &resolved.margin, 0, 128},
                   std::tuple{"inset"_L1, &resolved.inset, 0, 128}}) {
        const auto field = readNumber(*object, name, path, *target, minimum, maximum);
        if(!field) {
            return std::unexpected{field.error()};
        }
        *target = *field;
    }

    const auto rounded = readBool(*object, "rounded"_L1, path, resolved.rounded);
    if(!rounded) {
        return std::unexpected{rounded.error()};
    }
    resolved.rounded = *rounded;

    const std::array colours{std::pair{"colour"_L1, &resolved.colour}};
    for(const auto& [name, target] : colours) {
        const auto colour = readColourSource(object->value(name), path + u'.' + name, *target);
        if(!colour) {
            return std::unexpected{colour.error()};
        }
        *target = *colour;
    }

    const auto edge = object->value("edge"_L1);
    if(!edge.isUndefined()) {
        const auto name = edge.toString();
        if(!edge.isString() || (name != "page"_L1 && name != "outer"_L1)) {
            return std::unexpected{
                errorWithPath(QCoreApplication::translate("ThemeAppearance",
                                                          "Choose the page edge or outer edge for the tab accent."),
                              path + u".edge"_s)};
        }
        resolved.edge = name == "outer"_L1 ? TabAppearance::Accent::Edge::Outer : TabAppearance::Accent::Edge::Page;
    }

    return resolved;
}

std::expected<TabAppearance::Focus, QString> readFocus(const QJsonValue& value, const QString& path,
                                                       const TabAppearance::Focus& defaults)
{
    auto resolved{defaults};
    const auto object = readObject(
        value, path, {"borderWidth"_L1, "inset"_L1, "radius"_L1, "pageGap"_L1, "keyboardOnly"_L1, "borderColour"_L1});
    if(!object) {
        return std::unexpected{object.error()};
    }

    for(const auto& [name, target, minimum, maximum] : std::array{
            std::tuple{"borderWidth"_L1, &resolved.borderWidth, 0, 32}, std::tuple{"inset"_L1, &resolved.inset, 0, 128},
            std::tuple{"radius"_L1, &resolved.radius, 0, 128}, std::tuple{"pageGap"_L1, &resolved.pageGap, 0, 128}}) {
        const auto field = readNumber(*object, name, path, *target, minimum, maximum);
        if(!field) {
            return std::unexpected{field.error()};
        }
        *target = *field;
    }

    const auto keyboardOnly = readBool(*object, "keyboardOnly"_L1, path, resolved.keyboardOnly);
    if(!keyboardOnly) {
        return std::unexpected{keyboardOnly.error()};
    }
    resolved.keyboardOnly = *keyboardOnly;

    const std::array colours{std::pair{"borderColour"_L1, &resolved.borderColour}};
    for(const auto& [name, target] : colours) {
        const auto colour = readColourSource(object->value(name), path + u'.' + name, *target);
        if(!colour) {
            return std::unexpected{colour.error()};
        }
        *target = *colour;
    }

    return resolved;
}

std::expected<TabAppearance::CloseState, QString> readCloseState(const QJsonValue& value, const QString& path,
                                                                 const TabAppearance::CloseState& defaults)
{
    auto resolved{defaults};
    const auto object = readObject(value, path, {"backgroundOpacity"_L1, "backgroundColour"_L1, "foregroundColour"_L1});
    if(!object) {
        return std::unexpected{object.error()};
    }

    const auto backgroundOpacity
        = readNumber(*object, "backgroundOpacity"_L1, path, resolved.backgroundOpacity, 0, 100);
    if(!backgroundOpacity) {
        return std::unexpected{backgroundOpacity.error()};
    }
    resolved.backgroundOpacity = *backgroundOpacity;

    const auto backgroundColour = readBrushSource(object->value("backgroundColour"_L1), path + u".backgroundColour"_s,
                                                  resolved.backgroundColour);
    if(!backgroundColour) {
        return std::unexpected{backgroundColour.error()};
    }
    resolved.backgroundColour = *backgroundColour;

    const std::array colours{std::pair{"foregroundColour"_L1, &resolved.foregroundColour}};
    for(const auto& [name, target] : colours) {
        const auto colour = readColourSource(object->value(name), path + u'.' + name, *target);
        if(!colour) {
            return std::unexpected{colour.error()};
        }
        *target = *colour;
    }

    return resolved;
}

std::expected<TabAppearance::Close, QString> readClose(const QJsonValue& value, const QString& path,
                                                       const TabAppearance::Close& defaults)
{
    auto resolved{defaults};
    const auto object = readObject(value, path,
                                   {"extent"_L1, "inset"_L1, "radius"_L1, "glyphSize"_L1, "glyphWidth"_L1, "normal"_L1,
                                    "hovered"_L1, "pressed"_L1, "disabled"_L1});
    if(!object) {
        return std::unexpected{object.error()};
    }

    const auto extent = readNumber(*object, "extent"_L1, path, resolved.extent, 1, 128);
    if(!extent) {
        return std::unexpected{extent.error()};
    }
    resolved.extent = *extent;

    for(const auto& [name, target, minimum, maximum] :
        std::array{std::tuple{"inset"_L1, &resolved.inset, 0, 64}, std::tuple{"radius"_L1, &resolved.radius, 0, 128},
                   std::tuple{"glyphSize"_L1, &resolved.glyphSize, 0, 64},
                   std::tuple{"glyphWidth"_L1, &resolved.glyphWidth, 0, 16}}) {
        const auto field = readNumber(*object, name, path, *target, minimum, maximum);
        if(!field) {
            return std::unexpected{field.error()};
        }
        *target = *field;
    }

    const std::array states{std::pair{"normal"_L1, &resolved.normal}, std::pair{"hovered"_L1, &resolved.hovered},
                            std::pair{"pressed"_L1, &resolved.pressed}, std::pair{"disabled"_L1, &resolved.disabled}};
    for(const auto& [name, state] : states) {
        const auto stateValue = readCloseState(stateOverrides(*object, name), path + u'.' + name, *state);
        if(!stateValue) {
            return std::unexpected{stateValue.error()};
        }
        *state = *stateValue;
    }

    return resolved;
}

std::expected<TabAppearance::Pane, QString> readPane(const QJsonValue& value, const QString& path,
                                                     const TabAppearance::Pane& defaults)
{
    auto resolved{defaults};
    const auto object
        = readObject(value, path,
                     {"borderWidth"_L1, "radius"_L1, "borderColour"_L1, "borderOpacity"_L1, "backgroundColour"_L1,
                      "backgroundOpacity"_L1, "reliefOpacity"_L1, "reliefStyle"_L1});
    if(!object) {
        return std::unexpected{object.error()};
    }

    for(const auto& [name, target, minimum, maximum] :
        std::array{std::tuple{"borderWidth"_L1, &resolved.borderWidth, 0, 32},
                   std::tuple{"radius"_L1, &resolved.radius, 0, 128}}) {
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

    const auto borderOpacity = readNumber(*object, "borderOpacity"_L1, path, resolved.borderOpacity, 0, 100);
    if(!borderOpacity) {
        return std::unexpected{borderOpacity.error()};
    }
    resolved.borderOpacity = *borderOpacity;

    const auto backgroundColour = readBrushSource(object->value("backgroundColour"_L1), path + u".backgroundColour"_s,
                                                  resolved.backgroundColour);
    if(!backgroundColour) {
        return std::unexpected{backgroundColour.error()};
    }
    resolved.backgroundColour = *backgroundColour;

    const auto reliefStyle = object->value("reliefStyle"_L1);
    if(!reliefStyle.isUndefined()) {
        const auto name = reliefStyle.toString();
        if(!reliefStyle.isString() || (name != "raised"_L1 && name != "sunken"_L1 && name != "classic"_L1)) {
            return std::unexpected{
                errorWithPath(QCoreApplication::translate("ThemeAppearance",
                                                          "Choose a raised, sunken or classic tab pane relief style."),
                              path + u".reliefStyle"_s)};
        }
        resolved.reliefStyle = name == "sunken"_L1  ? TabAppearance::Pane::ReliefStyle::Sunken
                             : name == "classic"_L1 ? TabAppearance::Pane::ReliefStyle::Classic
                                                    : TabAppearance::Pane::ReliefStyle::Raised;
    }

    for(const auto& [name, target] : std::array{std::pair{"backgroundOpacity"_L1, &resolved.backgroundOpacity},
                                                std::pair{"reliefOpacity"_L1, &resolved.reliefOpacity}}) {
        const auto field = readNumber(*object, name, path, *target, 0, 100);
        if(!field) {
            return std::unexpected{field.error()};
        }
        *target = *field;
    }

    return resolved;
}

std::expected<TabAppearance::Strip, QString> readStrip(const QJsonValue& value, const QString& path,
                                                       const TabAppearance::Strip& defaults)
{
    auto resolved{defaults};
    const auto object = readObject(value, path,
                                   {"backgroundColour"_L1, "backgroundOpacity"_L1, "radius"_L1, "leadingPadding"_L1,
                                    "trailingPadding"_L1, "outerPadding"_L1});
    if(!object) {
        return std::unexpected{object.error()};
    }

    const auto colour = readBrushSource(object->value("backgroundColour"_L1), path + u".backgroundColour"_s,
                                        resolved.backgroundColour);
    if(!colour) {
        return std::unexpected{colour.error()};
    }
    resolved.backgroundColour = *colour;

    const auto opacity = readNumber(*object, "backgroundOpacity"_L1, path, resolved.backgroundOpacity, 0, 100);
    if(!opacity) {
        return std::unexpected{opacity.error()};
    }
    resolved.backgroundOpacity = *opacity;

    const auto radius = readNumber(*object, "radius"_L1, path, resolved.radius, 0, 128);
    if(!radius) {
        return std::unexpected{radius.error()};
    }
    resolved.radius = *radius;

    for(const auto& [name, target] : std::array{std::pair{"leadingPadding"_L1, &resolved.leadingPadding},
                                                std::pair{"trailingPadding"_L1, &resolved.trailingPadding},
                                                std::pair{"outerPadding"_L1, &resolved.outerPadding}}) {
        const auto field = readNumber(*object, name, path, *target, 0, 128);
        if(!field) {
            return std::unexpected{field.error()};
        }
        *target = *field;
    }

    return resolved;
}
} // namespace

std::expected<TabAppearance, QString> resolveTabAppearance(const QJsonObject& appearance)
{
    TabAppearance resolved;
    const QString path = u"appearance.tab"_s;
    const auto object = readObject(appearance.value("tab"_L1), path,
                                   {"design"_L1, "alignment"_L1, "horizontalPadding"_L1, "verticalPadding"_L1,
                                    "minimumThickness"_L1, "inset"_L1, "radius"_L1, "normal"_L1, "hovered"_L1,
                                    "pressed"_L1, "selected"_L1, "selectedHovered"_L1, "disabled"_L1,
                                    "selectedDisabled"_L1, "accent"_L1, "focus"_L1, "close"_L1, "pane"_L1, "strip"_L1});
    if(!object) {
        return std::unexpected{object.error()};
    }

    const auto design = object->value("design"_L1);
    if(!design.isUndefined()) {
        const auto name = design.toString();
        if(!design.isString() || (name != "flat"_L1 && name != "connected"_L1)) {
            return std::unexpected{
                errorWithPath(QCoreApplication::translate("ThemeAppearance", "Choose a flat or connected tab design."),
                              path + u".design"_s)};
        }
        resolved.design = name == "connected"_L1 ? TabAppearance::Design::Connected : TabAppearance::Design::Flat;
    }

    const auto alignment = object->value("alignment"_L1);
    if(!alignment.isUndefined()) {
        const auto name = alignment.toString();
        if(!alignment.isString() || (name != "leading"_L1 && name != "center"_L1 && name != "trailing"_L1)) {
            return std::unexpected{errorWithPath(
                QCoreApplication::translate("ThemeAppearance", "Choose leading, centred or trailing tab alignment."),
                path + u".alignment"_s)};
        }
        resolved.alignment = name == "center"_L1   ? TabAppearance::Alignment::Centre
                           : name == "trailing"_L1 ? TabAppearance::Alignment::Trailing
                                                   : TabAppearance::Alignment::Leading;
    }

    for(const auto& [name, target, minimum, maximum] :
        std::array{std::tuple{"horizontalPadding"_L1, &resolved.horizontalPadding, 0, 128},
                   std::tuple{"verticalPadding"_L1, &resolved.verticalPadding, 0, 128},
                   std::tuple{"minimumThickness"_L1, &resolved.minimumThickness, 0, 512}}) {
        const auto field = readNumber(*object, name, path, *target, minimum, maximum);
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

    const std::array states{std::pair{"normal"_L1, &resolved.normal},
                            std::pair{"hovered"_L1, &resolved.hovered},
                            std::pair{"pressed"_L1, &resolved.pressed},
                            std::pair{"selected"_L1, &resolved.selected},
                            std::pair{"selectedHovered"_L1, &resolved.selectedHovered},
                            std::pair{"disabled"_L1, &resolved.disabled},
                            std::pair{"selectedDisabled"_L1, &resolved.selectedDisabled}};
    auto surfaces = *object;
    surfaces.insert("selected"_L1, stateOverrides(*object, "selected"_L1));
    for(const auto& [name, state] : states) {
        const auto base  = name == "selectedHovered"_L1 || name == "selectedDisabled"_L1 ? "selected"_L1 : "normal"_L1;
        const auto value = readSurface(stateOverrides(surfaces, name, base), path + u'.' + name, *state);
        if(!value) {
            return std::unexpected{value.error()};
        }
        *state = *value;
    }

    const auto accent = readAccent(object->value("accent"_L1), path + u".accent"_s, resolved.accent);
    if(!accent) {
        return std::unexpected{accent.error()};
    }
    resolved.accent = *accent;

    const auto focus = readFocus(object->value("focus"_L1), path + u".focus"_s, resolved.focus);
    if(!focus) {
        return std::unexpected{focus.error()};
    }
    resolved.focus = *focus;

    const auto close = readClose(object->value("close"_L1), path + u".close"_s, resolved.close);
    if(!close) {
        return std::unexpected{close.error()};
    }
    resolved.close = *close;

    const auto pane = readPane(object->value("pane"_L1), path + u".pane"_s, resolved.pane);
    if(!pane) {
        return std::unexpected{pane.error()};
    }
    resolved.pane = *pane;

    const auto strip = readStrip(object->value("strip"_L1), path + u".strip"_s, resolved.strip);
    if(!strip) {
        return std::unexpected{strip.error()};
    }
    resolved.strip = *strip;

    return resolved;
}
} // namespace Fooyin
