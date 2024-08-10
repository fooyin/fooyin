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
#include <QPixmapCache>

namespace {
void drawHalfPolygon(QPainter* painter, const QPolygonF& polygon, bool drawLeftHalf)
{
    QRectF clipRect;
    if(drawLeftHalf) {
        clipRect = QRectF{polygon.boundingRect().topLeft(),
                          QPointF{polygon.boundingRect().center().x(), polygon.boundingRect().bottom()}};
    }
    else {
        clipRect = QRectF{QPointF{polygon.boundingRect().center().x(), polygon.boundingRect().top()},
                          polygon.boundingRect().bottomRight()};
    }

    const QPolygonF clippedPolygon = polygon.intersected(QPolygonF(clipRect));

    painter->setPen(Qt::NoPen);
    painter->drawPolygon(clippedPolygon, Qt::WindingFill);
}
} // namespace

namespace Fooyin {
StarRating::StarRating()
    : StarRating{0, 5}
{ }

StarRating::StarRating(float rating, int maxStarCount)
    : m_rating{rating}
    , m_maxCount{maxStarCount}
    , m_scale{20}
{
    double angle{-0.314};
    for(int i{0}; i < 5; ++i) {
        m_starPolygon.append(QPointF(0.5 + (0.5 * std::cos(angle)), 0.5 + (0.5 * std::sin(angle))));
        angle += 2.513;
    }
}

float StarRating::rating() const
{
    return m_rating;
}

int StarRating::maxStarCount() const
{
    return m_maxCount;
}

int StarRating::starScale() const
{
    return m_scale;
}

void StarRating::setRating(float rating)
{
    m_rating = rating;
}

void StarRating::setMaxStarCount(int maxStarCount)
{
    m_maxCount = maxStarCount;
}

void StarRating::setStarScale(int scale)
{
    m_scale = scale;
}

void StarRating::paint(QPainter* painter, const QRect& rect, const QPalette& palette, EditMode mode,
                       Qt::Alignment alignment) const
{
    const QString cacheKey = QStringLiteral("StarRating:%1|%2|%3|%4|%5|%6")
                                 .arg(m_rating)
                                 .arg(m_scale)
                                 .arg(m_maxCount)
                                 .arg(mode == EditMode::Editable ? 1 : 0)
                                 .arg(rect.width())
                                 .arg(alignment);

    QPixmap pixmap;
    if(!QPixmapCache::find(cacheKey, &pixmap)) {
        pixmap = QPixmap{rect.size()};
        pixmap.fill(Qt::transparent);

        QPainter pixmapPainter(&pixmap);
        pixmapPainter.setRenderHint(QPainter::Antialiasing, true);

        const QBrush brush{mode == EditMode::Editable ? palette.highlight() : palette.windowText()};

        QBrush fadedBrush{brush};
        QColor fadedColour{brush.color()};
        fadedColour.setAlphaF(0.2F);
        fadedBrush.setColor(fadedColour);

        const int yOffset = (rect.height() - m_scale) / 2;

        int xOffset{0};
        const int totalWidth = m_maxCount * m_scale;
        if(alignment & Qt::AlignHCenter) {
            xOffset = (rect.width() - totalWidth) / 2;
        }
        else if(alignment & Qt::AlignRight) {
            xOffset = rect.width() - totalWidth;
        }

        pixmapPainter.translate(xOffset, yOffset);
        pixmapPainter.scale(m_scale, m_scale);

        const int fullStars     = std::floor(m_rating * static_cast<float>(m_maxCount));
        const float partialStar = (m_rating * static_cast<float>(m_maxCount)) - static_cast<float>(fullStars);

        for(int i{0}; i < m_maxCount; ++i) {
            if(i < fullStars) {
                // Draw full star
                pixmapPainter.setPen(Qt::NoPen);
                pixmapPainter.setBrush(brush);
                pixmapPainter.drawPolygon(m_starPolygon, Qt::WindingFill);
            }
            else {
                pixmapPainter.setPen(Qt::NoPen);
                pixmapPainter.setBrush(fadedBrush);
                pixmapPainter.drawPolygon(m_starPolygon, Qt::WindingFill);
            }

            if(i == fullStars && partialStar >= 0.5) {
                // Draw half star
                pixmapPainter.setBrush(brush);
                drawHalfPolygon(&pixmapPainter, m_starPolygon, true);
            }

            pixmapPainter.translate(1.0, 0.0);
        }

        pixmapPainter.end();

        QPixmapCache::insert(cacheKey, pixmap);
    }

    painter->drawPixmap(rect, pixmap);
}

QSize StarRating::sizeHint() const
{
    return m_scale * QSize{m_maxCount, 1};
}
} // namespace Fooyin
