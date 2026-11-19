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

#include <gui/brushsource.h>

#include <utils/jsonutils.h>

#include <QCoreApplication>
#include <QJsonArray>
#include <QLinearGradient>

#include <array>
#include <cmath>

using namespace Qt::StringLiterals;

namespace Fooyin {
namespace {
constexpr std::array gradientDirections{
    std::pair{LinearGradientSource::Direction::AlongControl, "alongControl"_L1},
    std::pair{LinearGradientSource::Direction::AcrossControl, "acrossControl"_L1},
    std::pair{LinearGradientSource::Direction::Horizontal, "horizontal"_L1},
    std::pair{LinearGradientSource::Direction::Vertical, "vertical"_L1},
};
} // namespace

QBrush resolveBrush(const BrushSource& source, const QPalette& palette, QPalette::ColorGroup group,
                    const QRectF& bounds, Qt::Orientation controlOrientation, bool upsideDown)
{
    if(const auto* role = std::get_if<QPalette::ColorRole>(&source)) {
        return resolveBrush(ColourSource{*role}, palette, group);
    }
    if(const auto* colour = std::get_if<QColor>(&source)) {
        return QBrush{*colour};
    }

    const auto& gradientSource = std::get<LinearGradientSource>(source);
    if(gradientSource.stops.empty()) {
        return QBrush{Qt::transparent};
    }

    using Direction = LinearGradientSource::Direction;
    const bool horizontal
        = gradientSource.direction == Direction::Horizontal
       || (gradientSource.direction == Direction::AlongControl && controlOrientation == Qt::Horizontal)
       || (gradientSource.direction == Direction::AcrossControl && controlOrientation == Qt::Vertical);

    auto start = bounds.topLeft();
    auto end   = horizontal ? bounds.topRight() : bounds.bottomLeft();
    if(gradientSource.direction == Direction::AlongControl && upsideDown) {
        std::swap(start, end);
    }

    QLinearGradient gradient{start, end};
    for(const auto& stop : gradientSource.stops) {
        gradient.setColorAt(stop.position, resolveBrush(stop.colour, palette, group).color());
    }
    return QBrush{gradient};
}

QString brushSourceName(const BrushSource& brush)
{
    if(const auto* role = std::get_if<QPalette::ColorRole>(&brush)) {
        return colourSourceName(ColourSource{*role});
    }
    if(const auto* colour = std::get_if<QColor>(&brush)) {
        return colourSourceName(ColourSource{*colour});
    }
    return QCoreApplication::translate("BrushSource", "Linear gradient");
}

QJsonObject brushSourceToJson(const BrushSource& brush)
{
    if(const auto* role = std::get_if<QPalette::ColorRole>(&brush)) {
        return colourSourceToJson(ColourSource{*role});
    }
    if(const auto* colour = std::get_if<QColor>(&brush)) {
        return colourSourceToJson(ColourSource{*colour});
    }

    const auto& gradient = std::get<LinearGradientSource>(brush);

    QJsonArray stops;
    for(const auto& stop : gradient.stops) {
        stops.append(QJsonObject{{u"position"_s, stop.position}, {u"colour"_s, colourSourceToJson(stop.colour)}});
    }

    const auto direction = std::ranges::find_if(
        gradientDirections, [&gradient](const auto& entry) { return entry.first == gradient.direction; });
    return {{u"linearGradient"_s, QJsonObject{{u"orientation"_s, QString{direction->second}}, {u"stops"_s, stops}}}};
}

std::expected<BrushSource, QString> readBrushSource(const QJsonValue& value, const QString& path,
                                                    const BrushSource& fallback)
{
    if(value.isUndefined()) {
        return fallback;
    }

    if(!value.isObject() || !value.toObject().contains("linearGradient"_L1)) {
        const auto colour = readColourSource(value, path);
        if(!colour) {
            return std::unexpected{colour.error()};
        }
        if(const auto* role = std::get_if<QPalette::ColorRole>(&*colour)) {
            return *role;
        }
        return std::get<QColor>(*colour);
    }

    const auto object = Utils::readObject(value, path, {"linearGradient"_L1});
    if(!object) {
        return std::unexpected{object.error()};
    }

    const QString gradientPath = path + u".linearGradient"_s;
    const auto gradient        = Utils::readObject(object->value("linearGradient"_L1), gradientPath,
                                                   {"orientation"_L1, "colours"_L1, "stops"_L1});
    if(!gradient) {
        return std::unexpected{gradient.error()};
    }

    LinearGradientSource resolved;

    const auto direction = gradient->value("orientation"_L1);
    if(!direction.isUndefined()) {
        const auto found = std::ranges::find_if(
            gradientDirections, [&direction](const auto& entry) { return entry.second == direction.toString(); });
        if(!direction.isString() || found == gradientDirections.end()) {
            return std::unexpected{
                Utils::errorWithPath(QCoreApplication::translate("BrushSource", "Choose a valid gradient direction."),
                                     gradientPath + u".orientation"_s)};
        }
        resolved.direction = found->first;
    }

    const bool positioned = gradient->contains("stops"_L1);
    if(positioned && gradient->contains("colours"_L1)) {
        return std::unexpected{Utils::errorWithPath(
            QCoreApplication::translate("BrushSource", "Choose either positioned stops or evenly spaced colours."),
            gradientPath)};
    }

    const auto key    = positioned ? "stops"_L1 : "colours"_L1;
    const auto values = gradient->value(key);
    if(!values.isArray() || values.toArray().size() < 2 || values.toArray().size() > 64) {
        return std::unexpected{Utils::errorWithPath(
            QCoreApplication::translate("BrushSource", "A gradient must contain between 2 and 64 colours."),
            gradientPath + u'.' + key)};
    }

    resolved.stops.clear();

    const auto valuesArray = values.toArray();
    double previous{-1.0};
    for(qsizetype index{0}; index < valuesArray.size(); ++index) {
        const QString stopPath = gradientPath + u'.' + key + u"[%1]"_s.arg(index);
        qreal position         = static_cast<double>(index) / static_cast<double>(valuesArray.size() - 1);

        QJsonValue colourValue{valuesArray[index]};
        if(positioned) {
            const auto stop = Utils::readObject(valuesArray[index], stopPath, {"position"_L1, "colour"_L1});
            if(!stop) {
                return std::unexpected{stop.error()};
            }

            const auto positionValue = stop->value("position"_L1);
            position                 = positionValue.toDouble(-1.0);
            if(!positionValue.isDouble() || !std::isfinite(position) || position < 0.0 || position > 1.0
               || position <= previous) {
                return std::unexpected{Utils::errorWithPath(
                    QCoreApplication::translate(
                        "BrushSource", "Stop positions must be between 0 and 1, in strictly increasing order."),
                    stopPath + u".position"_s)};
            }

            colourValue = stop->value("colour"_L1);
            if(colourValue.isUndefined()) {
                return std::unexpected{Utils::errorWithPath(
                    QCoreApplication::translate("BrushSource", "Choose a colour for each gradient stop."),
                    stopPath + u".colour"_s)};
            }
        }

        const auto colour = readColourSource(colourValue, positioned ? stopPath + u".colour"_s : stopPath);
        if(!colour) {
            return std::unexpected{colour.error()};
        }

        resolved.stops.emplace_back(position, *colour);
        previous = position;
    }

    return resolved;
}
} // namespace Fooyin
