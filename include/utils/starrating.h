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

#include <QMetaType>
#include <QPolygonF>
#include <QSize>

class QPainter;
class QPalette;
class QRect;

namespace Fooyin {
class FYUTILS_EXPORT StarRating
{
public:
    enum class EditMode : uint8_t
    {
        Editable,
        ReadOnly
    };

    StarRating();
    StarRating(int starCount, int maxStarCount);
    StarRating(int starCount, int maxStarCount, bool alwaysDisplay);

    [[nodiscard]] int starCount() const;
    [[nodiscard]] int maxStarCount() const;

    void setStarCount(int starCount);
    void setMaxStarCount(int maxStarCount);
    void setAlwaysDisplay(bool display);

    void paint(QPainter* painter, const QRect& rect, const QPalette& palette, EditMode mode) const;
    [[nodiscard]] QSize sizeHint() const;

private:
    QPolygonF m_starPolygon;
    int m_count;
    int m_maxCount;
    bool m_alwaysDisplay;
};
} // namespace Fooyin

Q_DECLARE_METATYPE(Fooyin::StarRating)
