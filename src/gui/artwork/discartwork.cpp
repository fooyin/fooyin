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

#include "discartwork.h"

#include <gui/guiutils.h>

#include <QConicalGradient>
#include <QPainter>
#include <QRadialGradient>

#include <cmath>

namespace Fooyin::Gui {
namespace {
void paintCD(QPainter& painter, const QPixmap& artwork, const QRectF& outer)
{
    const QPointF centre  = outer.center();
    const qreal radius    = outer.width() / 2.0;
    const qreal hubRadius = radius * 0.28;

    painter.drawPixmap(0, 0, artwork);

    QConicalGradient sheen{centre, 30.0};
    sheen.setColorAt(0.0, QColor{255, 110, 90, 35});
    sheen.setColorAt(0.16, QColor{255, 235, 120, 45});
    sheen.setColorAt(0.32, QColor{100, 255, 185, 35});
    sheen.setColorAt(0.48, QColor{235, 250, 255, 65});
    sheen.setColorAt(0.58, QColor{110, 170, 255, 40});
    sheen.setColorAt(0.74, QColor{230, 130, 255, 45});
    sheen.setColorAt(0.9, QColor{255, 225, 245, 65});
    sheen.setColorAt(1.0, QColor{255, 110, 90, 35});
    painter.fillRect(artwork.rect(), sheen);

    QRadialGradient rim{centre, radius};
    rim.setColorAt(0.0, Qt::transparent);
    rim.setColorAt(0.965, Qt::transparent);
    rim.setColorAt(0.98, QColor{225, 230, 240, 110});
    rim.setColorAt(0.99, QColor{255, 255, 255, 210});
    rim.setColorAt(1.0, QColor{65, 70, 80, 160});
    painter.fillRect(artwork.rect(), rim);

    QRadialGradient hub{centre, hubRadius};
    hub.setColorAt(0.0, QColor{225, 228, 235, 230});
    hub.setColorAt(0.48, QColor{250, 251, 255, 245});
    hub.setColorAt(0.52, QColor{180, 185, 195, 210});
    hub.setColorAt(0.78, QColor{225, 228, 235, 220});
    hub.setColorAt(0.8, QColor{250, 251, 255, 240});
    hub.setColorAt(0.94, QColor{210, 215, 225, 220});
    hub.setColorAt(0.97, QColor{65, 70, 80, 220});
    hub.setColorAt(1.0, QColor{240, 242, 248, 220});
    painter.setPen(Qt::NoPen);
    painter.setBrush(hub);
    painter.drawEllipse(centre, hubRadius, hubRadius);
}

void paintVinyl(QPainter& painter, const QPixmap& artwork, const QRectF& outer)
{
    const QPointF centre    = outer.center();
    const qreal radius      = outer.width() / 2.0;
    const qreal labelRadius = radius * 0.33;

    QRadialGradient surface{centre, radius};
    surface.setColorAt(0.0, QColor{28, 28, 30});
    surface.setColorAt(0.4, QColor{22, 22, 24});
    surface.setColorAt(0.96, QColor{14, 14, 16});
    surface.setColorAt(1.0, QColor{50, 50, 52});
    painter.setPen(Qt::NoPen);
    painter.setBrush(surface);
    painter.drawEllipse(outer);

    const qreal grooveSpacing     = std::max(3.0, radius * 0.016);
    const qreal firstGrooveRadius = radius * 0.4;
    const int grooveCount         = static_cast<int>(std::ceil(((radius * 0.96) - firstGrooveRadius) / grooveSpacing));

    painter.setPen(QPen{QColor{255, 255, 255, 12}, 1.0});
    painter.setBrush(Qt::NoBrush);
    for(int grooveIndex{0}; grooveIndex < grooveCount; ++grooveIndex) {
        const qreal grooveRadius = firstGrooveRadius + (grooveIndex * grooveSpacing);
        painter.drawEllipse(centre, grooveRadius, grooveRadius);
    }

    QConicalGradient sheen{centre, 30.0};
    sheen.setColorAt(0.0, Qt::transparent);
    sheen.setColorAt(0.12, QColor{255, 255, 255, 5});
    sheen.setColorAt(0.18, QColor{255, 255, 255, 28});
    sheen.setColorAt(0.24, QColor{255, 255, 255, 5});
    sheen.setColorAt(0.5, Qt::transparent);
    sheen.setColorAt(0.62, QColor{255, 255, 255, 5});
    sheen.setColorAt(0.68, QColor{255, 255, 255, 20});
    sheen.setColorAt(0.74, QColor{255, 255, 255, 5});
    sheen.setColorAt(1.0, Qt::transparent);
    painter.setPen(Qt::NoPen);
    painter.setBrush(sheen);
    painter.drawEllipse(outer);

    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen{QColor{255, 255, 255, 18}, 0.8});
    painter.drawEllipse(centre, radius * 0.365, radius * 0.365);

    QPainterPath label;
    label.addEllipse(centre, labelRadius, labelRadius);
    painter.save();
    painter.setClipPath(label);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.drawPixmap(QRectF{centre.x() - labelRadius, centre.y() - labelRadius, labelRadius * 2.0, labelRadius * 2.0},
                       artwork, artwork.rect());
    painter.restore();
    painter.setPen(QPen{QColor{0, 0, 0, 120}, 1.0});
    painter.drawPath(label);
}
} // namespace

QPainterPath discArtworkPath(const QRectF& rect, CoverStyle style)
{
    const QRectF outer     = rect.adjusted(1.0, 1.0, -1.0, -1.0);
    const qreal radius     = outer.width() / 2.0;
    const qreal holeRadius = radius * (style == CoverStyle::Vinyl ? 0.024 : 0.132);

    QPainterPath outline;
    outline.addEllipse(outer);
    outline.addEllipse(outer.center(), holeRadius, holeRadius);
    return outline;
}

QPixmap renderDiscArtwork(const QPixmap& cover, const QSize& size, qreal dpr, CoverStyle style)
{
    const int diameter = std::min(size.width(), size.height());
    if(diameter <= 2) {
        return {};
    }

    const QPixmap artwork
        = scaleArtworkPixmap(cover, QSize{diameter, diameter}, dpr, ArtworkScaling::Fill, Qt::AlignCenter);
    if(artwork.isNull()) {
        return {};
    }

    QPixmap disc{artwork.size()};
    disc.setDevicePixelRatio(artwork.devicePixelRatio());
    disc.fill(Qt::transparent);

    const QRectF rect{QPointF{}, disc.deviceIndependentSize()};
    const QRectF outer = rect.adjusted(1.0, 1.0, -1.0, -1.0);

    QPainter painter{&disc};
    painter.setRenderHint(QPainter::Antialiasing);
    if(style == CoverStyle::Vinyl) {
        paintVinyl(painter, artwork, outer);
    }
    else {
        paintCD(painter, artwork, outer);
    }

    QPixmap mask{disc.size()};
    mask.setDevicePixelRatio(disc.devicePixelRatio());
    mask.fill(Qt::transparent);
    QPainter maskPainter{&mask};
    maskPainter.setRenderHint(QPainter::Antialiasing);
    maskPainter.fillPath(discArtworkPath(rect, style), Qt::white);
    maskPainter.end();
    painter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
    painter.drawPixmap(0, 0, mask);
    return disc;
}
} // namespace Fooyin::Gui
