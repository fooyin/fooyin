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

    [[nodiscard]] static QColor dimmedTextColour(const QPalette& palette = QApplication::palette())
    {
        QColor colour = palette.color(QPalette::Active, QPalette::Text);
        colour.setAlpha(std::max(1, colour.alpha() / 2));
        return colour;
    }

    [[nodiscard]] static QColor defaultColour(Type type, const QPalette& palette = QApplication::palette())
    {
        switch(type) {
            case Type::Background:
                return palette.color(QPalette::Active, QPalette::Base);
            case Type::LineUnplayed:
            case Type::LinePlayed:
            case Type::WordLineSynced:
                return dimmedTextColour(palette);
            case Type::LineSynced:
            case Type::WordSynced:
            case Type::LineUnsynced:
                return palette.color(QPalette::Active, QPalette::Text);
            default:
                return {};
        }
    }

    [[nodiscard]] QColor colour(Type type, const QPalette& palette = QApplication::palette()) const
    {
        const QColor override = lyricsColours.value(type);
        if(override.isValid()) {
            return override;
        }

        return defaultColour(type, palette);
    }

    [[nodiscard]] bool hasOverride(Type type) const
    {
        return lyricsColours.contains(type);
    }

    [[nodiscard]] bool isEmpty() const
    {
        return lyricsColours.isEmpty();
    }

    void setColour(Type type, const QColor& colour)
    {
        if(colour.isValid()) {
            lyricsColours[type] = colour;
        }
        else {
            lyricsColours.remove(type);
        }
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
