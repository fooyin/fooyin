/*
 * Fooyin
 * Copyright 2022-2024, Luke Taylor <LukeT1@proton.me>
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

#include <gui/widgets/toolbutton.h>

#include <QStyleOptionToolButton>
#include <QStylePainter>

namespace Fooyin {
ToolButton::ToolButton(QWidget* parent)
    : QToolButton{parent}
{ }

void ToolButton::enterEvent(QEnterEvent* event)
{
    QToolButton::enterEvent(event);
    emit entered();
}

void ToolButton::paintEvent(QPaintEvent* /*event*/)
{
    QStylePainter painter{this};
    QStyleOptionToolButton opt;
    initStyleOption(&opt);
    // Remove menu indicator
    opt.features &= ~QStyleOptionToolButton::HasMenu;
    painter.drawComplexControl(QStyle::CC_ToolButton, opt);
}
} // namespace Fooyin

#include "gui/widgets/moc_toolbutton.cpp"
