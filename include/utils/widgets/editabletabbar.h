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

#pragma once

#include "fyutils_export.h"

#include <QTabBar>

namespace Fooyin {
class PopupLineEdit;

class FYUTILS_EXPORT EditableTabBar : public QTabBar
{
    Q_OBJECT

public:
    enum class EditMode
    {
        Dialog,
        Inline
    };

    explicit EditableTabBar(QWidget* parent = nullptr);

    void showEditor();
    void closeEditor();

    void setEditTitle(const QString& title);
    void setEditMode(EditMode mode);

signals:
    void middleClicked(int index);
    void addButtonClicked();
    void tabTextChanged(int index, const QString& text);

protected:
    bool event(QEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    QString m_title;
    EditMode m_mode;
    PopupLineEdit* m_lineEdit;
    QPoint m_accumDelta;
};
} // namespace Fooyin
