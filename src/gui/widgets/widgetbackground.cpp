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

#include <utils/async.h>

#include <QBasicTimer>
#include <QEvent>
#include <QImage>
#include <QPainter>
#include <QPixmap>
#include <QTimerEvent>
#include <QWidget>

#include <cstdint>
#include <optional>

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
class WidgetBackgroundPrivate
{
public:
    WidgetBackgroundPrivate(WidgetBackground* self, QWidget* viewport);

    struct PreparationKey
    {
        qint64 sourceKey{0};
        QSize size;
        qreal dpr{1.0};
        int blur{0};

        bool operator==(const PreparationKey& other) const = default;
    };

    struct PreparationRequest
    {
        PreparationKey key;
        QImage image;
        uint64_t generation{0};
    };

    struct PixmapCache
    {
        PreparationKey key;
        PreparationKey requestedKey;
        QPixmap pixmap;
        uint64_t generation{0};
        std::optional<PreparationRequest> pending;
    };

    void invalidateScaledBackground();
    void startPreparation();
    QPixmap preparedBackgroundPixmap(const QPixmap& source, PixmapCache& cache);
    [[nodiscard]] QPoint backgroundPixmapPosition(const QSize& pixmapSize) const;
    void drawBackgroundPixmap(QPainter& painter, const QPixmap& pixmap);

    WidgetBackground* m_self;
    QWidget* m_viewport;
    BackgroundConfig m_config;
    PixmapFadeController* m_bgFadeController;
    QBasicTimer m_bgResizeTimer;
    PixmapCache m_cachedBg;
    PixmapCache m_cachedPreviousBg;
    bool m_preparing{false};
};

WidgetBackgroundPrivate::WidgetBackgroundPrivate(WidgetBackground* self, QWidget* viewport)
    : m_self{self}
    , m_viewport{viewport}
    , m_bgFadeController{new PixmapFadeController(self)}
{
    m_bgFadeController->setUpdateCallback([this] { m_viewport->update(); });
}

void WidgetBackgroundPrivate::invalidateScaledBackground()
{
    for(auto* cache : {&m_cachedBg, &m_cachedPreviousBg}) {
        ++cache->generation;
        cache->requestedKey = {};
        cache->pending.reset();
    }
}

void WidgetBackgroundPrivate::startPreparation()
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
    }).then(m_self, [this, cache, key, generation](QImage image) {
        if(cache->generation == generation && qFuzzyCompare(key.dpr, m_viewport->devicePixelRatioF())
           && !image.isNull()) {
            cache->key    = key;
            cache->pixmap = QPixmap::fromImage(std::move(image));
        }

        m_preparing = false;
        startPreparation();
        m_viewport->update();
    });
}

QPixmap WidgetBackgroundPrivate::preparedBackgroundPixmap(const QPixmap& source, PixmapCache& cache)
{
    const QSize viewportSize = m_viewport->size();
    const qreal dpr          = m_viewport->devicePixelRatioF();
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

    const PreparationKey key{source.cacheKey(), targetSize, dpr, m_config.blur};
    if(key == cache.key && !cache.pixmap.isNull()) {
        return cache.pixmap;
    }

    if(key != cache.requestedKey) {
        cache.requestedKey = key;
        cache.pending      = PreparationRequest{key, source.toImage(), ++cache.generation};
        startPreparation();
    }

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

    if(m_config.scaling != static_cast<int>(BackgroundImageScaling::OriginalSize)) {
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
    if(p->m_bgFadeController->pixmap().cacheKey() == pixmap.cacheKey()) {
        return;
    }

    if(p->m_bgFadeController->enabled()) {
        p->m_cachedPreviousBg.key    = p->m_cachedBg.key;
        p->m_cachedPreviousBg.pixmap = p->m_cachedBg.pixmap;
    }
    else {
        p->m_cachedPreviousBg.key    = {};
        p->m_cachedPreviousBg.pixmap = {};
    }

    if(pixmap.isNull()) {
        p->m_cachedBg.pixmap         = {};
        p->m_cachedPreviousBg.pixmap = {};
    }
    p->invalidateScaledBackground();
    p->m_bgFadeController->setPixmap(pixmap);
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

bool WidgetBackground::hasImage() const
{
    return p->m_config.imageMode != static_cast<int>(BackgroundImageMode::None) && p->m_config.opacity > 0
        && !p->m_bgFadeController->pixmap().isNull();
}

bool WidgetBackground::eventFilter(QObject* watched, QEvent* event)
{
    if(watched == p->m_viewport && event->type() == QEvent::Resize) {
        p->invalidateScaledBackground();
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
        p->m_viewport->update();
        return;
    }
    QObject::timerEvent(event);
}
} // namespace Fooyin
