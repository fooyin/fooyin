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

#include <utils/starrating.h>

#include <QWidget>

namespace Fooyin {
class FYUTILS_EXPORT StarEditor : public QWidget
{
    Q_OBJECT

public:
    explicit StarEditor(QWidget* parent = nullptr);

    [[nodiscard]] StarRating rating() const;
    void setRating(const StarRating& rating);

    [[nodiscard]] QSize sizeHint() const override;

signals:
    void editingFinished();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    [[nodiscard]] int starAtPosition(int x) const;

    StarRating m_rating;
};
} // namespace Fooyin
