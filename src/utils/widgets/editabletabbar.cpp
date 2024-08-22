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

#include <QInputDialog>
#include <QMainWindow>
#include <QMouseEvent>
#include <QStyle>
#include <QWheelEvent>

namespace Fooyin {
EditableTabBar::EditableTabBar(QWidget* parent)
    : QTabBar{parent}
    , m_title{tr("Tab")}
    , m_mode{EditMode::Inline}
    , m_lineEdit{nullptr}
{
    setMovable(true);
}

void EditableTabBar::showEditor()
{
    const int currIndex = currentIndex();

    if(m_mode == EditMode::Inline) {
        m_lineEdit = new PopupLineEdit(tabText(currentIndex()), this);
        m_lineEdit->setAttribute(Qt::WA_DeleteOnClose);

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
    else {
        const QString currentText = tabText(currIndex);

        bool ok{false};
        const QString text = QInputDialog::getText(Utils::getMainWindow(), tr("Edit %1 Name").arg(m_title),
                                                   tr("%1 name:").arg(m_title), QLineEdit::Normal, currentText, &ok);

        if(ok && !text.isEmpty() && text != currentText) {
            setTabText(currIndex, text);
            emit tabTextChanged(currIndex, text);
        }
    }
}

void EditableTabBar::closeEditor()
{
    if(m_lineEdit) {
        m_lineEdit->close();
    }
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
    return QTabBar::event(event);
}

void EditableTabBar::mouseDoubleClickEvent(QMouseEvent* event)
{
    const QPoint pos = event->position().toPoint();

    if(event->button() & Qt::MiddleButton) {
        return;
    }

    if(tabAt(pos) >= 0) {
        showEditor();
    }

    QTabBar::mouseDoubleClickEvent(event);
}

void EditableTabBar::mousePressEvent(QMouseEvent* event)
{
    const QPoint pos = event->position().toPoint();

    if(event->button() & Qt::MiddleButton) {
        emit middleClicked(tabAt(pos));
    }

    QTabBar::mousePressEvent(event);
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
        emit tabBarClicked(currentIndex());
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
            emit tabBarClicked(currentIndex());
            event->accept();
        }
    }
}
} // namespace Fooyin

#include "utils/widgets/moc_editabletabbar.cpp"
