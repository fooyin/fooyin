/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <luket@pm.me>
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

#pragma once

#include <gui/brushsource.h>
#include <utils/datastream.h>

#include <QApplication>
#include <QColor>
#include <QDataStream>
#include <QMetaType>
#include <QPalette>

#include <optional>
#include <vector>

namespace Fooyin::VuMeter {
struct Colours
{
    static constexpr qint32 Magic   = 0x5655434F;
    static constexpr qint32 Version = 2;

    enum class Type : uint8_t
    {
        Background = 0,
        Peak,
        Legend,
        // Retained for reading legacy binary colour maps
        Gradient1,
        Gradient2,
        Gridlines,
    };

    static QColor defaultColour(Type type, const QPalette& palette = QApplication::palette())
    {
        switch(type) {
            case Type::Background:
                return palette.color(QPalette::Window);
            case Type::Peak:
                return {190, 40, 10};
            case Type::Legend:
                return palette.text().color();
            case Type::Gridlines:
                return defaultGridColour(palette);
            default:
                return {};
        }
    }

    static QColor defaultGridColour(const QPalette& palette)
    {
        QColor colour = palette.color(QPalette::Active, QPalette::Text);
        colour.setAlpha(65);
        return colour;
    }

    static BrushSource defaultGradient()
    {
        return LinearGradientSource{.direction = LinearGradientSource::Direction::AlongControl,
                                    .stops     = {{.position = 0.0, .colour = QColor{65, 65, 65}},
                                                  {.position = 1.0, .colour = QPalette::Highlight}}};
    }

    QMap<Type, QColor> meterColours;
    std::optional<BrushSource> barGradient;

    [[nodiscard]] QColor colour(Type type, const QPalette& palette = QApplication::palette()) const
    {
        return meterColours.value(type, defaultColour(type, palette));
    }

    [[nodiscard]] bool hasOverride(Type type) const
    {
        return meterColours.contains(type);
    }

    [[nodiscard]] bool isEmpty() const
    {
        return meterColours.isEmpty() && !barGradient.has_value();
    }

    [[nodiscard]] BrushSource gradient() const
    {
        return barGradient ? *barGradient : defaultGradient();
    }

    [[nodiscard]] const std::optional<BrushSource>& customGradient() const
    {
        return barGradient;
    }

    void setColour(Type type, const QColor& colour)
    {
        if(colour.isValid()) {
            meterColours[type] = colour;
        }
        else {
            meterColours.remove(type);
        }
    }

    void setGradient(const BrushSource& brush)
    {
        barGradient = brush;
    }

    void setLegacyGradient(const std::vector<QColor>& colours)
    {
        barGradient.reset();

        LinearGradientSource gradient;
        gradient.stops.clear();

        for(const auto& colour : colours) {
            if(colour.isValid()) {
                gradient.stops.emplace_back(0.0, colour);
            }
        }
        if(gradient.stops.size() == 1) {
            barGradient = std::get<QColor>(gradient.stops.front().colour);
        }
        else if(!gradient.stops.empty()) {
            for(size_t index{0}; index < gradient.stops.size(); ++index) {
                gradient.stops[index].position
                    = static_cast<double>(index) / static_cast<double>(gradient.stops.size() - 1);
            }
            barGradient = gradient;
        }
    }

    bool operator==(const Colours& other) const = default;

    friend QDataStream& operator<<(QDataStream& stream, const Colours& colours)
    {
        stream << Magic;
        stream << Version;
        stream << colours.meterColours;
        stream << (colours.barGradient ? brushSourceToJson(*colours.barGradient) : QJsonObject{});
        return stream;
    }

    friend QDataStream& operator>>(QDataStream& stream, Colours& colours)
    {
        qint32 magic{0};
        qint32 version{0};

        stream.startTransaction();
        stream >> magic;
        stream >> version;

        if(magic == Magic && (version == 1 || version == Version)) {
            stream >> colours.meterColours;

            if(version == 1) {
                std::vector<QColor> legacy;
                DataStream::readContainer(stream, legacy);
                colours.setLegacyGradient(legacy);
            }
            else {
                QJsonObject source;
                stream >> source;

                colours.barGradient.reset();

                if(!source.isEmpty()) {
                    if(const auto brush = readBrushSource(source, QString{})) {
                        colours.barGradient = *brush;
                    }
                    else {
                        stream.setStatus(QDataStream::ReadCorruptData);
                    }
                }
            }

            if(!stream.commitTransaction()) {
                colours = {};
            }
            return stream;
        }

        stream.rollbackTransaction();
        stream.resetStatus();

        if(magic == Magic) {
            colours = {};
            stream.setStatus(QDataStream::ReadCorruptData);
            return stream;
        }

        QMap<Type, QColor> legacyColours;
        stream >> legacyColours;

        std::vector<QColor> legacyGradient;
        if(legacyColours.contains(Type::Gradient1)) {
            legacyGradient.emplace_back(legacyColours.value(Type::Gradient1));
            legacyColours.remove(Type::Gradient1);
        }
        if(legacyColours.contains(Type::Gradient2)) {
            legacyGradient.emplace_back(legacyColours.value(Type::Gradient2));
            legacyColours.remove(Type::Gradient2);
        }

        colours.meterColours = std::move(legacyColours);
        colours.setLegacyGradient(legacyGradient);
        return stream;
    }
};
} // namespace Fooyin::VuMeter

Q_DECLARE_METATYPE(Fooyin::VuMeter::Colours)
