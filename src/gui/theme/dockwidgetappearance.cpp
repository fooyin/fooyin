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

#include "dockwidgetappearance.h"

#include "appearanceutils.h"

#include <array>
#include <tuple>

using namespace Qt::StringLiterals;

namespace Fooyin {
using namespace Appearance;
namespace {
std::expected<DockWidgetAppearance::Border, QString> readBorder(const QJsonValue& value, const QString& path,
                                                                const DockWidgetAppearance::Border& defaults)
{
    auto resolved{defaults};
    const auto object
        = readObject(value, path, {"width"_L1, "colour"_L1, "tintColour"_L1, "tintOpacity"_L1, "opacity"_L1});
    if(!object) {
        return std::unexpected{object.error()};
    }

    for(const auto& [name, target, maximum] : std::array{std::tuple{"width"_L1, &resolved.width, 32},
                                                         std::tuple{"tintOpacity"_L1, &resolved.tintOpacity, 100},
                                                         std::tuple{"opacity"_L1, &resolved.opacity, 100}}) {
        const auto number = readNumber(*object, name, path, *target, 0, maximum);
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

std::expected<DockWidgetAppearance::Title, QString> readTitle(const QJsonValue& value, const QString& path,
                                                              const DockWidgetAppearance::Title& defaults)
{
    auto resolved{defaults};
    const auto object = readObject(
        value, path,
        {"backgroundColour"_L1, "backgroundOpacity"_L1, "foregroundColour"_L1, "iconColour"_L1, "border"_L1});
    if(!object) {
        return std::unexpected{object.error()};
    }

    const auto background = readBrushSource(object->value("backgroundColour"_L1), path + u".backgroundColour"_s,
                                            resolved.backgroundColour);
    if(!background) {
        return std::unexpected{background.error()};
    }
    resolved.backgroundColour = *background;

    const auto opacity = readNumber(*object, "backgroundOpacity"_L1, path, resolved.backgroundOpacity, 0, 100);
    if(!opacity) {
        return std::unexpected{opacity.error()};
    }
    resolved.backgroundOpacity = *opacity;

    for(const auto& [name, target] : std::array{std::pair{"foregroundColour"_L1, &resolved.foregroundColour},
                                                std::pair{"iconColour"_L1, &resolved.iconColour}}) {
        const auto colour = readColourSource(object->value(name), path + u'.' + name, *target);
        if(!colour) {
            return std::unexpected{colour.error()};
        }
        *target = *colour;
    }

    const auto border = readBorder(object->value("border"_L1), path + u".border"_s, resolved.border);
    if(!border) {
        return std::unexpected{border.error()};
    }
    resolved.border = *border;

    return resolved;
}
} // namespace

std::expected<DockWidgetAppearance, QString> resolveDockWidgetAppearance(const QJsonObject& appearance)
{
    DockWidgetAppearance resolved;
    const QString path = u"appearance.dockWidget"_s;
    const auto object = readObject(appearance.value("dockWidget"_L1), path,
                                   {"titleMargin"_L1, "buttonMargin"_L1, "separatorExtent"_L1, "title"_L1, "frame"_L1});
    if(!object) {
        return std::unexpected{object.error()};
    }

    for(const auto& [name, target] : std::array{std::pair{"titleMargin"_L1, &resolved.titleMargin},
                                                std::pair{"buttonMargin"_L1, &resolved.buttonMargin},
                                                std::pair{"separatorExtent"_L1, &resolved.separatorExtent}}) {
        const auto number = readNumber(*object, name, path, *target, 0, 128);
        if(!number) {
            return std::unexpected{number.error()};
        }
        *target = *number;
    }

    const auto title = readTitle(object->value("title"_L1), path + u".title"_s, resolved.title);
    if(!title) {
        return std::unexpected{title.error()};
    }
    resolved.title = *title;

    const auto frame = readBorder(object->value("frame"_L1), path + u".frame"_s, resolved.frame);
    if(!frame) {
        return std::unexpected{frame.error()};
    }
    resolved.frame = *frame;

    return resolved;
}
} // namespace Fooyin
