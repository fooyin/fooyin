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

#include "toolbarappearance.h"

#include "appearanceutils.h"

#include <array>
#include <tuple>

using namespace Qt::StringLiterals;

namespace Fooyin {
using namespace Appearance;
namespace {
std::expected<ToolBarAppearance::Border, QString> readBorder(const QJsonValue& value, const QString& path,
                                                             const ToolBarAppearance::Border& defaults)
{
    auto resolved{defaults};
    const auto object = readObject(value, path,
                                   {"width"_L1, "colour"_L1, "tintColour"_L1, "tintOpacity"_L1, "opacity"_L1,
                                    "floatingOpacity"_L1, "floatingRadius"_L1});
    if(!object) {
        return std::unexpected{object.error()};
    }

    for(const auto& [name, target, minimum, maximum] : std::array{
            std::tuple{"width"_L1, &resolved.width, 0, 32}, std::tuple{"tintOpacity"_L1, &resolved.tintOpacity, 0, 100},
            std::tuple{"opacity"_L1, &resolved.opacity, 0, 100},
            std::tuple{"floatingOpacity"_L1, &resolved.floatingOpacity, 0, 100}}) {
        const auto number = readNumber(*object, name, path, *target, minimum, maximum);
        if(!number) {
            return std::unexpected{number.error()};
        }
        *target = *number;
    }

    const auto radius = readNumber(*object, "floatingRadius"_L1, path, resolved.floatingRadius, 0, 128);
    if(!radius) {
        return std::unexpected{radius.error()};
    }
    resolved.floatingRadius = *radius;

    for(const auto& [name, target] :
        std::array{std::pair{"colour"_L1, &resolved.colour}, std::pair{"tintColour"_L1, &resolved.tintColour}}) {
        const auto colour = readColourSource(object->value(name), path + u'.' + name, *target);
        if(!colour) {
            return std::unexpected{colour.error()};
        }
        *target = *colour;
    }

    return resolved;
}

std::expected<ToolBarAppearance::Handle, QString> readHandle(const QJsonValue& value, const QString& path,
                                                             const ToolBarAppearance::Handle& defaults)
{
    auto resolved{defaults};
    const auto object = readObject(
        value, path, {"extent"_L1, "dotRadius"_L1, "columnSpacing"_L1, "rowSpacing"_L1, "colour"_L1, "opacity"_L1});
    if(!object) {
        return std::unexpected{object.error()};
    }

    for(const auto& [name, target, minimum, maximum] : std::array{
            std::tuple{"extent"_L1, &resolved.extent, 0, 128}, std::tuple{"opacity"_L1, &resolved.opacity, 0, 100}}) {
        const auto number = readNumber(*object, name, path, *target, minimum, maximum);
        if(!number) {
            return std::unexpected{number.error()};
        }
        *target = *number;
    }

    for(const auto& [name, target, minimum, maximum] :
        std::array{std::tuple{"dotRadius"_L1, &resolved.dotRadius, 0, 32},
                   std::tuple{"columnSpacing"_L1, &resolved.columnSpacing, 0, 128},
                   std::tuple{"rowSpacing"_L1, &resolved.rowSpacing, 0, 128}}) {
        const auto number = readNumber(*object, name, path, *target, minimum, maximum);
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

    return resolved;
}

std::expected<ToolBarAppearance::Separator, QString> readSeparator(const QJsonValue& value, const QString& path,
                                                                   const ToolBarAppearance::Separator& defaults)
{
    auto resolved{defaults};
    const auto object = readObject(value, path,
                                   {"extent"_L1, "length"_L1, "margin"_L1, "width"_L1, "colour"_L1, "tintColour"_L1,
                                    "tintOpacity"_L1, "opacity"_L1});
    if(!object) {
        return std::unexpected{object.error()};
    }

    for(const auto& [name, target, minimum, maximum] :
        std::array{std::tuple{"extent"_L1, &resolved.extent, 0, 128},
                   std::tuple{"tintOpacity"_L1, &resolved.tintOpacity, 0, 100},
                   std::tuple{"opacity"_L1, &resolved.opacity, 0, 100}}) {
        const auto number = readNumber(*object, name, path, *target, minimum, maximum);
        if(!number) {
            return std::unexpected{number.error()};
        }
        *target = *number;
    }

    for(const auto& [name, target, minimum, maximum] :
        std::array{std::tuple{"length"_L1, &resolved.length, 0, 128}, std::tuple{"margin"_L1, &resolved.margin, 0, 128},
                   std::tuple{"width"_L1, &resolved.width, 0, 32}}) {
        const auto number = readNumber(*object, name, path, *target, minimum, maximum);
        if(!number) {
            return std::unexpected{number.error()};
        }
        *target = *number;
    }

    for(const auto& [name, target] :
        std::array{std::pair{"colour"_L1, &resolved.colour}, std::pair{"tintColour"_L1, &resolved.tintColour}}) {
        const auto colour = readColourSource(object->value(name), path + u'.' + name, *target);
        if(!colour) {
            return std::unexpected{colour.error()};
        }
        *target = *colour;
    }

    return resolved;
}

std::expected<ToolBarAppearance::Extension, QString> readExtension(const QJsonValue& value, const QString& path,
                                                                   const ToolBarAppearance::Extension& defaults)
{
    auto resolved{defaults};
    const auto object = readObject(value, path, {"extent"_L1, "glyphSize"_L1, "glyphWidth"_L1, "colour"_L1});
    if(!object) {
        return std::unexpected{object.error()};
    }

    const auto extent = readNumber(*object, "extent"_L1, path, resolved.extent, 1, 128);
    if(!extent) {
        return std::unexpected{extent.error()};
    }
    resolved.extent = *extent;

    for(const auto& [name, target, minimum, maximum] :
        std::array{std::tuple{"glyphSize"_L1, &resolved.glyphSize, 0, 128},
                   std::tuple{"glyphWidth"_L1, &resolved.glyphWidth, 0, 32}}) {
        const auto number = readNumber(*object, name, path, *target, minimum, maximum);
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

    return resolved;
}
} // namespace

std::expected<ToolBarAppearance, QString> resolveToolBarAppearance(const QJsonObject& appearance)
{
    ToolBarAppearance resolved;
    const QString path = u"appearance.toolBar"_s;
    const auto object  = readObject(appearance.value("toolBar"_L1), path,
                                    {"itemMargin"_L1, "itemSpacing"_L1, "backgroundColour"_L1, "backgroundOpacity"_L1,
                                     "border"_L1, "handle"_L1, "separator"_L1, "extension"_L1});
    if(!object) {
        return std::unexpected{object.error()};
    }

    for(const auto& [name, target, maximum] :
        std::array{std::tuple{"itemMargin"_L1, &resolved.itemMargin, 128},
                   std::tuple{"itemSpacing"_L1, &resolved.itemSpacing, 128},
                   std::tuple{"backgroundOpacity"_L1, &resolved.backgroundOpacity, 100}}) {
        const auto number = readNumber(*object, name, path, *target, 0, maximum);
        if(!number) {
            return std::unexpected{number.error()};
        }
        *target = *number;
    }

    const auto background = readBrushSource(object->value("backgroundColour"_L1), path + u".backgroundColour"_s,
                                            resolved.backgroundColour);
    if(!background) {
        return std::unexpected{background.error()};
    }
    resolved.backgroundColour = *background;

    const auto border = readBorder(object->value("border"_L1), path + u".border"_s, resolved.border);
    if(!border) {
        return std::unexpected{border.error()};
    }
    resolved.border = *border;

    const auto handle = readHandle(object->value("handle"_L1), path + u".handle"_s, resolved.handle);
    if(!handle) {
        return std::unexpected{handle.error()};
    }
    resolved.handle = *handle;

    const auto separator = readSeparator(object->value("separator"_L1), path + u".separator"_s, resolved.separator);
    if(!separator) {
        return std::unexpected{separator.error()};
    }
    resolved.separator = *separator;

    const auto extension = readExtension(object->value("extension"_L1), path + u".extension"_s, resolved.extension);
    if(!extension) {
        return std::unexpected{extension.error()};
    }
    resolved.extension = *extension;

    return resolved;
}
} // namespace Fooyin
