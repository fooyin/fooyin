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

#include <gui/fywidget.h>

#include <QJsonArray>

namespace Fooyin {
FyWidget::FyWidget(QWidget* parent)
    : QWidget{parent}
    , m_id{"FyWidget."}
{
    static int id{0};
    m_id.append(id++);
}

Id FyWidget::id() const
{
    return m_id;
}

QString FyWidget::layoutName() const
{
    return name();
}

FyWidget* FyWidget::findParent() const
{
    QWidget* parent = parentWidget();
    while(parent && !qobject_cast<FyWidget*>(parent)) {
        parent = parent->parentWidget();
    }
    return qobject_cast<FyWidget*>(parent);
}

void FyWidget::layoutEditingMenu(ActionContainer* /*menu*/) { }

void FyWidget::saveLayout(QJsonArray& array)
{
    array.append(layoutName());
}

void FyWidget::loadLayout(const QJsonObject& /*object*/) { }
} // namespace Fooyin

#include "gui/moc_fywidget.cpp"
