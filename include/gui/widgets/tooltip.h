/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <LukeT1@proton.me>
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

#include <QWidget>

namespace Fooyin {
class FYUTILS_EXPORT ToolTip : public QWidget
{
    Q_OBJECT

public:
    explicit ToolTip(QWidget* parent = nullptr);

    void setText(const QString& text);
    void setSubtext(const QString& text);
    void setPosition(const QPoint& pos);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    void redraw();

    QString m_text;
    QString m_subText;
    QPoint m_pos;

    QFont m_font;
    QFont m_subFont;

    QPixmap m_pixmap;
    QPixmap m_cachedBg;
};
} // namespace Fooyin
