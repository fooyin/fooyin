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

#include <gui/brushsource.h>

#include <QAbstractButton>
#include <QIcon>

class QPainter;

namespace Fooyin {
void paintBrushSwatch(QPainter& painter, const BrushSource& brush, const QPalette& palette, QPalette::ColorGroup group,
                      const QRectF& bounds);
QIcon brushSourceIcon(const BrushSource& brush, const QPalette& palette, QPalette::ColorGroup group, qreal ratio,
                      const QSize& size = QSize{16, 12});

class SwatchButton : public QAbstractButton
{
public:
    explicit SwatchButton(QWidget* parent);

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void paintBackground(QPainter& painter) const;
    void paintFocus(QPainter& painter) const;
};
} // namespace Fooyin
