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

#include <utils/starrating.h>

#include <QPainter>
#include <QPalette>

constexpr int StarScaleFactor = 20;

namespace Fooyin {
StarRating::StarRating()
    : StarRating{0, 5}
{ }

StarRating::StarRating(int starCount, int maxStarCount)
    : m_count{starCount}
    , m_maxCount{maxStarCount}
{
    double angle{-0.314};
    for(int i{0}; i < 5; ++i) {
        m_starPolygon.append(QPointF(0.5 + (0.5 * std::cos(angle)), 0.5 + (0.5 * std::sin(angle))));
        angle += 2.513;
    }
}

int StarRating::starCount() const
{
    return m_count;
}

int StarRating::maxStarCount() const
{
    return m_maxCount;
}

void StarRating::setStarCount(int starCount)
{
    m_count = starCount;
}

void StarRating::setMaxStarCount(int maxStarCount)
{
    m_maxCount = maxStarCount;
}

void StarRating::paint(QPainter* painter, const QRect& rect, const QPalette& palette, EditMode mode) const
{
    painter->save();

    painter->setRenderHint(QPainter::Antialiasing, true);

    const QBrush brush{mode == EditMode::Editable ? palette.highlight() : palette.windowText()};
    painter->setPen(Qt::NoPen);
    painter->setBrush(brush);

    QPen dotPen{brush, 0.2};
    dotPen.setCapStyle(Qt::RoundCap);

    const int yOffset = (rect.height() - StarScaleFactor) / 2;
    painter->translate(rect.x(), rect.y() + yOffset);
    painter->scale(StarScaleFactor, StarScaleFactor);

    for(int i{0}; i < m_maxCount; ++i) {
        if(i < m_count) {
            painter->setPen(Qt::NoPen);
            painter->drawPolygon(m_starPolygon, Qt::WindingFill);
        }
        else if(mode == EditMode::Editable) {
            painter->setPen(dotPen);
            painter->drawPoint(QPointF{0.5, 0.5});
        }

        painter->translate(1.0, 0.0);
    }

    painter->restore();
}

QSize StarRating::sizeHint() const
{
    return StarScaleFactor * QSize{m_maxCount, 1};
}
} // namespace Fooyin
