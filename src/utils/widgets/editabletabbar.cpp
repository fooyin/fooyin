/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
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

#include <gui/guiconstants.h>
#include <utils/utils.h>
#include <utils/widgets/popuplineedit.h>

namespace Fooyin {
EditableTabBar::EditableTabBar(QWidget* parent)
    : QTabBar{parent}
    , m_lineEdit{nullptr}
    , m_showAddButton{false}
    , m_addButtonAdded{false}
{
    setMovable(true);
}

bool EditableTabBar::addButtonEnabled() const
{
    return m_showAddButton;
}

void EditableTabBar::setAddButtonEnabled(bool enabled)
{
    if(std::exchange(m_showAddButton, enabled) == enabled) {
        return;
    }

    if(m_showAddButton) {
        m_addButtonAdded = true;
        insertTab(0, Utils::iconFromTheme(Constants::Icons::Add), QStringLiteral(""));
        setMovable(false);
    }
    else if(m_addButtonAdded) {
        m_addButtonAdded = false;
        removeTab(0);
        setMovable(true);
    }
}

void EditableTabBar::showEditor()
{
    m_lineEdit = new PopupLineEdit(tabText(currentIndex()), this);
    m_lineEdit->setAttribute(Qt::WA_DeleteOnClose);

    const int currIndex = currentIndex();

    QObject::connect(m_lineEdit, &QObject::destroyed, this, [this]() { m_lineEdit = nullptr; });
    QObject::connect(m_lineEdit, &PopupLineEdit::editingCancelled, m_lineEdit, &QWidget::close);
    QObject::connect(m_lineEdit, &PopupLineEdit::editingFinished, this, [this, currIndex]() {
        const QString text = m_lineEdit->text();
        if(text != tabText(currIndex)) {
            setTabText(currIndex, m_lineEdit->text());
            emit tabTextChanged(currIndex, m_lineEdit->text());
        }
        m_lineEdit->close();
    });

    const QRect rect = tabRect(currIndex);
    m_lineEdit->setGeometry(rect);

    m_lineEdit->show();
    m_lineEdit->selectAll();
    m_lineEdit->setFocus(Qt::ActiveWindowFocusReason);
}

void EditableTabBar::closeEditor()
{
    if(m_lineEdit) {
        m_lineEdit->close();
    }
}

void EditableTabBar::mouseDoubleClickEvent(QMouseEvent* event)
{
    if(m_showAddButton) {
        const QPoint pos = event->position().toPoint();

        if(isAddButtonTab(pos)) {
            return;
        }
    }

    showEditor();
    QTabBar::mouseDoubleClickEvent(event);
}

void EditableTabBar::mousePressEvent(QMouseEvent* event)
{
    if(m_showAddButton) {
        const QPoint pos = event->position().toPoint();

        if(isAddButtonTab(pos)) {
            emit addButtonClicked();
            return;
        }
    }

    QTabBar::mousePressEvent(event);
}

bool EditableTabBar::isAddButtonTab(const QPoint& pos)
{
    if(tabRect(currentIndex()).contains(pos)) {
        return currentIndex() == 0;
    }
    if(isTabEnabled(0) && tabRect(0).contains(pos)) {
        return true;
    }

    return false;
}
} // namespace Fooyin

#include "utils/widgets/moc_editabletabbar.cpp"
