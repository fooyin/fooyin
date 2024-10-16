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
    };

    QMap<Type, QColor> lyricsColours{{Type::Background, QApplication::palette().base().color()},
                                     {Type::LineUnplayed, QApplication::palette().text().color().darker(150)},
                                     {Type::LinePlayed, QApplication::palette().text().color().darker(150)},
                                     {Type::LineSynced, QApplication::palette().text().color()},
                                     {Type::WordLineSynced, QApplication::palette().text().color()},
                                     {Type::WordSynced, QApplication::palette().highlight().color()}};

    [[nodiscard]] QColor colour(Type type) const
    {
        return lyricsColours.value(type);
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
