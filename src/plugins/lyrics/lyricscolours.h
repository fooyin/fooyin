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

namespace Fooyin::Lyrics {
struct Colours
{
    enum class Type : uint8_t
    {
        Background = 0,
        LineUnplayed,
        LinePlayed,
        LineSynced,
        WordLineSynced,
        WordSynced,
        LineUnsynced
    };
    
    QMap<Type, QColor> lyricsColours;
    
    [[nodiscard]] static QColor defaultColour(Type type)
    {
        switch(type) {
            case Type::Background:     return QApplication::palette().base().color();
            case Type::LineUnplayed:   return QApplication::palette().placeholderText().color();
            case Type::LinePlayed:     return QApplication::palette().placeholderText().color();
            case Type::LineSynced:     return QApplication::palette().text().color();
            case Type::WordLineSynced: return QApplication::palette().text().color();
            case Type::WordSynced:     return QApplication::palette().highlight().color();
            case Type::LineUnsynced:   return QApplication::palette().text().color();
            default:                   return {};
        }
    }

    [[nodiscard]] QColor colour(Type type) const
    {
        return lyricsColours.value(type, defaultColour(type));
    }

    void setColour(Type type, const QColor& colour)
    {
        lyricsColours[type] = colour;
    }

    bool operator==(const Colours& other) const
    {
        return std::tie(lyricsColours) == std::tie(other.lyricsColours);
    };

    bool operator!=(const Colours& other) const
    {
        return !(*this == other);
    };

    friend QDataStream& operator<<(QDataStream& stream, const Colours& colours)
    {
        stream << colours.lyricsColours;
        return stream;
    }

    friend QDataStream& operator>>(QDataStream& stream, Colours& colours)
    {
        stream >> colours.lyricsColours;
        return stream;
    }
};
} // namespace Fooyin::Lyrics
