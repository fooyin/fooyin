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

#include "backgroundrenderer.h"

#include "pixmapfadecontroller.h"

#include <utils/async.h>

#include <QPainter>
#include <QTimerEvent>

#include <cmath>

QT_BEGIN_NAMESPACE
// Exported by qimageeffects.cpp
void qt_blurImage(QImage& blurImage, qreal radius, bool quality, int transposed = 0);
QT_END_NAMESPACE

namespace {
QImage prepareBackground(QImage image, const QSize& targetSize, qreal radius, qreal dpr)
{
    const int blurScale = radius >= 16.0 ? 4 : radius >= 4.0 ? 2 : 1;
    const QSize blurSize{std::max(1, targetSize.width() / blurScale), std::max(1, targetSize.height() / blurScale)};

    image = image.scaled(blurSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    if(radius > 0 && !image.isNull()) {
        image = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
        qt_blurImage(image, radius / blurScale, true);
    }
    if(blurSize != targetSize) {
        image = image.scaled(targetSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }

    image.setDevicePixelRatio(dpr);
    return image;
}
} // namespace

namespace Fooyin {
BackgroundRenderer::BackgroundRenderer(QObject* parent)
    : QObject{parent}
    , m_dpr{1.0}
    , m_bgFadeController{new PixmapFadeController(this)}
    , m_preparing{false}
{
    m_bgFadeController->setUpdateCallback([this] { Q_EMIT changed(); });
}

bool BackgroundRenderer::hasImage() const
{
    return m_config.imageMode != static_cast<int>(BackgroundImageMode::None) && m_config.opacity > 0
        && !m_bgFadeController->pixmap().isNull();
}

void BackgroundRenderer::setCanvas(const QSize& size, qreal dpr)
{
    if(m_size == size && qFuzzyCompare(m_dpr, dpr)) {
        return;
    }
    m_size = size;
    m_dpr  = dpr;
    invalidateScaledBackground();
    if(hasImage() && m_config.scaling != static_cast<int>(BackgroundImageScaling::OriginalSize)) {
        m_bgResizeTimer.start(100, this);
    }
    else {
        m_bgResizeTimer.stop();
    }
    Q_EMIT changed();
}

void BackgroundRenderer::setConfig(const BackgroundConfig& config)
{
    auto validated{config};
    validated.normalise();
    if(m_config == validated) {
        return;
    }
    m_config = std::move(validated);
    m_bgFadeController->setDurationMs(m_config.fadeDuration);
    m_bgFadeController->setEnabled(m_config.fadeDuration > 0);

    invalidateScaledBackground();
    Q_EMIT changed();
}

void BackgroundRenderer::setPixmap(const QPixmap& pixmap)
{
    if(m_bgFadeController->pixmap().cacheKey() == pixmap.cacheKey()) {
        return;
    }

    if(m_bgFadeController->enabled()) {
        m_cachedPreviousBg.key    = m_cachedBg.key;
        m_cachedPreviousBg.pixmap = m_cachedBg.pixmap;
    }
    else {
        m_cachedPreviousBg.key    = {};
        m_cachedPreviousBg.pixmap = {};
    }

    if(pixmap.isNull()) {
        m_cachedBg.pixmap         = {};
        m_cachedPreviousBg.pixmap = {};
    }
    invalidateScaledBackground();
    m_bgFadeController->setPixmap(pixmap);
    Q_EMIT changed();
}

void BackgroundRenderer::draw(QPainter& painter)
{
    const QPixmap& currentPixmap = m_bgFadeController->pixmap();
    if(m_config.imageMode == static_cast<int>(BackgroundImageMode::None) || currentPixmap.isNull()
       || m_config.opacity <= 0) {
        return;
    }

    const QPixmap pixmap = preparedBackgroundPixmap(currentPixmap, m_cachedBg);
    if(pixmap.isNull()) {
        return;
    }

    painter.save();

    const double opacity = std::clamp(m_config.opacity, 0, 100) / 100.0;

    if(m_bgFadeController->progress() < 1.0) {
        const QPixmap& prevPixmap = m_bgFadeController->previousPixmap();
        const QPixmap prev        = preparedBackgroundPixmap(prevPixmap, m_cachedPreviousBg);

        if(!prev.isNull()) {
            painter.setOpacity(opacity * (1.0 - m_bgFadeController->progress()));
            drawBackgroundPixmap(painter, prev);
        }
        painter.setOpacity(opacity * m_bgFadeController->progress());
    }
    else {
        painter.setOpacity(opacity);
    }

    drawBackgroundPixmap(painter, pixmap);

    painter.restore();
}

void BackgroundRenderer::timerEvent(QTimerEvent* event)
{
    if(event->timerId() == m_bgResizeTimer.timerId()) {
        m_bgResizeTimer.stop();
        Q_EMIT changed();
        return;
    }
    QObject::timerEvent(event);
}

void BackgroundRenderer::invalidateScaledBackground()
{
    for(auto* cache : {&m_cachedBg, &m_cachedPreviousBg}) {
        ++cache->generation;
        cache->requestedKey = {};
        cache->pending.reset();
    }
}

void BackgroundRenderer::startPreparation()
{
    if(m_preparing) {
        return;
    }

    auto* cache = m_cachedBg.pending ? &m_cachedBg : m_cachedPreviousBg.pending ? &m_cachedPreviousBg : nullptr;
    if(!cache) {
        return;
    }

    auto request = std::move(*cache->pending);
    cache->pending.reset();
    m_preparing = true;
    const auto key{request.key};
    const auto generation{request.generation};

    Utils::asyncExec([request = std::move(request)]() mutable {
        return prepareBackground(std::move(request.image), request.key.size, request.key.blur * request.key.dpr,
                                 request.key.dpr);
    }).then(this, [this, cache, key, generation](QImage image) {
        if(cache->generation == generation && qFuzzyCompare(key.dpr, m_dpr) && !image.isNull()) {
            cache->key    = key;
            cache->pixmap = QPixmap::fromImage(std::move(image));
        }

        m_preparing = false;
        startPreparation();
        Q_EMIT changed();
    });
}

QPixmap BackgroundRenderer::preparedBackgroundPixmap(const QPixmap& source, PixmapCache& cache)
{
    const QSize viewportSize = m_size;
    const qreal dpr          = m_dpr;
    if(viewportSize.isEmpty() || source.isNull()) {
        return {};
    }

    // Keep displaying the prepared image until an interactive resize settles
    if(m_bgResizeTimer.isActive() && cache.key.sourceKey == source.cacheKey() && cache.key.blur == m_config.blur
       && qFuzzyCompare(cache.key.dpr, dpr) && !cache.pixmap.isNull()) {
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

    const PreparationKey key{.sourceKey = source.cacheKey(), .size = targetSize, .dpr = dpr, .blur = m_config.blur};
    if(key == cache.key && !cache.pixmap.isNull()) {
        return cache.pixmap;
    }

    if(key != cache.requestedKey) {
        cache.requestedKey = key;
        cache.pending = PreparationRequest{.key = key, .image = source.toImage(), .generation = ++cache.generation};
        startPreparation();
    }

    return cache.pixmap;
}

QPoint BackgroundRenderer::backgroundPixmapPosition(const QSize& pixmapSize) const
{
    const QSize viewportSize = m_size;

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

void BackgroundRenderer::drawBackgroundPixmap(QPainter& painter, const QPixmap& pixmap)
{
    QSize pixmapSize = pixmap.deviceIndependentSize().toSize();

    if(m_config.scaling != static_cast<int>(BackgroundImageScaling::OriginalSize)) {
        const QSize viewportSize = m_size;

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
} // namespace Fooyin

#include "moc_backgroundrenderer.cpp"
