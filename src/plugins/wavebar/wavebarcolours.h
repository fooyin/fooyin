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

namespace Fooyin::WaveBar {
struct Colours
{
    QColor bgUnplayed{Qt::transparent};
    QColor bgPlayed{Qt::transparent};

    QColor maxUnplayed{140, 140, 140};
    QColor maxPlayed{qApp->palette().highlight().color()};

    QColor maxBorder{Qt::transparent};
    QColor minUnplayed{140, 140, 140};
    QColor minPlayed{qApp->palette().highlight().color()};
    QColor minBorder{Qt::transparent};

    QColor rmsMaxUnplayed{65, 65, 65};
    QColor rmsMaxPlayed{maxPlayed.darker(150)};
    QColor rmsMaxBorder{Qt::transparent};

    QColor rmsMinUnplayed{65, 65, 65};
    QColor rmsMinPlayed{maxPlayed.darker(150)};
    QColor rmsMinBorder{Qt::transparent};

    QColor cursor{maxPlayed};
    QColor seekingCursor{rmsMaxPlayed};

    bool operator==(const Colours& other) const
    {
        return std::tie(bgUnplayed, bgPlayed, maxUnplayed, maxPlayed, maxBorder, minUnplayed, minPlayed, minBorder,
                        rmsMaxUnplayed, rmsMaxPlayed, rmsMaxBorder, rmsMinUnplayed, rmsMinPlayed, rmsMinBorder, cursor,
                        seekingCursor)
            == std::tie(other.bgUnplayed, other.bgPlayed, other.maxUnplayed, other.maxPlayed, other.maxBorder,
                        other.minUnplayed, other.minPlayed, other.minBorder, other.rmsMaxUnplayed, other.rmsMaxPlayed,
                        other.rmsMaxBorder, other.rmsMinUnplayed, other.rmsMinPlayed, other.rmsMinBorder, other.cursor,
                        other.seekingCursor);
    };

    bool operator!=(const Colours& other) const
    {
        return !(*this == other);
    };

    friend QDataStream& operator<<(QDataStream& stream, const Colours& colours)
    {
        stream << colours.bgUnplayed;
        stream << colours.bgPlayed;
        stream << colours.maxUnplayed;
        stream << colours.maxPlayed;
        stream << colours.maxBorder;
        stream << colours.minUnplayed;
        stream << colours.minPlayed;
        stream << colours.minBorder;
        stream << colours.rmsMaxUnplayed;
        stream << colours.rmsMaxPlayed;
        stream << colours.rmsMaxBorder;
        stream << colours.rmsMinUnplayed;
        stream << colours.rmsMinPlayed;
        stream << colours.rmsMinBorder;
        stream << colours.cursor;
        return stream;
    }

    friend QDataStream& operator>>(QDataStream& stream, Colours& colours)
    {
        stream >> colours.bgUnplayed;
        stream >> colours.bgPlayed;
        stream >> colours.maxUnplayed;
        stream >> colours.maxPlayed;
        stream >> colours.maxBorder;
        stream >> colours.minUnplayed;
        stream >> colours.minPlayed;
        stream >> colours.minBorder;
        stream >> colours.rmsMaxUnplayed;
        stream >> colours.rmsMaxPlayed;
        stream >> colours.rmsMaxBorder;
        stream >> colours.rmsMinUnplayed;
        stream >> colours.rmsMinPlayed;
        stream >> colours.rmsMinBorder;
        stream >> colours.cursor;
        return stream;
    }
};
} // namespace Fooyin::WaveBar

Q_DECLARE_METATYPE(Fooyin::WaveBar::Colours)
