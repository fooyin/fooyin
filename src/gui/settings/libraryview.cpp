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

#include "libraryview.h"

#include <QHeaderView>
#include <QResizeEvent>

namespace Fy::Gui::Settings {
LibraryView::LibraryView(QWidget* parent)
    : QTableView{parent}
{ }

void LibraryView::resizeEvent(QResizeEvent* event)
{
    const int width = event->size().width();
    setColumnWidth(1, width * 0.30);
    setColumnWidth(2, width * 0.55);
    setColumnWidth(3, width * 0.15);
}
} // namespace Fy::Gui::Settings
