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

#include <gui/widgets/popuplineedit.h>

#include <QKeyEvent>
#include <QPainter>

namespace Fooyin {
PopupLineEdit::PopupLineEdit(QWidget* parent)
    : PopupLineEdit{{}, parent}
{ }

PopupLineEdit::PopupLineEdit(const QString& contents, QWidget* parent)
    : QLineEdit{contents, parent}
    , m_initialString{contents}
{ }

void PopupLineEdit::paintEvent(QPaintEvent* event)
{
    QPainter painter{this};

    // Some styles paint QLineEdit with a transparent background
    // As we're designed to be used as a popup, override that
    painter.fillRect(event->rect(), palette().base());

    QLineEdit::paintEvent(event);
}

void PopupLineEdit::keyPressEvent(QKeyEvent* event)
{
    QLineEdit::keyPressEvent(event);

    if(event->key() == Qt::Key_Escape) {
        m_cancelled = true;
        Q_EMIT editingCancelled();
    }
}

void PopupLineEdit::focusOutEvent(QFocusEvent* event)
{
    if(m_initialString == text()) {
        Q_EMIT editingCancelled();
    }

    if(!m_cancelled) {
        // Allow our base widget to emit editingFinished.
        QLineEdit::focusOutEvent(event);
    }
}
} // namespace Fooyin

#include "gui/widgets/moc_popuplineedit.cpp"
