/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <luket@pm.me>
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

#include <gui/widgets/editabletabbar.h>

#include <gui/guiconstants.h>
#include <gui/widgets/popuplineedit.h>
#include <utils/utils.h>

#include <QInputDialog>
#include <QMainWindow>
#include <QMouseEvent>
#include <QStyle>
#include <QStyleOption>
#include <QWheelEvent>

namespace Fooyin {
EditableTabBar::EditableTabBar(QWidget* parent)
    : QTabBar{parent}
    , m_title{tr("Tab")}
    , m_mode{EditMode::Inline}
    , m_lineEdit{nullptr}
    , m_suppressHoverState{false}
{
    setMovable(true);
}

void EditableTabBar::showEditor(int index)
{
    if(m_mode == EditMode::Inline) {
        m_lineEdit = new PopupLineEdit(tabText(index), this);
        m_lineEdit->setAttribute(Qt::WA_DeleteOnClose);

        QObject::connect(m_lineEdit, &QObject::destroyed, this, [this]() { m_lineEdit = nullptr; });
        QObject::connect(m_lineEdit, &PopupLineEdit::editingCancelled, m_lineEdit, &QWidget::close);
        QObject::connect(m_lineEdit, &PopupLineEdit::editingFinished, this, [this, index]() {
            const QString text = m_lineEdit->text();
            if(!text.isEmpty() && text != tabText(index)) {
                setTabText(index, m_lineEdit->text());
                Q_EMIT tabTextChanged(index, m_lineEdit->text());
            }
            m_lineEdit->close();
        });

        const QRect rect = tabRect(index);
        m_lineEdit->setGeometry(rect);

        m_lineEdit->show();
        m_lineEdit->selectAll();
        m_lineEdit->setFocus(Qt::ActiveWindowFocusReason);
    }
    else {
        const QString currentText = tabText(index);

        bool ok{false};
        const QString text = QInputDialog::getText(Utils::getMainWindow(), tr("Rename Tab"), tr("Name:"),
                                                   QLineEdit::Normal, currentText, &ok);

        if(ok && !text.isEmpty() && text != currentText) {
            setTabText(index, text);
            Q_EMIT tabTextChanged(index, text);
        }
    }
}

void EditableTabBar::closeEditor()
{
    if(m_lineEdit) {
        m_lineEdit->close();
    }
}

void EditableTabBar::clearHoverState()
{
    m_suppressHoverState = true;
    update();
}

void EditableTabBar::setEditTitle(const QString& title)
{
    m_title = title;
}

void EditableTabBar::setEditMode(EditMode mode)
{
    m_mode = mode;
}

bool EditableTabBar::event(QEvent* event)
{
    if(event->type() == QEvent::HoverLeave) {
        m_accumDelta = {};
    }

    if(event->type() == QEvent::Enter || event->type() == QEvent::HoverEnter || event->type() == QEvent::HoverMove) {
        if(m_suppressHoverState) {
            m_suppressHoverState = false;
            update();
        }
    }

    return QTabBar::event(event);
}

void EditableTabBar::initStyleOption(QStyleOptionTab* option, int tabIndex) const
{
    QTabBar::initStyleOption(option, tabIndex);

    if(m_suppressHoverState) {
        option->state &= ~QStyle::State_MouseOver;
    }
}

void EditableTabBar::mouseDoubleClickEvent(QMouseEvent* event)
{
    if(event->button() > Qt::MiddleButton) {
        event->ignore();
        return;
    }

    const QPoint pos = event->position().toPoint();

    if(event->button() & Qt::MiddleButton) {
        return;
    }

    const int tab = tabAt(pos);
    if(tab >= 0) {
        showEditor(tab);
    }

    QTabBar::mouseDoubleClickEvent(event);
}

void EditableTabBar::mousePressEvent(QMouseEvent* event)
{
    if(event->button() != Qt::LeftButton) {
        event->ignore();
        return;
    }

    QTabBar::mousePressEvent(event);
}

void EditableTabBar::mouseReleaseEvent(QMouseEvent* event)
{
    if(event->button() & Qt::MiddleButton) {
        const QPoint pos = event->position().toPoint();
        const int index  = tabAt(pos);
        if(index >= 0 || rect().contains(pos)) {
            Q_EMIT middleClicked(index);
        }
        event->accept();
        return;
    }

    QTabBar::mouseReleaseEvent(event);
}

void EditableTabBar::wheelEvent(QWheelEvent* event)
{
    const QPoint pos = event->position().toPoint();

    if(tabAt(pos) < 0) {
        return;
    }

    if(!style()->styleHint(QStyle::SH_TabBar_AllowWheelScrolling)) {
        return;
    }

    if(currentIndex() > 0 && currentIndex() < count() - 1) {
        QTabBar::wheelEvent(event);
        Q_EMIT tabBarClicked(currentIndex());
        return;
    }

    // Wrap-around if scrolling on first or last tab
    // TODO: Make optional via a setting

    m_accumDelta += event->angleDelta();
    const int xSteps = m_accumDelta.x() / QWheelEvent::DefaultDeltasPerStep;
    const int ySteps = m_accumDelta.y() / QWheelEvent::DefaultDeltasPerStep;

    int offset{0};
    if(xSteps > 0 || ySteps > 0) {
        offset       = -1;
        m_accumDelta = {};
    }
    else if(xSteps < 0 || ySteps < 0) {
        offset       = 1;
        m_accumDelta = {};
    }

    int proposedIndex = currentIndex() + offset;
    if(proposedIndex < 0) {
        proposedIndex = count() - 1;
    }
    else if(proposedIndex >= count()) {
        proposedIndex = 0;
    }

    if(isTabEnabled(proposedIndex) && isTabVisible(proposedIndex)) {
        if(proposedIndex != currentIndex()) {
            setCurrentIndex(proposedIndex);
            Q_EMIT tabBarClicked(currentIndex());
            event->accept();
        }
    }
}
} // namespace Fooyin

#include "gui/widgets/moc_editabletabbar.cpp"
