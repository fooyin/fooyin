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

#include <utils/widgets/popuplineedit.h>

#include <QKeyEvent>

namespace Fy::Utils {
PopupLineEdit::PopupLineEdit(QWidget* parent)
    : PopupLineEdit{QStringLiteral(""), parent}
{ }

PopupLineEdit::PopupLineEdit(const QString& contents, QWidget* parent)
    : QLineEdit{contents, parent}
{
    setWindowFlags(Qt::CustomizeWindowHint | Qt::Tool);
}

void PopupLineEdit::keyPressEvent(QKeyEvent* event)
{
    QLineEdit::keyPressEvent(event);

    if(event->key() == Qt::Key_Escape) {
        emit editingCancelled();
    }
}

void PopupLineEdit::focusOutEvent(QFocusEvent* event)
{
    QLineEdit::focusOutEvent(event);

    emit editingFinished();
}
} // namespace Fy::Utils
