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

#include "styleutils.h"

#include "fooyinstyle.h"

#include <QPainter>
#include <QPainterPath>
#include <QPixmapCache>

#include <array>
#include <cmath>

using namespace Qt::StringLiterals;

namespace {
QColor mix(const QColor& base, const QColor& foreground, float amount)
{
    amount *= foreground.alphaF();
    return QColor::fromRgbF(std::lerp(base.redF(), foreground.redF(), amount),
                            std::lerp(base.greenF(), foreground.greenF(), amount),
                            std::lerp(base.blueF(), foreground.blueF(), amount), base.alphaF());
}
} // namespace

namespace Fooyin::Style {
QBrush animatedBrush(const QStyleOption& option, const QWidget* widget, TransitionChannel channel, const QBrush& target)
{
    if(widget) {
        if(const auto* style = qobject_cast<const FooyinStyle*>(widget->style())) {
            return style->animatedBrush(option, widget, channel, target);
        }
    }
    return target;
}

QBrush opacityBrush(const QBrush& brush, double opacity)
{
    auto colour = brush.color();
    colour.setAlphaF(colour.alphaF() * static_cast<float>(std::clamp(opacity, 0.0, 1.0)));
    return QBrush{colour};
}

QPalette::ColorGroup colourGroup(const QStyleOption& option)
{
    return !(option.state & QStyle::State_Enabled) ? QPalette::Disabled
         : (option.state & QStyle::State_Active)   ? QPalette::Active
                                                   : QPalette::Inactive;
}

QColor panelColour(const QStyleOption& option)
{
    const auto group = colourGroup(option);
    return mix(option.palette.color(group, QPalette::Window), option.palette.color(group, QPalette::WindowText), 0.025);
}

QColor outlineColour(const QStyleOption& option)
{
    const auto group = colourGroup(option);
    return mix(option.palette.color(group, QPalette::Mid), option.palette.color(group, QPalette::WindowText), 0.22);
}

QIcon tintedIcon(const QIcon& icon, const QSize& size, double devicePixelRatio, QIcon::Mode mode, QIcon::State state,
                 const QColor& colour)
{
    const QPixmap pixmap = icon.pixmap(size, devicePixelRatio, mode, state);
    if(pixmap.isNull()) {
        return icon;
    }

    const QString key = u"FooyinStyleTint|%1|%2|%3"_s.arg(pixmap.cacheKey())
                            .arg(static_cast<quint64>(colour.rgba64()))
                            .arg(pixmap.devicePixelRatioF(), 0, 'g', 17);
    QPixmap tintedPixmap;
    if(!QPixmapCache::find(key, &tintedPixmap)) {
        tintedPixmap = pixmap;
        QPainter painter{&tintedPixmap};
        painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
        painter.fillRect(tintedPixmap.rect(), colour);
        painter.end();
        QPixmapCache::insert(key, tintedPixmap);
    }

    QIcon tinted;
    tinted.addPixmap(tintedPixmap, mode, QIcon::Off);
    tinted.addPixmap(tintedPixmap, mode, QIcon::On);
    return tinted;
}

void drawChevron(const QStyleOption& option, const QRect& rect, QPainter& painter, bool up, QPalette::ColorRole role)
{
    drawChevron(option, rect, painter, up, option.palette.brush(colourGroup(option), role));
}

void drawChevron(const QStyleOption& /*option*/, const QRect& rect, QPainter& painter, bool up, const QBrush& brush)
{
    if(rect.isEmpty()) {
        return;
    }

    const QPointF centre   = QRectF{rect}.center();
    const double direction = up ? -1.0 : 1.0;
    const std::array points{centre + QPointF{-3.0, -1.5 * direction}, centre + QPointF{0.0, 1.5 * direction},
                            centre + QPointF{3.0, -1.5 * direction}};

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen{brush, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin});
    painter.drawPolyline(points.data(), static_cast<int>(points.size()));
    painter.restore();
}

void drawArrow(const QStyleOption& option, QPainter& painter, Qt::ArrowType arrow)
{
    const int extent = std::min({12, option.rect.width(), option.rect.height()});
    if(extent <= 0 || arrow == Qt::NoArrow) {
        return;
    }

    painter.save();
    painter.setClipRect(option.rect, Qt::IntersectClip);
    painter.translate(QRectF{option.rect}.center());
    if(arrow == Qt::LeftArrow || arrow == Qt::RightArrow) {
        painter.rotate(arrow == Qt::LeftArrow ? 90.0 : -90.0);
    }
    painter.scale(extent / 12.0, extent / 12.0);
    drawChevron(option, {-6, -6, 12, 12}, painter, arrow == Qt::UpArrow, QPalette::ButtonText);
    painter.restore();
}

void drawRelief(const QStyleOption& option, QPainter& painter, const QPainterPath& path, double width, double opacity)
{
    if(width <= 0 || opacity <= 0) {
        return;
    }

    const auto group  = colourGroup(option);
    const auto amount = static_cast<float>(opacity / 100.0);

    auto shadow = option.palette.color(group, QPalette::Shadow);
    shadow.setAlphaF(shadow.alphaF() * amount);
    auto light = option.palette.color(group, QPalette::Light);
    light.setAlphaF(light.alphaF() * amount);

    painter.strokePath(path.translated(0.0, 1.0), QPen{shadow, width});
    painter.strokePath(path.translated(0.0, -1.0), QPen{light, width});
}
} // namespace Fooyin::Style
