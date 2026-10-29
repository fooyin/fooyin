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

#include <gui/widgets/widgetbackground.h>

#include "pixmapfadecontroller.h"

#include <QBasicTimer>
#include <QEvent>
#include <QPainter>
#include <QPixmap>
#include <QTimerEvent>
#include <QWidget>

QT_BEGIN_NAMESPACE
// Exported by qimageeffects.cpp
void qt_blurImage(QImage& blurImage, qreal radius, bool quality, int transposed = 0);
QT_END_NAMESPACE

namespace {
QPixmap blurredPixmap(const QPixmap& pixmap, qreal radius)
{
    if(pixmap.isNull() || radius <= 0) {
        return pixmap;
    }

    QImage blurred = pixmap.toImage().convertToFormat(QImage::Format_ARGB32_Premultiplied);
    qt_blurImage(blurred, radius, true);
    return QPixmap::fromImage(blurred);
}
} // namespace

namespace Fooyin {
class WidgetBackgroundPrivate
{
public:
    WidgetBackgroundPrivate(WidgetBackground* self, QWidget* viewport);

    struct PixmapCache
    {
        qint64 sourceKey{0};
        QSize viewportSize;
        qreal dpr{1.0};
        QPixmap pixmap;
    };

    void invalidateScaledBackground();
    [[nodiscard]] QPixmap preparedBackgroundPixmap(const QPixmap& source, PixmapCache& cache) const;
    [[nodiscard]] QPoint backgroundPixmapPosition(const QSize& pixmapSize) const;
    void drawBackgroundPixmap(QPainter& painter, const QPixmap& pixmap);

    QWidget* m_viewport;
    BackgroundConfig m_config;
    PixmapFadeController* m_bgFadeController;
    QBasicTimer m_bgResizeTimer;
    mutable PixmapCache m_cachedBg;
    mutable PixmapCache m_cachedPreviousBg;
};

WidgetBackgroundPrivate::WidgetBackgroundPrivate(WidgetBackground* self, QWidget* viewport)
    : m_viewport{viewport}
    , m_bgFadeController{new PixmapFadeController(self)}
{
    m_bgFadeController->setUpdateCallback([this] { m_viewport->update(); });
}

void WidgetBackgroundPrivate::invalidateScaledBackground()
{
    m_cachedBg         = {};
    m_cachedPreviousBg = {};
}

QPixmap WidgetBackgroundPrivate::preparedBackgroundPixmap(const QPixmap& source, PixmapCache& cache) const
{
    const QSize viewportSize = m_viewport->size();
    const qreal dpr          = m_viewport->devicePixelRatioF();
    if(viewportSize.isEmpty() || source.isNull()) {
        return {};
    }

    // Keep using the cache during an interactive resize
    const bool useCache = m_config.scaling == static_cast<int>(BackgroundImageScaling::OriginalSize)
                       || cache.viewportSize == viewportSize;
    if(cache.sourceKey == source.cacheKey() && (useCache || m_bgResizeTimer.isActive()) && qFuzzyCompare(cache.dpr, dpr)
       && !cache.pixmap.isNull()) {
        return cache.pixmap;
    }

    QSize targetSize = source.size();

    switch(static_cast<BackgroundImageScaling>(m_config.scaling)) {
        case BackgroundImageScaling::ScaledAndCropped:
            targetSize.scale(viewportSize, Qt::KeepAspectRatioByExpanding);
            break;
        case BackgroundImageScaling::Scaled:
            targetSize = viewportSize;
            break;
        case BackgroundImageScaling::ScaledKeepProportions:
            targetSize.scale(viewportSize, Qt::KeepAspectRatio);
            break;
        case BackgroundImageScaling::OriginalSize:
            if(m_config.maxSize > 0) {
                const QSize maxSize{m_config.maxSize, m_config.maxSize};
                if(targetSize.width() > maxSize.width() || targetSize.height() > maxSize.height()) {
                    targetSize.scale(maxSize, Qt::KeepAspectRatio);
                }
            }
            break;
    }

    if(targetSize.isEmpty()) {
        return {};
    }

    targetSize = {static_cast<int>(std::ceil(static_cast<qreal>(targetSize.width()) * dpr)),
                  static_cast<int>(std::ceil(static_cast<qreal>(targetSize.height()) * dpr))};

    cache.sourceKey    = source.cacheKey();
    cache.viewportSize = viewportSize;
    cache.dpr          = dpr;
    cache.pixmap       = source.scaled(targetSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    cache.pixmap.setDevicePixelRatio(dpr);
    cache.pixmap = blurredPixmap(cache.pixmap, m_config.blur * dpr);

    return cache.pixmap;
}

QPoint WidgetBackgroundPrivate::backgroundPixmapPosition(const QSize& pixmapSize) const
{
    const QSize viewportSize = m_viewport->size();

    if(m_config.scaling != static_cast<int>(BackgroundImageScaling::OriginalSize)) {
        return {(viewportSize.width() - pixmapSize.width()) / 2, (viewportSize.height() - pixmapSize.height()) / 2};
    }

    int x{0};
    int y{0};

    switch(static_cast<BackgroundImagePosition>(m_config.position)) {
        case BackgroundImagePosition::TopLeft:
        case BackgroundImagePosition::Left:
        case BackgroundImagePosition::BottomLeft:
            x = 0;
            break;
        case BackgroundImagePosition::Top:
        case BackgroundImagePosition::Middle:
        case BackgroundImagePosition::Bottom:
            x = (viewportSize.width() - pixmapSize.width()) / 2;
            break;
        case BackgroundImagePosition::TopRight:
        case BackgroundImagePosition::Right:
        case BackgroundImagePosition::BottomRight:
            x = viewportSize.width() - pixmapSize.width();
            break;
    }

    switch(static_cast<BackgroundImagePosition>(m_config.position)) {
        case BackgroundImagePosition::TopLeft:
        case BackgroundImagePosition::Top:
        case BackgroundImagePosition::TopRight:
            y = 0;
            break;
        case BackgroundImagePosition::Left:
        case BackgroundImagePosition::Middle:
        case BackgroundImagePosition::Right:
            y = (viewportSize.height() - pixmapSize.height()) / 2;
            break;
        case BackgroundImagePosition::BottomLeft:
        case BackgroundImagePosition::Bottom:
        case BackgroundImagePosition::BottomRight:
            y = viewportSize.height() - pixmapSize.height();
            break;
    }

    return {x, y};
}

void WidgetBackgroundPrivate::drawBackgroundPixmap(QPainter& painter, const QPixmap& pixmap)
{
    QSize pixmapSize = pixmap.deviceIndependentSize().toSize();

    if(m_bgResizeTimer.isActive() && m_config.scaling != static_cast<int>(BackgroundImageScaling::OriginalSize)) {
        const QSize viewportSize = m_viewport->size();

        if(m_config.scaling == static_cast<int>(BackgroundImageScaling::Scaled)) {
            pixmapSize = viewportSize;
        }
        else {
            const Qt::AspectRatioMode aspectRatioMode
                = m_config.scaling == static_cast<int>(BackgroundImageScaling::ScaledAndCropped)
                    ? Qt::KeepAspectRatioByExpanding
                    : Qt::KeepAspectRatio;
            pixmapSize.scale(viewportSize, aspectRatioMode);
        }

        painter.drawPixmap(QRect{backgroundPixmapPosition(pixmapSize), pixmapSize}, pixmap);
        return;
    }

    painter.drawPixmap(backgroundPixmapPosition(pixmapSize), pixmap);
}

WidgetBackground::WidgetBackground(QWidget* viewport)
    : QObject{viewport}
    , p{std::make_unique<WidgetBackgroundPrivate>(this, viewport)}
{
    p->m_viewport->installEventFilter(this);
}

WidgetBackground::~WidgetBackground() = default;

void WidgetBackground::setConfig(const BackgroundConfig& config)
{
    auto validated{config};
    validated.normalise();
    if(p->m_config == validated) {
        return;
    }
    p->m_config = std::move(validated);
    p->m_bgFadeController->setDurationMs(p->m_config.fadeDuration);
    p->m_bgFadeController->setEnabled(p->m_config.fadeDuration > 0);

    p->invalidateScaledBackground();
    p->m_viewport->update();
}

void WidgetBackground::setPixmap(const QPixmap& pixmap)
{
    p->invalidateScaledBackground();
    p->m_bgFadeController->setPixmap(pixmap);
}

bool WidgetBackground::hasImage() const
{
    return p->m_config.imageMode != static_cast<int>(BackgroundImageMode::None) && p->m_config.opacity > 0
        && !p->m_bgFadeController->pixmap().isNull();
}

bool WidgetBackground::eventFilter(QObject* watched, QEvent* event)
{
    if(watched == p->m_viewport && event->type() == QEvent::Resize) {
        const auto& config = p->m_config;

        const bool resizeBackground = config.imageMode != static_cast<int>(BackgroundImageMode::None)
                                   && config.scaling != static_cast<int>(BackgroundImageScaling::OriginalSize)
                                   && config.opacity > 0 && !p->m_bgFadeController->pixmap().isNull();
        if(!resizeBackground) {
            p->m_bgResizeTimer.stop();
        }
        else {
            p->m_bgResizeTimer.start(100, this);
        }
    }

    return QObject::eventFilter(watched, event);
}

void WidgetBackground::timerEvent(QTimerEvent* event)
{
    if(event->timerId() == p->m_bgResizeTimer.timerId()) {
        p->m_bgResizeTimer.stop();
        p->invalidateScaledBackground();
        p->m_viewport->update();
        return;
    }
    QObject::timerEvent(event);
}

void WidgetBackground::draw(QPainter& painter)
{
    const QPixmap& currentPixmap = p->m_bgFadeController->pixmap();
    if(p->m_config.imageMode == static_cast<int>(BackgroundImageMode::None) || currentPixmap.isNull()
       || p->m_config.opacity <= 0) {
        return;
    }

    const QPixmap pixmap = p->preparedBackgroundPixmap(currentPixmap, p->m_cachedBg);
    if(pixmap.isNull()) {
        return;
    }

    painter.save();

    const double opacity = std::clamp(p->m_config.opacity, 0, 100) / 100.0;

    if(p->m_bgFadeController->progress() < 1.0) {
        const QPixmap& prevPixmap = p->m_bgFadeController->previousPixmap();
        const QPixmap prev        = p->preparedBackgroundPixmap(prevPixmap, p->m_cachedPreviousBg);

        if(!prev.isNull()) {
            painter.setOpacity(opacity * (1.0 - p->m_bgFadeController->progress()));
            p->drawBackgroundPixmap(painter, prev);
        }
        painter.setOpacity(opacity * p->m_bgFadeController->progress());
    }
    else {
        painter.setOpacity(opacity);
    }

    p->drawBackgroundPixmap(painter, pixmap);

    painter.restore();
}
} // namespace Fooyin
