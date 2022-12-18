/*
 * Fooyin
 * Copyright 2022, Luke Taylor <LukeT1@proton.me>
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

#include "fywidget.h"

#include <QJsonArray>

FyWidget::FyWidget(QWidget* parent)
    : QWidget(parent)
{ }

FyWidget::~FyWidget() = default;

FyWidget* FyWidget::findParent()
{
    QWidget* parent = parentWidget();
    while(parent && !qobject_cast<FyWidget*>(parent)) {
        parent = parent->parentWidget();
    }
    return qobject_cast<FyWidget*>(parent);
}

void FyWidget::addWidgetMenu(QMenu* menu, QAction* action)
{
    Q_UNUSED(menu)
    Q_UNUSED(action)
}

void FyWidget::layoutEditingMenu(QMenu* menu)
{
    Q_UNUSED(menu)
}

void FyWidget::saveLayout(QJsonArray& array)
{
    array.append(name());
}
