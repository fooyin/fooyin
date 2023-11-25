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

#pragma once

#include "fyutils_export.h"

#include <QTableView>

namespace Fooyin {
class ActionManager;
class WidgetContext;
class Command;

class FYUTILS_EXPORT ExtendableTableView : public QTableView
{
    Q_OBJECT

public:
    explicit ExtendableTableView(ActionManager* actionManager, QWidget* parent = nullptr);

    void rowAdded();

    [[nodiscard]] QAction* removeAction() const;

    virtual void addContextActions(QMenu* menu);

signals:
    void newRowClicked();

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    ActionManager* m_actionManager;
    WidgetContext* m_context;
    QAction* m_remove;
    Command* m_removeCommand;

    QRect m_buttonRect;
    bool m_mouseOverButton;
    bool m_pendingRow;
};
} // namespace Fooyin
