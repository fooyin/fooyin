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

#include <QColor>
#include <QPoint>
#include <QPointer>
#include <QWidget>

namespace Fooyin {
class LayoutPanelOverlay : public QWidget
{
    Q_OBJECT

public:
    explicit LayoutPanelOverlay(QWidget* parent, LayoutPanelOverlay* outline = nullptr);

    void setHovered(bool hovered);

Q_SIGNALS:
    void dragRequested();
    void selectionRequested();

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    QPoint m_pressPosition;
    QPointer<LayoutPanelOverlay> m_outline;
    bool m_pressed;
    bool m_hovered;
};

class LayoutStackGrip : public LayoutPanelOverlay
{
    Q_OBJECT

public:
    explicit LayoutStackGrip(QWidget* parent, LayoutPanelOverlay* outline = nullptr);

    [[nodiscard]] QSize sizeHint() const override;

    void setBackgroundColour(const QColor& colour);
    void setOrientation(Qt::Orientation orientation);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QColor m_bgColour;
    Qt::Orientation m_orientation;
};
} // namespace Fooyin
