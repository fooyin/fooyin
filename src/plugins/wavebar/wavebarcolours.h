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

    QColor fgUnplayed{140, 140, 140};
    QColor fgPlayed{qApp->palette().highlight().color()};
    QColor fgBorder{Qt::transparent};

    QColor rmsUnplayed{65, 65, 65};
    QColor rmsPlayed{fgPlayed.darker(150)};
    QColor rmsBorder{Qt::transparent};

    QColor cursor{fgPlayed};
    QColor seekingCursor{rmsPlayed};

    bool operator==(const Colours& other) const
    {
        return std::tie(bgUnplayed, bgPlayed, fgUnplayed, fgPlayed, fgBorder, rmsUnplayed, rmsPlayed, rmsBorder, cursor,
                        seekingCursor)
            == std::tie(other.bgUnplayed, other.bgPlayed, other.fgUnplayed, other.fgBorder, other.fgPlayed,
                        other.rmsUnplayed, other.rmsPlayed, other.rmsBorder, other.cursor, other.seekingCursor);
    };

    bool operator!=(const Colours& other) const
    {
        return !(*this == other);
    };

    friend QDataStream& operator<<(QDataStream& stream, const Colours& colours)
    {
        stream << colours.bgUnplayed;
        stream << colours.bgPlayed;
        stream << colours.fgUnplayed;
        stream << colours.fgPlayed;
        stream << colours.fgBorder;
        stream << colours.rmsUnplayed;
        stream << colours.rmsPlayed;
        stream << colours.rmsBorder;
        stream << colours.cursor;
        return stream;
    }

    friend QDataStream& operator>>(QDataStream& stream, Colours& colours)
    {
        stream >> colours.bgUnplayed;
        stream >> colours.bgPlayed;
        stream >> colours.fgUnplayed;
        stream >> colours.fgPlayed;
        stream >> colours.fgBorder;
        stream >> colours.rmsUnplayed;
        stream >> colours.rmsPlayed;
        stream >> colours.rmsBorder;
        stream >> colours.cursor;
        return stream;
    }
};
} // namespace Fooyin::WaveBar

Q_DECLARE_METATYPE(Fooyin::WaveBar::Colours)
