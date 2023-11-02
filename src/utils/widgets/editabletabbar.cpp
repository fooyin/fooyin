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

#include <utils/widgets/editabletabbar.h>

#include <utils/widgets/popuplineedit.h>

namespace Fy::Utils {
EditableTabBar::EditableTabBar(QWidget* parent)
    : QTabBar{parent}
{
    setMovable(true);
}

void EditableTabBar::showEditor()
{
    auto* lineEdit = new PopupLineEdit(tabText(currentIndex()), this);
    lineEdit->setAttribute(Qt::WA_DeleteOnClose);

    QObject::connect(lineEdit, &PopupLineEdit::editingCancelled, lineEdit, &QWidget::close);
    QObject::connect(lineEdit, &PopupLineEdit::editingFinished, this, [this, lineEdit]() {
        const QString text = lineEdit->text();
        if(text != tabText(currentIndex())) {
            setTabText(currentIndex(), lineEdit->text());
            emit tabTextChanged(currentIndex(), lineEdit->text());
        }
        lineEdit->close();
    });

    const QRect rect = tabRect(currentIndex());
    lineEdit->setFixedSize(rect.size());

    lineEdit->move(mapToGlobal(rect.topLeft()));
    lineEdit->setText(tabText(currentIndex()));

    lineEdit->show();
    lineEdit->selectAll();
    lineEdit->activateWindow();
    lineEdit->setFocus(Qt::ActiveWindowFocusReason);
}

void EditableTabBar::mouseDoubleClickEvent(QMouseEvent* /*event*/)
{
    showEditor();
}
} // namespace Fy::Utils
