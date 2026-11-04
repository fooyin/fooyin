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

#include "artworkreflection.h"

#include <gui/guiutils.h>

#include <QLinearGradient>
#include <QPainter>
#include <QPixmapCache>

#include <cmath>

namespace Fooyin::Gui {
int artworkReflectionHeight(int artworkHeight, const ArtworkReflectionOptions& options)
{
    return std::max(1, static_cast<int>(std::lround(artworkHeight * options.height / 100.0)));
}

void drawArtworkReflection(QPainter& painter, const QPixmap& source, const QPixmap& scaledImage,
                           const QRect& artworkRect, int cornerRadiusPercent, const ArtworkReflectionOptions& options)
{
    if(!options.enabled || options.opacity <= 0 || source.isNull() || scaledImage.isNull() || artworkRect.isEmpty()) {
        return;
    }
    const int height       = artworkReflectionHeight(artworkRect.height(), options);
    const qreal dpr        = scaledImage.devicePixelRatio();
    const QString cacheKey = QStringLiteral("fy_artwork_reflection|%1|%2|%3|%4|%5|%6|%7")
                                 .arg(source.cacheKey())
                                 .arg(scaledImage.width())
                                 .arg(scaledImage.height())
                                 .arg(dpr)
                                 .arg(height)
                                 .arg(cornerRadiusPercent)
                                 .arg(options.opacity);

    QPixmap reflection;
    if(!QPixmapCache::find(cacheKey, &reflection)) {
        reflection = QPixmap{scaledImage.width(), static_cast<int>(std::ceil(height * dpr))};
        reflection.setDevicePixelRatio(dpr);
        reflection.fill(Qt::transparent);

        QPainter reflectionPainter{&reflection};
        reflectionPainter.translate(0, artworkRect.height());
        reflectionPainter.scale(1, -1);
        drawRoundedPixmap(reflectionPainter, {0, 0, artworkRect.width(), artworkRect.height()},
                          Qt::AlignLeft | Qt::AlignTop, scaledImage, cornerRadiusPercent);
        reflectionPainter.resetTransform();
        reflectionPainter.setCompositionMode(QPainter::CompositionMode_DestinationIn);

        QLinearGradient fade{0, 0, 0, static_cast<qreal>(height)};
        fade.setColorAt(0, QColor{0, 0, 0, static_cast<int>(std::lround(options.opacity * 255.0 / 100))});
        fade.setColorAt(1, Qt::transparent);
        reflectionPainter.fillRect(QRectF{QPointF{}, reflection.deviceIndependentSize()}, fade);
        reflectionPainter.end();

        QPixmapCache::insert(cacheKey, reflection);
    }

    painter.drawPixmap(artworkRect.left(), artworkRect.bottom() + 1 + options.gap, reflection);
}

} // namespace Fooyin::Gui
