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

#include "pixmapfadecontroller.h"

#include <QVariantAnimation>

namespace Fooyin {
PixmapFadeController::PixmapFadeController(QObject* parent)
    : QObject{parent}
    , m_animation{new QVariantAnimation(this)}
    , m_enabled{false}
    , m_durationMs{1000}
    , m_progress{1.0}
{
    m_animation->setStartValue(0.0);
    m_animation->setEndValue(1.0);
    m_animation->setDuration(m_durationMs);
    m_animation->setEasingCurve(QEasingCurve::InOutCubic);

    QObject::connect(m_animation, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
        m_progress = value.toReal();
        requestUpdate();
    });
    QObject::connect(m_animation, &QVariantAnimation::finished, this, [this] {
        m_prevPixmap = {};
        m_progress   = 1.0;
        requestUpdate();
    });
}

void PixmapFadeController::setUpdateCallback(std::function<void()> callback)
{
    m_updateCallback = std::move(callback);
}

void PixmapFadeController::setEnabled(const bool enabled)
{
    m_enabled = enabled;

    if(!m_enabled) {
        stop();
    }
}

void PixmapFadeController::setDurationMs(const int durationMs)
{
    m_durationMs = std::clamp(durationMs, MinDurationMs, MaxDurationMs);
    m_animation->setDuration(m_durationMs);
}

void PixmapFadeController::setPixmap(const QPixmap& pixmap)
{
    if(pixmapsMatch(m_pixmap, pixmap)) {
        return;
    }

    const QPixmap prevPixmap{m_pixmap};
    m_pixmap = pixmap;

    if(!m_enabled || pixmap.isNull()) {
        stop();
        return;
    }

    m_prevPixmap = prevPixmap;
    m_animation->stop();
    m_progress = 0.0;
    requestUpdate();
    m_animation->start();
}

void PixmapFadeController::stop()
{
    m_animation->stop();
    m_prevPixmap = {};
    m_progress   = 1.0;
    requestUpdate();
}

bool PixmapFadeController::enabled() const
{
    return m_enabled;
}

int PixmapFadeController::durationMs() const
{
    return m_durationMs;
}

qreal PixmapFadeController::progress() const
{
    return m_progress;
}

const QPixmap& PixmapFadeController::pixmap() const
{
    return m_pixmap;
}

const QPixmap& PixmapFadeController::previousPixmap() const
{
    return m_prevPixmap;
}

bool PixmapFadeController::pixmapsMatch(const QPixmap& lhs, const QPixmap& rhs)
{
    if(lhs.cacheKey() == rhs.cacheKey()) {
        return true;
    }

    if(lhs.size() != rhs.size() || !qFuzzyCompare(lhs.devicePixelRatio(), rhs.devicePixelRatio())) {
        return false;
    }

    return lhs.toImage() == rhs.toImage();
}

void PixmapFadeController::requestUpdate()
{
    if(m_updateCallback) {
        m_updateCallback();
    }
}
} // namespace Fooyin
