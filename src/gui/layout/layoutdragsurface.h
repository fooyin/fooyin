/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
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

#include "layoututils.h"

#include <QPointF>
#include <QUndoCommand>
#include <QWidget>

namespace Fooyin {
class LayoutDragController;

class LayoutDragSurface : public QWidget
{
    Q_OBJECT

public:
    explicit LayoutDragSurface(LayoutDragController* controller, QWidget* parent);

    void reset();

    void updateTarget(const QPointF& position, Qt::KeyboardModifiers modifiers);
    [[nodiscard]] bool hasValidTarget() const;

    [[nodiscard]] std::unique_ptr<QUndoCommand> dropCommand(const QPointF& position, Qt::KeyboardModifiers modifiers);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    [[nodiscard]] LayoutDropTarget dropTarget(const QPointF& position, Qt::KeyboardModifiers modifiers) const;

    LayoutDragController* m_controller;
    LayoutDropTarget m_target;
};
} // namespace Fooyin
