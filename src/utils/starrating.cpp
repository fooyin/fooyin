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

#include <utils/starrating.h>

#include <QPainter>
#include <QPalette>
#include <QPixmapCache>

using namespace Qt::StringLiterals;

namespace Fooyin {
StarRating::StarRating()
    : StarRating{0, 5}
{ }

StarRating::StarRating(float rating, int maxStarCount)
    : StarRating{rating, maxStarCount, 17}
{ }

StarRating::StarRating(float rating, int maxStarCount, int scale)
    : StarRating{rating, maxStarCount, scale, {}}
{ }

StarRating::StarRating(float rating, int maxStarCount, int scale, const RatingStarColours& colours)
    : StarRating{rating, maxStarCount, scale, colours, {}}
{ }

StarRating::StarRating(float rating, int maxStarCount, int scale, const RatingStarColours& colours,
                       const QColor& unratedColour)
    : m_rating{rating}
    , m_maxCount{maxStarCount}
    , m_scale{scale}
    , m_colours{colours}
    , m_unratedColour{unratedColour}
    , m_orientation{Qt::Horizontal}
{ }

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

Qt::Orientation StarRating::orientation() const
{
    return m_orientation;
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

void StarRating::setOrientation(Qt::Orientation orientation)
{
    m_orientation = orientation;
}

void StarRating::paint(QPainter* painter, const QRect& rect, EditMode mode, Qt::Alignment alignment) const
{
    RatingStarSymbols symbols{m_ratingSymbols};

    if(symbols.fullStarSymbol.isEmpty()) {
        symbols.fullStarSymbol = defaultRatingFullStarSymbol();
    }
    if(symbols.halfStarSymbol.isEmpty()) {
        symbols.halfStarSymbol = defaultRatingHalfStarSymbol();
    }
    if(symbols.emptyStarSymbol.isEmpty()) {
        symbols.emptyStarSymbol = defaultRatingEmptyStarSymbol();
    }

    const int colourIndex = std::clamp(static_cast<int>(std::ceil(m_rating * static_cast<float>(m_maxCount))) - 1, 0,
                                       static_cast<int>(m_colours.size()) - 1);
    const QColor customColour = m_rating > 0 ? m_colours.at(colourIndex) : QColor{};
    const qreal dpr           = painter->device()->devicePixelRatioF();
    const QString cacheKey    = u"StarRating:%1|%2|%3|%4|%5|%6|%7"_s.arg(m_rating)
                                    .arg(m_scale)
                                    .arg(m_orientation)
                                    .arg(m_maxCount)
                                    .arg(mode == EditMode::Editable ? 1 : 0)
                                    .arg(rect.width())
                                    .arg(rect.height())
                              + u"|%1|%2|%3|%4"_s.arg(alignment.toInt())
                                    .arg(brushes.filled.color().name(QColor::HexArgb))
                                    .arg(brushes.faded.color().name(QColor::HexArgb))
                                    .arg(dpr);

    QPixmap pixmap;
    if(!QPixmapCache::find(cacheKey, &pixmap)) {
        pixmap = QPixmap{rect.size() * dpr};
        pixmap.setDevicePixelRatio(dpr);
        pixmap.fill(Qt::transparent);

        QPainter pixmapPainter(&pixmap);
        pixmapPainter.setRenderHint(QPainter::Antialiasing, true);
        pixmapPainter.setFont(QFont(QString{u"Arial"}, m_scale));
        pixmapPainter.setPen(customColour);

        const bool vertical = m_orientation == Qt::Vertical;
        const int yOffset   = (rect.height() - m_scale) * 0.5 + m_scale;

        const QSize starSize = sizeHint();
        int xOffset          = (rect.width() - starSize.width()) / 2;
        int yOffset          = (rect.height() - starSize.height()) / 2;
        if(vertical) {
            if(alignment & Qt::AlignTop) {
                yOffset = 0;
            }
            else if(alignment & Qt::AlignBottom) {
                yOffset = rect.height() - starSize.height();
            }
        }
        else {
            if(alignment & Qt::AlignLeft) {
                xOffset = 0;
            }
            else if(alignment & Qt::AlignRight) {
                xOffset = rect.width() - starSize.width();
            }
        }

        pixmapPainter.translate(xOffset, vertical ? yOffset + starSize.height() - m_scale : yOffset);
        pixmapPainter.scale(m_scale, m_scale);

        const int fullStars     = std::floor(m_rating * static_cast<float>(m_maxCount));
        const float partialStar = (m_rating * static_cast<float>(m_maxCount)) - static_cast<float>(fullStars);

        for(int i{0}; i < m_maxCount; ++i) {
            if(i < fullStars) {
                pixmapPainter.drawText(QPointF(xOffset, yOffset), symbols.fullStarSymbol);
            }
            else if(i == fullStars && partialStar >= 0.5) {
                pixmapPainter.drawText(QPointF(xOffset, yOffset), symbols.halfStarSymbol);
            }
            else {
                pixmapPainter.drawText(QPointF(xOffset, yOffset), symbols.emptyStarSymbol);
            }

            if(i == fullStars && partialStar >= 0.5) {
                // Draw half star
                pixmapPainter.setBrush(brushes.filled);
                drawHalfPolygon(&pixmapPainter, m_starPolygon, true);
            }

            pixmapPainter.translate(vertical ? 0.0 : 1.0, vertical ? -1.0 : 0.0);
            xOffset = xOffset + m_scale;
        }
        pixmapPainter.end();

        QPixmapCache::insert(cacheKey, pixmap);
    }

    painter->drawPixmap(rect.topLeft(), pixmap);
}

QSize StarRating::sizeHint() const
{
    return m_scale * (m_orientation == Qt::Vertical ? QSize{1, m_maxCount} : QSize{m_maxCount, 1});
}
} // namespace Fooyin
