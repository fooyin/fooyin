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
#include <QMap>
#include <QMetaType>
#include <QPalette>

#include <tuple>

constexpr quint8 SerialisationVersion = 1;

namespace Fooyin::WaveBar {
struct Colours
{
    enum class Type : uint8_t
    {
        BgUnplayed = 0,
        BgPlayed,
        MaxUnplayed,
        MaxPlayed,
        MaxBorder,
        MinUnplayed,
        MinPlayed,
        MinBorder,
        RmsMaxUnplayed,
        RmsMaxPlayed,
        RmsMaxBorder,
        RmsMinUnplayed,
        RmsMinPlayed,
        RmsMinBorder,
        Cursor,
        SeekingCursor
    };

    QMap<Type, QColor> waveColours;

    [[nodiscard]] static QColor defaultColour(Type type, const QPalette& palette = QApplication::palette())
    {
        const QColor highlight = palette.color(QPalette::Active, QPalette::Highlight);
        const QColor rmsPlayed = highlight.darker(150);

        switch(type) {
            case Type::BgUnplayed:
            case Type::BgPlayed:
            case Type::MaxBorder:
            case Type::MinBorder:
            case Type::RmsMaxBorder:
            case Type::RmsMinBorder:
                return Qt::transparent;
            case Type::MaxUnplayed:
            case Type::MinUnplayed:
                return {140, 140, 140};
            case Type::MaxPlayed:
            case Type::MinPlayed:
            case Type::Cursor:
                return highlight;
            case Type::RmsMaxUnplayed:
            case Type::RmsMinUnplayed:
                return {65, 65, 65};
            case Type::RmsMaxPlayed:
            case Type::RmsMinPlayed:
            case Type::SeekingCursor:
                return rmsPlayed;
        }

        return {};
    }

    [[nodiscard]] QColor colour(Type type, const QPalette& palette = QApplication::palette()) const
    {
        return waveColours.value(type, defaultColour(type, palette));
    }

    [[nodiscard]] bool hasOverride(Type type) const
    {
        return waveColours.contains(type);
    }

    [[nodiscard]] bool isEmpty() const
    {
        return waveColours.isEmpty();
    }

    void setColour(Type type, const QColor& colour)
    {
        if(colour.isValid()) {
            waveColours[type] = colour;
        }
        else {
            waveColours.remove(type);
        }
    }

    bool operator==(const Colours& other) const
    {
        return std::tie(waveColours) == std::tie(other.waveColours);
    };

    bool operator!=(const Colours& other) const
    {
        return !(*this == other);
    };

    friend QDataStream& operator<<(QDataStream& stream, const Colours& colours)
    {
        stream << SerialisationVersion;
        stream << colours.waveColours;
        return stream;
    }

    friend QDataStream& operator>>(QDataStream& stream, Colours& colours)
    {
        colours.waveColours.clear();

        stream.startTransaction();

        quint8 version{0};
        stream >> version;

        QMap<Type, QColor> savedColours;

        if(version == SerialisationVersion) {
            stream >> savedColours;
        }

        if(version == SerialisationVersion && stream.commitTransaction()) {
            colours.waveColours = std::move(savedColours);
            return stream;
        }

        stream.rollbackTransaction();

        QColor bgUnplayed;
        QColor bgPlayed;
        QColor maxUnplayed;
        QColor maxPlayed;
        QColor maxBorder;
        QColor minUnplayed;
        QColor minPlayed;
        QColor minBorder;
        QColor rmsMaxUnplayed;
        QColor rmsMaxPlayed;
        QColor rmsMaxBorder;
        QColor rmsMinUnplayed;
        QColor rmsMinPlayed;
        QColor rmsMinBorder;
        QColor cursor;

        stream >> bgUnplayed;
        stream >> bgPlayed;
        stream >> maxUnplayed;
        stream >> maxPlayed;
        stream >> maxBorder;
        stream >> minUnplayed;
        stream >> minPlayed;
        stream >> minBorder;
        stream >> rmsMaxUnplayed;
        stream >> rmsMaxPlayed;
        stream >> rmsMaxBorder;
        stream >> rmsMinUnplayed;
        stream >> rmsMinPlayed;
        stream >> rmsMinBorder;
        stream >> cursor;

        colours.setColour(Type::BgUnplayed, bgUnplayed);
        colours.setColour(Type::BgPlayed, bgPlayed);
        colours.setColour(Type::MaxUnplayed, maxUnplayed);
        colours.setColour(Type::MaxPlayed, maxPlayed);
        colours.setColour(Type::MaxBorder, maxBorder);
        colours.setColour(Type::MinUnplayed, minUnplayed);
        colours.setColour(Type::MinPlayed, minPlayed);
        colours.setColour(Type::MinBorder, minBorder);
        colours.setColour(Type::RmsMaxUnplayed, rmsMaxUnplayed);
        colours.setColour(Type::RmsMaxPlayed, rmsMaxPlayed);
        colours.setColour(Type::RmsMaxBorder, rmsMaxBorder);
        colours.setColour(Type::RmsMinUnplayed, rmsMinUnplayed);
        colours.setColour(Type::RmsMinPlayed, rmsMinPlayed);
        colours.setColour(Type::RmsMinBorder, rmsMinBorder);
        colours.setColour(Type::Cursor, cursor);
        return stream;
    }
};
} // namespace Fooyin::WaveBar

Q_DECLARE_METATYPE(Fooyin::WaveBar::Colours)
