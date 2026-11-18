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

#include "splitterappearance.h"

#include "appearanceutils.h"

#include <array>

using namespace Qt::StringLiterals;

namespace Fooyin {
using namespace Appearance;
namespace {
std::expected<SplitterAppearance::State, QString> readState(const QJsonValue& value, const QString& path,
                                                            const SplitterAppearance::State& defaults)
{
    auto resolved{defaults};
    const auto object
        = readObject(value, path, {"backgroundColour"_L1, "backgroundOpacity"_L1, "gripColour"_L1, "gripOpacity"_L1});
    if(!object) {
        return std::unexpected{object.error()};
    }

    for(const auto& [name, target] : std::array{std::pair{"backgroundColour"_L1, &resolved.backgroundColour},
                                                std::pair{"gripColour"_L1, &resolved.gripColour}}) {
        const auto colour = readColourSource(object->value(name), path + u'.' + name, *target);
        if(!colour) {
            return std::unexpected{colour.error()};
        }
        *target = *colour;
    }

    for(const auto& [name, target] : std::array{std::pair{"backgroundOpacity"_L1, &resolved.backgroundOpacity},
                                                std::pair{"gripOpacity"_L1, &resolved.gripOpacity}}) {
        const auto opacity = readNumber(*object, name, path, *target, 0, 100);
        if(!opacity) {
            return std::unexpected{opacity.error()};
        }
        *target = *opacity;
    }
    return resolved;
}

std::expected<SplitterAppearance::Grip, QString> readGrip(const QJsonValue& value, const QString& path,
                                                          const SplitterAppearance::Grip& defaults)
{
    auto resolved{defaults};
    const auto object = readObject(value, path, {"length"_L1, "thickness"_L1, "margin"_L1, "cornerRadius"_L1});
    if(!object) {
        return std::unexpected{object.error()};
    }

    for(const auto& [name, target] :
        std::array{std::pair{"length"_L1, &resolved.length}, std::pair{"thickness"_L1, &resolved.thickness},
                   std::pair{"margin"_L1, &resolved.margin}}) {
        const auto number = readNumber(*object, name, path, *target, 0, 128);
        if(!number) {
            return std::unexpected{number.error()};
        }
        *target = *number;
    }

    const auto radius = readNumber(*object, "cornerRadius"_L1, path, resolved.cornerRadius, 0, 100);
    if(!radius) {
        return std::unexpected{radius.error()};
    }
    resolved.cornerRadius = *radius;

    return resolved;
}
} // namespace

std::expected<SplitterAppearance, QString> resolveSplitterAppearance(const QJsonObject& appearance)
{
    SplitterAppearance resolved;
    const QString path = u"appearance.splitter"_s;
    const auto object  = readObject(
        appearance.value("splitter"_L1), path,
        {"width"_L1, "backgroundRadius"_L1, "grip"_L1, "normal"_L1, "hovered"_L1, "pressed"_L1, "disabled"_L1});
    if(!object) {
        return std::unexpected{object.error()};
    }
    const auto width = readNumber(*object, "width"_L1, path, resolved.width, 1, 128);
    if(!width) {
        return std::unexpected{width.error()};
    }
    resolved.width = *width;

    const auto radius = readNumber(*object, "backgroundRadius"_L1, path, resolved.backgroundRadius, 0, 128);
    if(!radius) {
        return std::unexpected{radius.error()};
    }
    resolved.backgroundRadius = *radius;

    const auto grip = readGrip(object->value("grip"_L1), path + u".grip"_s, resolved.grip);
    if(!grip) {
        return std::unexpected{grip.error()};
    }
    resolved.grip = *grip;

    for(const auto& [name, target] :
        std::array{std::pair{"normal"_L1, &resolved.normal}, std::pair{"hovered"_L1, &resolved.hovered},
                   std::pair{"pressed"_L1, &resolved.pressed}, std::pair{"disabled"_L1, &resolved.disabled}}) {
        const auto state = readState(stateOverrides(*object, name), path + u'.' + name, *target);
        if(!state) {
            return std::unexpected{state.error()};
        }
        *target = *state;
    }

    return resolved;
}
} // namespace Fooyin
