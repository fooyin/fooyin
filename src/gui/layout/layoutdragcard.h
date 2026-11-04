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

#include <QIcon>
#include <QPixmap>
#include <QString>
#include <QWidget>

namespace Fooyin {
class LayoutDragCard : public QWidget
{
    Q_OBJECT

public:
    explicit LayoutDragCard(QWidget* parent);

    void setContent(const QString& text, bool creating, QPixmap snapshot);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QIcon m_icon;
    QString m_text;
    QPixmap m_snapshot;
    int m_headerHeight;
};
} // namespace Fooyin