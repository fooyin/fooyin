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

#include <utils/datastream.h>

#include <QApplication>
#include <QColor>
#include <QDataStream>
#include <QMetaType>
#include <QPalette>

#include <vector>

namespace Fooyin::VuMeter {
struct Colours
{
    static constexpr qint32 Magic   = 0x5655434F;
    static constexpr qint32 Version = 1;

    enum class Type : uint8_t
    {
        Background = 0,
        Peak,
        Legend,
        Gradient1,
        Gradient2,
    };

    static QColor defaultColour(Type type, const QPalette& palette = QApplication::palette())
    {
        switch(type) {
            case Type::Background:
                return Qt::transparent;
            case Type::Peak:
                return {190, 40, 10};
            case Type::Legend:
                return palette.text().color();
            case Type::Gradient1:
                return defaultGradient(palette).front();
            case Type::Gradient2:
                return defaultGradient(palette).back();
        }

        return {};
    }

    static std::vector<QColor> defaultGradient(const QPalette& palette = QApplication::palette())
    {
        return {{65, 65, 65}, palette.color(QPalette::Active, QPalette::Highlight)};
    }

    QMap<Type, QColor> meterColours;
    std::vector<QColor> barGradient;

    [[nodiscard]] QColor colour(Type type, const QPalette& palette = QApplication::palette()) const
    {
        switch(type) {
            case Type::Gradient1:
                return gradient(palette).front();
            case Type::Gradient2:
                return gradient(palette).back();
            default:
                return meterColours.value(type, defaultColour(type, palette));
        }
    }

    [[nodiscard]] bool hasOverride(Type type) const
    {
        return type == Type::Gradient1 || type == Type::Gradient2 ? !barGradient.empty() : meterColours.contains(type);
    }

    [[nodiscard]] bool isEmpty() const
    {
        return meterColours.isEmpty() && barGradient.empty();
    }

    [[nodiscard]] std::vector<QColor> gradient(const QPalette& palette = QApplication::palette()) const
    {
        return barGradient.empty() ? defaultGradient(palette) : barGradient;
    }

    [[nodiscard]] const std::vector<QColor>& customGradient() const
    {
        return barGradient;
    }

    void setColour(Type type, const QColor& colour)
    {
        if(type == Type::Gradient1 || type == Type::Gradient2) {
            std::vector<QColor> colours = gradient();
            if(colours.size() < 2) {
                colours = defaultGradient();
            }
            colours.at(type == Type::Gradient1 ? 0 : colours.size() - 1) = colour;
            setGradient(colours);
            return;
        }

        if(colour.isValid()) {
            meterColours[type] = colour;
        }
        else {
            meterColours.remove(type);
        }
    }

    void setGradient(const std::vector<QColor>& colours)
    {
        barGradient.clear();
        for(const QColor& colour : colours) {
            if(colour.isValid()) {
                barGradient.push_back(colour);
            }
        }
    }

    bool operator==(const Colours& other) const
    {
        return std::tie(meterColours, barGradient) == std::tie(other.meterColours, other.barGradient);
    };

    bool operator!=(const Colours& other) const
    {
        return !(*this == other);
    };

    friend QDataStream& operator<<(QDataStream& stream, const Colours& colours)
    {
        stream << Magic;
        stream << Version;
        stream << colours.meterColours;
        DataStream::writeContainer(stream, colours.barGradient);
        return stream;
    }

    friend QDataStream& operator>>(QDataStream& stream, Colours& colours)
    {
        qint32 magic{0};
        qint32 version{0};

        stream.startTransaction();
        stream >> magic;
        stream >> version;

        if(magic == Magic && version == Version) {
            stream >> colours.meterColours;
            DataStream::readContainer(stream, colours.barGradient);
            stream.commitTransaction();
            return stream;
        }

        stream.rollbackTransaction();

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

        colours.meterColours = legacyColours;
        colours.setGradient(legacyGradient);
        return stream;
    }
};
} // namespace Fooyin::VuMeter

Q_DECLARE_METATYPE(Fooyin::VuMeter::Colours)
