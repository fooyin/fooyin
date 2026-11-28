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

#include "scrollbarappearance.h"

#include "appearanceutils.h"

#include <QCoreApplication>

#include <array>
#include <tuple>

using namespace Qt::StringLiterals;

namespace Fooyin {
using namespace Appearance;
namespace {
std::expected<ScrollBarAppearance::Thumb, QString> readThumb(const QJsonValue& value, const QString& path,
                                                             const ScrollBarAppearance& appearance,
                                                             const ScrollBarAppearance::Thumb& defaults)
{
    auto resolved{defaults};
    const auto object = readObject(value, path,
                                   {"sideInset"_L1, "endInset"_L1, "cornerRadius"_L1, "opacity"_L1, "borderWidth"_L1,
                                    "backgroundColour"_L1, "borderColour"_L1});
    if(!object) {
        return std::unexpected{object.error()};
    }

    for(const auto& [name, target] :
        std::array{std::pair{"sideInset"_L1, &resolved.sideInset}, std::pair{"endInset"_L1, &resolved.endInset}}) {
        const auto field = readNumber(*object, name, path, *target, 0, 128);
        if(!field) {
            return std::unexpected{field.error()};
        }
        *target = *field;
    }

    for(const auto& [name, target] :
        std::array{std::pair{"cornerRadius"_L1, &resolved.cornerRadius}, std::pair{"opacity"_L1, &resolved.opacity}}) {
        const auto field = readNumber(*object, name, path, *target, 0, 100);
        if(!field) {
            return std::unexpected{field.error()};
        }
        *target = *field;
    }

    const auto borderWidth = readNumber(*object, "borderWidth"_L1, path, resolved.borderWidth, 0, 32);
    if(!borderWidth) {
        return std::unexpected{borderWidth.error()};
    }
    resolved.borderWidth = *borderWidth;

    for(const auto& [name, target] : std::array{std::pair{"backgroundColour"_L1, &resolved.backgroundColour},
                                                std::pair{"borderColour"_L1, &resolved.borderColour}}) {
        const auto field = readColourSource(object->value(name), path + u'.' + name, *target);
        if(!field) {
            return std::unexpected{field.error()};
        }
        *target = *field;
    }

    if(resolved.sideInset * 2 >= appearance.width) {
        return std::unexpected{
            errorWithPath(QCoreApplication::translate(
                              "ThemeAppearance", "The thumb side inset must be less than half the scroll bar width."),
                          path + u".sideInset"_s)};
    }
    if(resolved.endInset * 2 >= appearance.minimumThumbLength) {
        return std::unexpected{errorWithPath(
            QCoreApplication::translate("ThemeAppearance",
                                        "The thumb end inset must be less than half the minimum thumb length."),
            path + u".endInset"_s)};
    }
    if(resolved.borderWidth > std::min(resolved.sideInset, resolved.endInset) * 2) {
        return std::unexpected{
            errorWithPath(QCoreApplication::translate(
                              "ThemeAppearance",
                              "The thumb border width must not exceed twice the smaller of the side and end insets."),
                          path + u".borderWidth"_s)};
    }

    return resolved;
}

std::expected<ScrollBarAppearance::TrackState, QString> readTrack(const QJsonValue& value, const QString& path,
                                                                  const ScrollBarAppearance::TrackState& defaults)
{
    auto resolved{defaults};
    const auto object = readObject(value, path, {"visible"_L1, "backgroundColour"_L1});
    if(!object) {
        return std::unexpected{object.error()};
    }

    const auto visible = readBool(*object, "visible"_L1, path, resolved.visible);
    if(!visible) {
        return std::unexpected{visible.error()};
    }
    resolved.visible = *visible;

    const auto backgroundColour = readColourSource(object->value("backgroundColour"_L1), path + u".backgroundColour"_s,
                                                   resolved.backgroundColour);
    if(!backgroundColour) {
        return std::unexpected{backgroundColour.error()};
    }
    resolved.backgroundColour = *backgroundColour;

    return resolved;
}

std::expected<ScrollBarAppearance::Focus, QString> readFocus(const QJsonValue& value, const QString& path, int width,
                                                             const ScrollBarAppearance::Focus& defaults)
{
    auto resolved{defaults};
    const auto object = readObject(value, path, {"borderWidth"_L1, "radius"_L1, "borderColour"_L1});
    if(!object) {
        return std::unexpected{object.error()};
    }

    for(const auto& [name, target, minimum, maximum] :
        std::array{std::tuple{"borderWidth"_L1, &resolved.borderWidth, 0, std::min(32, width)},
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

    return resolved;
}
std::expected<ScrollBarAppearance::AutoHide, QString> readAutoHide(const QJsonValue& value, const QString& path,
                                                                   const ScrollBarAppearance::AutoHide& defaults)
{
    auto resolved{defaults};
    const auto object = readObject(value, path, {"enabled"_L1, "hideDelay"_L1, "fadeDuration"_L1});
    if(!object) {
        return std::unexpected{object.error()};
    }

    const auto enabled = readBool(*object, "enabled"_L1, path, resolved.enabled);
    if(!enabled) {
        return std::unexpected{enabled.error()};
    }
    resolved.enabled = *enabled;

    for(const auto& [name, target, minimum, maximum] :
        std::array{std::tuple{"hideDelay"_L1, &resolved.hideDelay, 0, 60000},
                   std::tuple{"fadeDuration"_L1, &resolved.fadeDuration, 0, 10000}}) {
        const auto field = readNumber(*object, name, path, *target, minimum, maximum);
        if(!field) {
            return std::unexpected{field.error()};
        }
        *target = *field;
    }

    return resolved;
}
} // namespace

std::expected<ScrollBarAppearance, QString> resolveScrollBarAppearance(const QJsonObject& appearance)
{
    ScrollBarAppearance resolved;
    const QString path = u"appearance.scrollBar"_s;
    const auto object  = readObject(appearance.value("scrollBar"_L1), path,
                                    {"width"_L1, "minimumThumbLength"_L1, "track"_L1, "normal"_L1, "hovered"_L1,
                                     "pressed"_L1, "disabled"_L1, "focus"_L1, "autoHide"_L1});
    if(!object) {
        return std::unexpected{object.error()};
    }

    for(const auto& [name, target, minimum, maximum] :
        std::array{std::tuple{"width"_L1, &resolved.width, 1, 256},
                   std::tuple{"minimumThumbLength"_L1, &resolved.minimumThumbLength, 1, 512}}) {
        const auto field = readNumber(*object, name, path, *target, minimum, maximum);
        if(!field) {
            return std::unexpected{field.error()};
        }
        *target = *field;
    }

    const auto track = readObject(object->value("track"_L1), path + u".track"_s,
                                  {"normal"_L1, "hovered"_L1, "pressed"_L1, "disabled"_L1});
    if(!track) {
        return std::unexpected{track.error()};
    }

    for(const auto& [name, target] : std::array{
            std::pair{"normal"_L1, &resolved.track.normal}, std::pair{"hovered"_L1, &resolved.track.hovered},
            std::pair{"pressed"_L1, &resolved.track.pressed}, std::pair{"disabled"_L1, &resolved.track.disabled}}) {
        const auto state = readTrack(stateOverrides(*track, name), path + u".track."_s + name, *target);
        if(!state) {
            return std::unexpected{state.error()};
        }
        *target = *state;
    }

    for(const auto& [name, target] :
        std::array{std::pair{"normal"_L1, &resolved.normal}, std::pair{"hovered"_L1, &resolved.hovered},
                   std::pair{"pressed"_L1, &resolved.pressed}, std::pair{"disabled"_L1, &resolved.disabled}}) {
        const auto state = readThumb(stateOverrides(*object, name), path + u'.' + name, resolved, *target);
        if(!state) {
            return std::unexpected{state.error()};
        }
        *target = *state;
    }

    const auto focus = readFocus(object->value("focus"_L1), path + u".focus"_s, resolved.width, resolved.focus);
    if(!focus) {
        return std::unexpected{focus.error()};
    }
    resolved.focus = *focus;

    const auto autoHide = readAutoHide(object->value("autoHide"_L1), path + u".autoHide"_s, resolved.autoHide);
    if(!autoHide) {
        return std::unexpected{autoHide.error()};
    }
    resolved.autoHide = *autoHide;

    return resolved;
}
} // namespace Fooyin
