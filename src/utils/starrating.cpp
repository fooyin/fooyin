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

#include <QFont>
#include <QFontMetricsF>
#include <QPainter>
#include <QPalette>
#include <QPixmapCache>
#include <QPolygonF>
#include <QSharedData>

#include <cmath>

using namespace Qt::StringLiterals;

namespace Fooyin {
namespace {
struct StarBrushes
{
    QBrush filled;
    QBrush faded;
};

StarBrushes getStarBrushes(const QPalette& palette, StarRating::EditMode mode, bool selected,
                           const QColor& customColour, const QColor& unratedColour)
{
    const QBrush filled = customColour.isValid() ? QBrush{customColour}
                        : mode == StarRating::EditMode::Editable
                            ? palette.highlight()
                            : (selected ? palette.highlightedText() : palette.text());

    QBrush faded;
    if(unratedColour.isValid()) {
        faded = unratedColour;
    }
    else {
        QColor fadedColour{filled.color()};
        fadedColour.setAlphaF(fadedColour.alphaF() * 0.2);
        faded = fadedColour;
    }

    return {.filled = filled, .faded = faded};
}
const QPolygonF& starPolygon()
{
    static const QPolygonF Polygon = []() {
        QPolygonF polygon;
        double angle{-0.314};
        for(int i{0}; i < 5; ++i) {
            polygon.append(QPointF{0.5 + (0.5 * std::cos(angle)), 0.5 + (0.5 * std::sin(angle))});
            angle += 2.513;
        }
        return polygon;
    }();
    return Polygon;
}
} // namespace

class StarRatingPrivate : public QSharedData
{
public:
    StarRatingPrivate(float rating, int maxStarCount, int scale, const RatingStarColours& colours,
                      const QColor& unratedColour, std::optional<RatingStarSymbols> symbols);

    void paint(QPainter* painter, const QRect& rect, const QPalette& palette, StarRating::EditMode mode,
               Qt::Alignment alignment, bool selected) const;
    [[nodiscard]] QSize sizeHint() const;

    float m_rating{0.0};
    int m_maxCount{5};
    int m_scale{17};
    RatingStarColours m_colours;
    std::optional<RatingStarSymbols> m_ratingSymbols;
    QFont m_symbolFont{};
    QColor m_unratedColour;
    Qt::Orientation m_orientation{Qt::Horizontal};
};

StarRatingPrivate::StarRatingPrivate(float rating, int maxStarCount, int scale, const RatingStarColours& colours,
                                     const QColor& unratedColour, std::optional<RatingStarSymbols> symbols)
    : m_rating{rating}
    , m_maxCount{maxStarCount}
    , m_scale{scale}
    , m_colours{colours}
    , m_ratingSymbols{std::move(symbols)}
    , m_unratedColour{unratedColour}
{ }

void StarRatingPrivate::paint(QPainter* painter, const QRect& rect, const QPalette& palette, StarRating::EditMode mode,
                              Qt::Alignment alignment, bool selected) const
{
    if(rect.isEmpty() || m_scale <= 0 || m_maxCount <= 0) {
        return;
    }

    auto symbols{m_ratingSymbols};
    if(symbols) {
        if(symbols->fullStarSymbol.isEmpty()) {
            symbols->fullStarSymbol = defaultRatingFullStarSymbol();
        }
        if(symbols->halfStarSymbol.isEmpty()) {
            symbols->halfStarSymbol = defaultRatingHalfStarSymbol();
        }
        if(symbols->emptyStarSymbol.isEmpty()) {
            symbols->emptyStarSymbol = defaultRatingEmptyStarSymbol();
        }
    }

    const int colourIndex = std::clamp(static_cast<int>(std::ceil(m_rating * static_cast<float>(m_maxCount))) - 1, 0,
                                       static_cast<int>(m_colours.size()) - 1);
    const QColor customColour = m_rating > 0 ? m_colours.at(colourIndex) : QColor{};
    const auto brushes        = getStarBrushes(palette, mode, selected, customColour, m_unratedColour);
    QFont font{m_symbolFont.resolve(painter->font())};
    font.setPixelSize(m_scale);

    const qreal dpr  = painter->device()->devicePixelRatioF();
    QString cacheKey = u"StarRating:%1|%2|%3|%4|%5|%6|%7"_s.arg(m_rating)
                           .arg(m_scale)
                           .arg(m_orientation)
                           .arg(m_maxCount)
                           .arg(mode == StarRating::EditMode::Editable ? 1 : 0)
                           .arg(rect.width())
                           .arg(rect.height())
                     + u"|%1|%2|%3|%4"_s.arg(alignment.toInt())
                           .arg(brushes.filled.color().name(QColor::HexArgb))
                           .arg(brushes.faded.color().name(QColor::HexArgb))
                           .arg(dpr);
    if(symbols) {
        cacheKey += u"|%1:%2|%3:%4|%5:%6|%7"_s.arg(
            QString::number(symbols->fullStarSymbol.size()), symbols->fullStarSymbol,
            QString::number(symbols->halfStarSymbol.size()), symbols->halfStarSymbol,
            QString::number(symbols->emptyStarSymbol.size()), symbols->emptyStarSymbol, font.key());
    }
    else {
        cacheKey += u"|painted"_s;
    }

    QPixmap pixmap;
    if(!QPixmapCache::find(cacheKey, &pixmap)) {
        pixmap = QPixmap{rect.size() * dpr};
        pixmap.setDevicePixelRatio(dpr);
        pixmap.fill(Qt::transparent);

        std::array<QString, 3> symbolText;
        std::array<QRectF, 3> symbolBounds;
        qreal symbolSize{1.0};
        if(symbols) {
            symbolText = {symbols->fullStarSymbol, symbols->halfStarSymbol, symbols->emptyStarSymbol};
            const QFontMetricsF fontMetrics{font, painter->device()};
            for(size_t i{0}; i < symbolText.size(); ++i) {
                symbolBounds[i] = fontMetrics.tightBoundingRect(symbolText[i]);
                symbolSize      = std::max({symbolSize, symbolBounds[i].width(), symbolBounds[i].height()});
            }
        }

        QPainter pixmapPainter{&pixmap};
        pixmapPainter.setRenderHint(QPainter::Antialiasing, true);
        if(symbols) {
            pixmapPainter.setRenderHint(QPainter::TextAntialiasing, true);
            pixmapPainter.setFont(font);
        }

        const auto drawSymbol = [&](size_t index, const QBrush& brush) {
            pixmapPainter.save();
            if(symbols) {
                pixmapPainter.setPen(QPen{brush, 1.0});
                pixmapPainter.translate(m_scale / 2.0, m_scale / 2.0);
                pixmapPainter.scale(m_scale / symbolSize, m_scale / symbolSize);
                pixmapPainter.drawText(-symbolBounds[index].center(), symbolText[index]);
            }
            else {
                pixmapPainter.setPen(Qt::NoPen);
                pixmapPainter.setBrush(brush);
                pixmapPainter.scale(m_scale, m_scale);
                pixmapPainter.drawPolygon(starPolygon(), Qt::WindingFill);
            }
            pixmapPainter.restore();
        };

        const bool vertical  = m_orientation == Qt::Vertical;
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

        const int fullStars     = std::floor(m_rating * static_cast<float>(m_maxCount));
        const float partialStar = (m_rating * static_cast<float>(m_maxCount)) - static_cast<float>(fullStars);
        for(int i{0}; i < m_maxCount; ++i) {
            const bool halfStar = i == fullStars && partialStar >= 0.5F;
            if(i < fullStars) {
                drawSymbol(0, brushes.filled);
            }
            else if(halfStar && symbols) {
                drawSymbol(1, brushes.filled);
            }
            else {
                drawSymbol(2, brushes.faded);
                if(halfStar) {
                    pixmapPainter.save();
                    pixmapPainter.setClipRect(QRectF{0, 0, m_scale / 2.0, static_cast<qreal>(m_scale)});
                    drawSymbol(0, brushes.filled);
                    pixmapPainter.restore();
                }
            }
            pixmapPainter.translate(vertical ? 0 : m_scale, vertical ? -m_scale : 0);
        }
        pixmapPainter.end();
        QPixmapCache::insert(cacheKey, pixmap);
    }

    painter->drawPixmap(rect.topLeft(), pixmap);
}

QSize StarRatingPrivate::sizeHint() const
{
    return m_scale * (m_orientation == Qt::Vertical ? QSize{1, m_maxCount} : QSize{m_maxCount, 1});
}

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
                       const QColor& unratedColour, std::optional<RatingStarSymbols> symbols)
    : p{new StarRatingPrivate(rating, maxStarCount, scale, colours, unratedColour, std::move(symbols))}
{ }

StarRating::~StarRating()                                      = default;
StarRating::StarRating(const StarRating& other)                = default;
StarRating& StarRating::operator=(const StarRating& other)     = default;
StarRating::StarRating(StarRating&& other) noexcept            = default;
StarRating& StarRating::operator=(StarRating&& other) noexcept = default;

float StarRating::rating() const
{
    return p->m_rating;
}

int StarRating::maxStarCount() const
{
    return p->m_maxCount;
}

int StarRating::starScale() const
{
    return p->m_scale;
}

Qt::Orientation StarRating::orientation() const
{
    return p->m_orientation;
}

void StarRating::setRating(float rating)
{
    p->m_rating = rating;
}

void StarRating::setMaxStarCount(int maxStarCount)
{
    p->m_maxCount = maxStarCount;
}

void StarRating::setStarScale(int scale)
{
    p->m_scale = scale;
}

void StarRating::setOrientation(Qt::Orientation orientation)
{
    p->m_orientation = orientation;
}

void StarRating::setSymbolFont(const QFont& font)
{
    p->m_symbolFont = font;
}

void StarRating::paint(QPainter* painter, const QRect& rect, const QPalette& palette, EditMode mode,
                       Qt::Alignment alignment, bool selected) const
{
    p->paint(painter, rect, palette, mode, alignment, selected);
}

QSize StarRating::sizeHint() const
{
    return p->sizeHint();
}
} // namespace Fooyin
