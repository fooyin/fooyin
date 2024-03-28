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
class FYUTILS_EXPORT ColourButton : public QWidget
{
    Q_OBJECT

public:
    explicit ColourButton(QWidget* parent = nullptr);
    explicit ColourButton(const QColor& colour, QWidget* parent = nullptr);

    [[nodiscard]] QColor colour() const;
    [[nodiscard]] bool colourChanged() const;

    void setColour(const QColor& colour);

signals:
    void clicked();

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    void pickColour();

    QColor m_colour;
    bool m_changed;
};
} // namespace Fooyin
