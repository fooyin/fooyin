/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <LukeT1@proton.me>
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

#include <QApplication>
#include <QColor>
#include <QDataStream>
#include <QMetaType>
#include <QPalette>

namespace Fooyin::VuMeter {
struct Colours
{
    enum class Type : uint8_t
    {
        Background = 0,
        Peak,
        Legend,
        Gradient1,
        Gradient2,
    };

    static QColor defaultColour(Type type)
    {
        switch(type) {
            case Type::Background:
                return Qt::transparent;
            case Type::Peak:
                return {190, 40, 10};
            case Type::Legend:
                return QApplication::palette().text().color();
            case Type::Gradient1:
                return {65, 65, 65};
            case Type::Gradient2:
                return QApplication::palette().highlight().color();
        }

        return {};
    }

    QMap<Type, QColor> meterColours{{Type::Background, defaultColour(Type::Background)},
                                    {Type::Peak, defaultColour(Type::Peak)},
                                    {Type::Legend, defaultColour(Type::Legend)},
                                    {Type::Gradient1, defaultColour(Type::Gradient1)},
                                    {Type::Gradient2, defaultColour(Type::Gradient2)}};

    [[nodiscard]] QColor colour(Type type) const
    {
        if(meterColours.contains(type)) {
            return meterColours.value(type);
        }

        return defaultColour(type);
    }

    void setColour(Type type, const QColor& colour)
    {
        meterColours[type] = colour;
    }

    bool operator==(const Colours& other) const
    {
        return std::tie(meterColours) == std::tie(other.meterColours);
    };

    bool operator!=(const Colours& other) const
    {
        return !(*this == other);
    };

    friend QDataStream& operator<<(QDataStream& stream, const Colours& colours)
    {
        stream << colours.meterColours;
        return stream;
    }

    friend QDataStream& operator>>(QDataStream& stream, Colours& colours)
    {
        stream >> colours.meterColours;
        return stream;
    }
};
} // namespace Fooyin::VuMeter

Q_DECLARE_METATYPE(Fooyin::VuMeter::Colours)
