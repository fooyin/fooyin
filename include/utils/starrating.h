/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <luket@pm.me>
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

#include <QColor>
#include <QMetaType>
#include <QPixmap>
#include <QPolygonF>
#include <QSize>
#include <QVariant>

#include <array>

class QPainter;
class QPalette;
class QRect;

namespace Fooyin {
using RatingStarColours = std::array<QColor, 5>;

class FYUTILS_EXPORT StarRating
{
public:
    enum class EditMode : uint8_t
    {
        Editable,
        ReadOnly
    };

    StarRating();
    StarRating(float rating, int maxStarCount);
    StarRating(float rating, int maxStarCount, int scale);
    StarRating(float rating, int maxStarCount, int scale, const RatingStarColours& colours);
    StarRating(float rating, int maxStarCount, int scale, const RatingStarColours& colours,
               const QColor& unratedColour);

    [[nodiscard]] float rating() const;
    [[nodiscard]] int maxStarCount() const;
    [[nodiscard]] int starScale() const;

    void setRating(float rating);
    void setMaxStarCount(int maxStarCount);
    void setStarScale(int scale);

    void paint(QPainter* painter, const QRect& rect, const QPalette& palette, EditMode mode,
               Qt::Alignment alignment = Qt::AlignLeft, bool selected = false) const;
    [[nodiscard]] QSize sizeHint() const;

    operator QVariant() const
    {
        return QVariant::fromValue(*this);
    }

private:
    QPolygonF m_starPolygon;
    float m_rating;
    int m_maxCount;
    int m_scale;
    RatingStarColours m_colours;
    QColor m_unratedColour;
};
} // namespace Fooyin

Q_DECLARE_METATYPE(Fooyin::StarRating)
