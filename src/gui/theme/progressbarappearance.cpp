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

#include "progressbarappearance.h"

#include "appearanceutils.h"

#include <QCoreApplication>

#include <array>
#include <tuple>

using namespace Qt::StringLiterals;

namespace Fooyin {
using namespace Appearance;
namespace {
std::expected<ProgressBarAppearance::Track, QString> readTrack(const QJsonValue& value, const QString& path,
                                                               const ProgressBarAppearance::Track& defaults)
{
    auto resolved{defaults};
    const auto object = readObject(value, path,
                                   {"inset"_L1, "radius"_L1, "borderWidth"_L1, "compactThickness"_L1,
                                    "compactRadius"_L1, "compactBorderWidth"_L1});
    if(!object) {
        return std::unexpected{object.error()};
    }

    for(const auto& [name, target, minimum, maximum] :
        std::array{std::tuple{"inset"_L1, &resolved.inset, 0, 128}, std::tuple{"radius"_L1, &resolved.radius, 0, 128},
                   std::tuple{"borderWidth"_L1, &resolved.borderWidth, 0, 32}}) {
        const auto field = readNumber(*object, name, path, *target, minimum, maximum);
        if(!field) {
            return std::unexpected{field.error()};
        }
        *target = *field;
    }

    const auto compactThickness = readNumber(*object, "compactThickness"_L1, path, resolved.compactThickness, 1, 256);
    if(!compactThickness) {
        return std::unexpected{compactThickness.error()};
    }
    resolved.compactThickness = *compactThickness;

    for(const auto& [name, target, minimum, maximum] :
        std::array{std::tuple{"compactRadius"_L1, &resolved.compactRadius, 0, 128},
                   std::tuple{"compactBorderWidth"_L1, &resolved.compactBorderWidth, 0, 32}}) {
        const auto field = readNumber(*object, name, path, *target, minimum, maximum);
        if(!field) {
            return std::unexpected{field.error()};
        }
        *target = *field;
    }

    return resolved;
}

std::expected<ProgressBarAppearance::Fill, QString> readFill(const QJsonValue& value, const QString& path,
                                                             const ProgressBarAppearance::Fill& defaults)
{
    auto resolved{defaults};
    const auto object = readObject(
        value, path,
        {"inset"_L1, "radius"_L1, "compactRadius"_L1, "segmented"_L1, "segmentLength"_L1, "segmentSpacing"_L1});
    if(!object) {
        return std::unexpected{object.error()};
    }

    for(const auto& [name, target] :
        std::array{std::pair{"inset"_L1, &resolved.inset}, std::pair{"radius"_L1, &resolved.radius},
                   std::pair{"compactRadius"_L1, &resolved.compactRadius}}) {
        const auto field = readNumber(*object, name, path, *target, 0, 128);
        if(!field) {
            return std::unexpected{field.error()};
        }
        *target = *field;
    }

    const auto segmented = readBool(*object, "segmented"_L1, path, resolved.segmented);
    if(!segmented) {
        return std::unexpected{segmented.error()};
    }
    resolved.segmented = *segmented;

    for(const auto& [name, target, minimum, maximum] :
        std::array{std::tuple{"segmentLength"_L1, &resolved.segmentLength, 1, 512},
                   std::tuple{"segmentSpacing"_L1, &resolved.segmentSpacing, 0, 128}}) {
        const auto field = readNumber(*object, name, path, *target, minimum, maximum);
        if(!field) {
            return std::unexpected{field.error()};
        }
        *target = *field;
    }

    return resolved;
}

std::expected<ProgressBarAppearance::Text, QString> readText(const QJsonValue& value, const QString& path,
                                                             const ProgressBarAppearance::Text& defaults)
{
    auto resolved{defaults};
    const auto object
        = readObject(value, path, {"padding"_L1, "visible"_L1, "alignment"_L1, "position"_L1, "spacing"_L1});
    if(!object) {
        return std::unexpected{object.error()};
    }

    for(const auto& [name, target] :
        std::array{std::pair{"padding"_L1, &resolved.padding}, std::pair{"spacing"_L1, &resolved.spacing}}) {
        const auto field = readNumber(*object, name, path, *target, 0, 128);
        if(!field) {
            return std::unexpected{field.error()};
        }
        *target = *field;
    }

    const auto visible = readBool(*object, "visible"_L1, path, resolved.visible);
    if(!visible) {
        return std::unexpected{visible.error()};
    }
    resolved.visible = *visible;

    const auto alignment = object->value("alignment"_L1);
    if(!alignment.isUndefined()) {
        const auto name = alignment.toString();
        if(!alignment.isString()
           || (name != "widget"_L1 && name != "leading"_L1 && name != "center"_L1 && name != "trailing"_L1)) {
            return std::unexpected{errorWithPath(
                QCoreApplication::translate("ThemeAppearance",
                                            "Choose the widget default, leading, centred or trailing text alignment."),
                path + u".alignment"_s)};
        }
        resolved.alignment = name == "leading"_L1  ? ProgressBarAppearance::Text::Alignment::Leading
                           : name == "center"_L1   ? ProgressBarAppearance::Text::Alignment::Centre
                           : name == "trailing"_L1 ? ProgressBarAppearance::Text::Alignment::Trailing
                                                   : ProgressBarAppearance::Text::Alignment::Widget;
    }

    const auto position = object->value("position"_L1);
    if(!position.isUndefined()) {
        const auto name = position.toString();
        if(!position.isString() || (name != "inside"_L1 && name != "left"_L1 && name != "right"_L1)) {
            return std::unexpected{
                errorWithPath(QCoreApplication::translate(
                                  "ThemeAppearance", "Choose text inside, to the left or to the right of the bar."),
                              path + u".position"_s)};
        }
        resolved.position = name == "left"_L1  ? ProgressBarAppearance::Text::Position::Left
                          : name == "right"_L1 ? ProgressBarAppearance::Text::Position::Right
                                               : ProgressBarAppearance::Text::Position::Inside;
    }

    return resolved;
}

std::expected<ProgressBarAppearance::Busy, QString> readBusy(const QJsonValue& value, const QString& path,
                                                             const ProgressBarAppearance::Busy& defaults)
{
    auto resolved{defaults};
    const auto object = readObject(value, path, {"duration"_L1, "segmentSize"_L1, "animated"_L1});
    if(!object) {
        return std::unexpected{object.error()};
    }

    for(const auto& [name, target, minimum, maximum] :
        std::array{std::tuple{"duration"_L1, &resolved.duration, 100, 60000},
                   std::tuple{"segmentSize"_L1, &resolved.segmentSize, 1, 100}}) {
        const auto field = readNumber(*object, name, path, *target, minimum, maximum);
        if(!field) {
            return std::unexpected{field.error()};
        }
        *target = *field;
    }

    const auto animated = readBool(*object, "animated"_L1, path, resolved.animated);
    if(!animated) {
        return std::unexpected{animated.error()};
    }
    resolved.animated = *animated;

    return resolved;
}

std::expected<ProgressBarAppearance::Colours, QString> readColours(const QJsonValue& value, const QString& path,
                                                                   const ProgressBarAppearance::Colours& defaults)
{
    auto resolved{defaults};
    const auto object = readObject(value, path,
                                   {"trackColour"_L1, "compactTrackColour"_L1, "borderColour"_L1, "fillColour"_L1,
                                    "textColour"_L1, "filledTextColour"_L1});
    if(!object) {
        return std::unexpected{object.error()};
    }

    const std::array brushes{std::pair{"trackColour"_L1, &resolved.trackColour},
                             std::pair{"compactTrackColour"_L1, &resolved.compactTrackColour},
                             std::pair{"fillColour"_L1, &resolved.fillColour}};
    for(const auto& [name, brush] : brushes) {
        const auto source = readBrushSource(object->value(name), path + u'.' + name, *brush);
        if(!source) {
            return std::unexpected{source.error()};
        }
        *brush = *source;
    }

    const std::array colours{std::pair{"borderColour"_L1, &resolved.borderColour},
                             std::pair{"textColour"_L1, &resolved.textColour},
                             std::pair{"filledTextColour"_L1, &resolved.filledTextColour}};
    for(const auto& [name, colour] : colours) {
        const auto source = readColourSource(object->value(name), path + u'.' + name, *colour);
        if(!source) {
            return std::unexpected{source.error()};
        }
        *colour = *source;
    }

    return resolved;
}
} // namespace

std::expected<ProgressBarAppearance, QString> resolveProgressBarAppearance(const QJsonObject& appearance)
{
    ProgressBarAppearance resolved;
    const QString path = u"appearance.progressBar"_s;
    const auto object
        = readObject(appearance.value("progressBar"_L1), path,
                     {"minimumThickness"_L1, "track"_L1, "fill"_L1, "normal"_L1, "disabled"_L1, "text"_L1, "busy"_L1});
    if(!object) {
        return std::unexpected{object.error()};
    }

    const auto minimumThickness = readNumber(*object, "minimumThickness"_L1, path, resolved.minimumThickness, 0, 512);
    if(!minimumThickness) {
        return std::unexpected{minimumThickness.error()};
    }
    resolved.minimumThickness = *minimumThickness;

    const auto track = readTrack(object->value("track"_L1), path + u".track"_s, resolved.track);
    if(!track) {
        return std::unexpected{track.error()};
    }
    resolved.track = *track;

    const auto fill = readFill(object->value("fill"_L1), path + u".fill"_s, resolved.fill);
    if(!fill) {
        return std::unexpected{fill.error()};
    }
    resolved.fill = *fill;

    for(const auto& [name, target] :
        std::array{std::pair{"normal"_L1, &resolved.normal}, std::pair{"disabled"_L1, &resolved.disabled}}) {
        const auto state = readColours(stateOverrides(*object, name), path + u'.' + name, *target);
        if(!state) {
            return std::unexpected{state.error()};
        }
        *target = *state;
    }

    const auto text = readText(object->value("text"_L1), path + u".text"_s, resolved.text);
    if(!text) {
        return std::unexpected{text.error()};
    }
    resolved.text = *text;

    const auto busy = readBusy(object->value("busy"_L1), path + u".busy"_s, resolved.busy);
    if(!busy) {
        return std::unexpected{busy.error()};
    }
    resolved.busy = *busy;

    return resolved;
}
} // namespace Fooyin
