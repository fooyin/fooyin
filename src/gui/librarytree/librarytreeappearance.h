/*
 * Fooyin
 * Copyright 2022-2023, Luke Taylor <LukeT1@proton.me>
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
#include <QFont>
#include <QPalette>

namespace Fy::Gui::Widgets {
struct LibraryTreeAppearance
{
    bool fontChanged{false};
    QFont font;

    bool colourChanged{false};
    QColor colour;

    int rowHeight{25};

    LibraryTreeAppearance()
        : colour{QApplication::palette().text().color()}
    { }

    friend QDataStream& operator<<(QDataStream& stream, const LibraryTreeAppearance& options)
    {
        stream << options.fontChanged;
        stream << options.font;
        stream << options.colourChanged;
        stream << options.colour;
        stream << options.rowHeight;
        return stream;
    }

    friend QDataStream& operator>>(QDataStream& stream, LibraryTreeAppearance& options)
    {
        stream >> options.fontChanged;
        stream >> options.font;
        if(!options.fontChanged) {
            options.font = QFont{};
        }
        stream >> options.colourChanged;
        stream >> options.colour;
        if(!options.colourChanged) {
            options.colour = QApplication::palette().text().color();
        }
        stream >> options.rowHeight;
        return stream;
    }
};
} // namespace Fy::Gui::Widgets
