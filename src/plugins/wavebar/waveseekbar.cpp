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

#include "waveseekbar.h"

#include <utils/stringutils.h>

#include <QImage>
#include <QMouseEvent>
#include <QPainter>
#include <QStyle>

#include <algorithm>
#include <cmath>

using namespace Qt::StringLiterals;

constexpr auto ToolTipDelay = 5;

namespace {
QColor blendColors(const QColor& color1, const QColor& color2, double ratio)
{
    const int r = static_cast<int>((color1.red() * (1.0 - ratio)) + (color2.red() * ratio));
    const int g = static_cast<int>((color1.green() * (1.0 - ratio)) + (color2.green() * ratio));
    const int b = static_cast<int>((color1.blue() * (1.0 - ratio)) + (color2.blue() * ratio));
    const int a = static_cast<int>((color1.alpha() * (1.0 - ratio)) + (color2.alpha() * ratio));

    return {r, g, b, a};
}

void setupPainter(QPainter& painter, bool isInProgress, bool isPlayed, int barWidth, double progress,
                  const QColor& unplayed, const QColor& played, const QColor& border)
{
    if(isInProgress) {
        painter.setBrush(blendColors(unplayed, played, progress));
    }
    else {
        painter.setBrush(isPlayed ? played : unplayed);
    }

    if(barWidth > 1) {
        painter.setPen(border);
    }
    else {
        painter.setPen(Qt::NoPen);
    }
}
} // namespace

namespace Fooyin::WaveBar {
WaveSeekBar::WaveSeekBar(QWidget* parent)
    : QWidget{parent}
    , m_playState{Player::PlayState::Stopped}
    , m_seekable{false}
    , m_scale{1.0}
    , m_position{0}
    , m_showCursor{true}
    , m_cursorWidth{3}
    , m_channelScale{0.9}
    , m_barWidth{1}
    , m_barGap{0}
    , m_sampleWidth{m_barWidth + m_barGap}
    , m_supersampleFactor{1}
    , m_maxScale{1.0}
    , m_centreGap{0}
    , m_mode{Default}
    , m_colours{}
    , m_waveformCacheRenderWidth{0}
    , m_waveformCacheDirty{true}
{
    setFocusPolicy(Qt::TabFocus);
}

void WaveSeekBar::setMouseFocusEnabled(bool enabled)
{
    setFocusPolicy(enabled ? Qt::StrongFocus : Qt::TabFocus);
}

void WaveSeekBar::processData(const WaveformData<float>& waveData)
{
    m_data  = waveData;
    m_scale = 1.0;

    invalidateWaveformCache();
    update();
}

void WaveSeekBar::setPlayState(Player::PlayState state)
{
    m_playState = state;
}

void WaveSeekBar::setSeekable(bool seekable)
{
    if(std::exchange(m_seekable, seekable) != seekable && !seekable) {
        stopSeeking();
    }
}

void WaveSeekBar::setPosition(uint64_t pos)
{
    const uint64_t oldPos = std::exchange(m_position, pos);

    if(oldPos == pos) {
        return;
    }

    const double oldX = positionFromValue(static_cast<double>(oldPos));
    const double x    = positionFromValue(static_cast<double>(pos));

    updateRange(oldX, x);
}

void WaveSeekBar::setShowCursor(bool show)
{
    if(std::exchange(m_showCursor, show) != show) {
        update();
    }
}

void WaveSeekBar::setCursorWidth(int width)
{
    const int validatedWidth = std::max(1, width);
    if(std::exchange(m_cursorWidth, validatedWidth) != validatedWidth) {
        update();
    }
}

void WaveSeekBar::setChannelScale(double scale)
{
    const double validatedScale = std::clamp(scale, 0.0, 1.0);
    if(std::exchange(m_channelScale, validatedScale) != validatedScale) {
        invalidate();
    }
}

void WaveSeekBar::setBarWidth(int width)
{
    const int validatedWidth = std::max(1, width);
    if(std::exchange(m_barWidth, validatedWidth) != validatedWidth) {
        m_sampleWidth = m_barWidth + m_barGap;
        invalidate();
    }
}

void WaveSeekBar::setBarGap(int gap)
{
    const int validatedGap = std::max(0, gap);
    if(std::exchange(m_barGap, validatedGap) != validatedGap) {
        m_sampleWidth = m_barWidth + m_barGap;
        invalidate();
    }
}

void WaveSeekBar::setMaxScale(double scale)
{
    const double validatedScale = std::clamp(scale, 0.0, 2.0);
    if(std::exchange(m_maxScale, validatedScale) != validatedScale) {
        invalidate();
    }
}

void WaveSeekBar::setCentreGap(int gap)
{
    const int validatedGap = std::max(0, gap);
    if(std::exchange(m_centreGap, validatedGap) != validatedGap) {
        invalidate();
    }
}

void WaveSeekBar::setMode(WaveModes mode)
{
    if(std::exchange(m_mode, mode) != mode) {
        invalidate();
    }
}

void WaveSeekBar::setColours(const Colours& colours)
{
    if(std::exchange(m_colours, colours) != colours) {
        invalidate();
    }
}

void WaveSeekBar::refreshStyleColours()
{
    invalidateWaveformCache();
}

void WaveSeekBar::setSupersampleFactor(int factor)
{
    factor = std::max(1, factor);
    if(std::exchange(m_supersampleFactor, factor) != factor) {
        invalidate();
    }
}

bool WaveSeekBar::isSeeking() const
{
    return !m_seekPos.isNull();
}

void WaveSeekBar::stopSeeking()
{
    if(m_seekTip) {
        m_seekTip->deleteLater();
    }

    m_seekPos = {};
    update();
}

void WaveSeekBar::paintEvent(QPaintEvent* event)
{
    QPainter painter{this};
    if(m_data.empty() || m_supersampleFactor <= 1) {
        paintWaveform(painter, event->rect(), width());
        drawCursors(painter);
        return;
    }

    drawCachedWaveform(painter, event->rect());
    drawCursors(painter);
}

void WaveSeekBar::mouseMoveEvent(QMouseEvent* event)
{
    if(!m_seekable) {
        event->ignore();
        return;
    }

    if(isSeeking() && event->buttons() & Qt::LeftButton) {
        updateMousePosition(event->position().toPoint());
        if(!m_pressPos.isNull() && std::abs(m_pressPos.x() - event->position().x()) > ToolTipDelay) {
            drawSeekTip();
        }
    }
    else {
        QWidget::mouseMoveEvent(event);
    }
}

void WaveSeekBar::mousePressEvent(QMouseEvent* event)
{
    if(!m_seekable) {
        event->ignore();
        return;
    }

    if(!m_data.empty() && event->button() == Qt::LeftButton) {
        m_pressPos = event->position().toPoint();
        updateMousePosition(event->position().toPoint());
    }
    else {
        QWidget::mousePressEvent(event);
    }
}

void WaveSeekBar::mouseReleaseEvent(QMouseEvent* event)
{
    if(!m_seekable) {
        event->ignore();
        return;
    }

    if(event->button() != Qt::LeftButton || !isSeeking()) {
        QWidget::mouseReleaseEvent(event);
        return;
    }

    stopSeeking();
    m_pressPos = {};

    m_position = valueFromPosition(static_cast<int>(event->position().x()));
    Q_EMIT sliderMoved(m_position);
}

void WaveSeekBar::wheelEvent(QWheelEvent* event)
{
    if(!m_seekable) {
        event->ignore();
        return;
    }

    if(event->angleDelta().y() < 0) {
        Q_EMIT seekBackward();
    }
    else {
        Q_EMIT seekForward();
    }

    event->accept();
}

void WaveSeekBar::keyPressEvent(QKeyEvent* event)
{
    if(!m_seekable) {
        event->ignore();
        return;
    }

    const auto key = event->key();

    if(key == Qt::Key_Right || key == Qt::Key_Up) {
        Q_EMIT seekForward();
        event->accept();
    }
    else if(key == Qt::Key_Left || key == Qt::Key_Down) {
        Q_EMIT seekBackward();
        event->accept();
    }
    else {
        QWidget::keyPressEvent(event);
    }
}

double WaveSeekBar::positionFromValue(double value) const
{
    return positionFromValue(value, width());
}

double WaveSeekBar::positionFromValue(double value, double renderWidth) const
{
    if(m_data.duration == 0) {
        return 0.0;
    }

    if(value >= static_cast<double>(m_data.duration)) {
        return renderWidth;
    }

    const auto max   = static_cast<double>(m_data.duration);
    const auto ratio = value / max;

    return ratio * renderWidth;
}

uint64_t WaveSeekBar::valueFromPosition(int pos) const
{
    if(pos <= 0) {
        return 0;
    }

    if(pos >= width()) {
        return m_data.duration;
    }

    const auto max   = static_cast<double>(m_data.duration);
    const auto ratio = static_cast<double>(pos) / static_cast<double>(width());

    const auto value = static_cast<uint64_t>(ratio * max);

    return value;
}

void WaveSeekBar::updateMousePosition(const QPoint& pos)
{
    QPoint widgetPos{pos};

    widgetPos.setX(std::clamp(pos.x(), 1, width()));

    const auto oldSeekPos = std::exchange(m_seekPos, widgetPos);
    updateRange(oldSeekPos.x(), m_seekPos.x());
}

void WaveSeekBar::updateRange(double first, double last)
{
    if(std::abs(first - last) < 0.0001) {
        return;
    }

    const double cursorWidthPx = static_cast<double>(m_cursorWidth) * m_scale;
    const double sampleWidthPx = std::max(1.0, static_cast<double>(m_sampleWidth) * m_scale);
    const double gradientPx
        = (m_barGap == 0) ? (static_cast<double>(std::max(1, m_barWidth)) * 2.0 * std::max(1.0, m_scale)) : 0.0;

    const double overscanPx = sampleWidthPx * 2.0;
    const double marginPx   = cursorWidthPx + std::max(sampleWidthPx, gradientPx) + overscanPx + 2.0;
    const double leftPos    = std::min(first, last) - marginPx;
    const double rightPos   = std::max(first, last) + marginPx;

    const int left        = static_cast<int>(std::floor(leftPos));
    const int updateWidth = std::max(1, static_cast<int>(std::ceil(rightPos)) - left + 1);
    const QRect updateRect(left, 0, updateWidth, height());
    update(updateRect);

    if(isSeeking() && m_seekTip) {
        drawSeekTip();
    }
}

void WaveSeekBar::invalidateWaveformCache()
{
    m_waveformCacheDirty = true;
}

int WaveSeekBar::cachedRenderWidth() const
{
    const int widgetWidth = std::max(1, width());
    return std::max(widgetWidth * m_supersampleFactor, m_data.sampleCount() * m_sampleWidth);
}

void WaveSeekBar::ensureWaveformCache()
{
    const QSize targetSize{cachedRenderWidth(), height()};
    if(targetSize.isEmpty()) {
        m_unplayedWaveformCache    = {};
        m_playedWaveformCache      = {};
        m_waveformCacheSize        = {};
        m_transitionCache          = {};
        m_waveformCacheRenderWidth = 0;
        m_waveformCacheDirty       = true;
        return;
    }

    if(!m_waveformCacheDirty && m_waveformCacheSize == targetSize) {
        return;
    }

    m_waveformCacheSize        = targetSize;
    m_waveformCacheRenderWidth = targetSize.width();

    m_unplayedWaveformCache = QImage{targetSize, QImage::Format_ARGB32_Premultiplied};
    m_unplayedWaveformCache.fill(Qt::transparent);
    {
        QPainter painter{&m_unplayedWaveformCache};
        paintWaveform(painter, QRect{QPoint{0, 0}, targetSize}, m_waveformCacheRenderWidth,
                      PlaybackColourMode::Unplayed);
    }

    m_playedWaveformCache = QImage{targetSize, QImage::Format_ARGB32_Premultiplied};
    m_playedWaveformCache.fill(Qt::transparent);
    {
        QPainter painter{&m_playedWaveformCache};
        paintWaveform(painter, QRect{QPoint{0, 0}, targetSize}, m_waveformCacheRenderWidth, PlaybackColourMode::Played);
    }

    m_waveformCacheDirty = false;
}

void WaveSeekBar::drawCachedWaveform(QPainter& painter, const QRect& dirtyRect)
{
    ensureWaveformCache();

    if(m_unplayedWaveformCache.isNull() || m_playedWaveformCache.isNull()) {
        paintWaveform(painter, dirtyRect, width());
        return;
    }

    painter.save();
    painter.setClipRect(dirtyRect);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const QRectF targetRect{0.0, 0.0, static_cast<double>(width()), static_cast<double>(height())};
    const double positionX = positionFromValue(static_cast<double>(m_position));

    painter.drawImage(targetRect, m_unplayedWaveformCache);

    const double radius    = std::max(2.0, static_cast<double>(m_barWidth) * 2.0);
    const double playedEnd = std::max(0.0, positionX - radius);

    if(playedEnd > 0.0) {
        painter.save();

        painter.setClipRect(QRectF{0.0, 0.0, playedEnd, static_cast<double>(height())}, Qt::IntersectClip);
        painter.drawImage(targetRect, m_playedWaveformCache);

        painter.restore();
    }

    drawCachedTransition(painter, positionX);

    painter.restore();
}

void WaveSeekBar::drawCachedTransition(QPainter& painter, double positionX)
{
    const double radius = std::max(2.0, static_cast<double>(m_barWidth) * 2.0);
    const QRectF transitionRect{positionX - radius, 0.0, radius * 2.0, static_cast<double>(height())};
    const QRectF clipped = transitionRect.intersected(QRectF{rect()});

    if(clipped.isEmpty()) {
        return;
    }

    const qreal dpr = devicePixelRatioF();
    const QSize pixelSize{qCeil(width() * dpr), qCeil(height() * dpr)};

    // Allocate only on resize
    if(m_transitionCache.size() != pixelSize || !qFuzzyCompare(m_transitionCache.devicePixelRatio(), dpr)) {
        m_transitionCache = QImage{pixelSize, QImage::Format_ARGB32_Premultiplied};

        m_transitionCache.setDevicePixelRatio(dpr);
        m_transitionCache.fill(Qt::transparent);
    }

    const QRect clearRect = clipped.toAlignedRect().adjusted(-2, 0, 2, 0).intersected(rect());
    if(clearRect.isEmpty()) {
        return;
    }

    // Clear only the region we're about to regenerate
    {
        QPainter clearPainter{&m_transitionCache};
        clearPainter.setCompositionMode(QPainter::CompositionMode_Source);
        clearPainter.fillRect(clearRect, Qt::transparent);
    }

    {
        QPainter p{&m_transitionCache};
        p.setClipRect(clearRect);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);

        p.setCompositionMode(QPainter::CompositionMode_Source);

        const QRectF targetRect{0.0, 0.0, static_cast<double>(width()), static_cast<double>(height())};
        p.drawImage(targetRect, m_playedWaveformCache);

        p.setCompositionMode(QPainter::CompositionMode_DestinationIn);

        QLinearGradient gradient{QPointF{positionX - radius, 0.0}, QPointF{positionX + radius, 0.0}};
        gradient.setColorAt(0.0, QColor{255, 255, 255, 255});
        gradient.setColorAt(1.0, QColor{255, 255, 255, 0});
        p.fillRect(clearRect, gradient);
    }

    painter.save();
    painter.setClipRect(clearRect, Qt::IntersectClip);
    painter.drawImage(QPointF{0.0, 0.0}, m_transitionCache);
    painter.restore();
}

void WaveSeekBar::drawCursors(QPainter& painter)
{
    if(m_showCursor && m_playState != Player::PlayState::Stopped) {
        const double posX = positionFromValue(static_cast<double>(m_position));

        QPen cursorPen{m_colours.colour(Colours::Type::Cursor, palette()), static_cast<double>(m_cursorWidth),
                       Qt::SolidLine, Qt::FlatCap};
        cursorPen.setCosmetic(true);
        painter.setPen(cursorPen);

        painter.drawLine(QPointF{posX, 0.0}, QPointF{posX, static_cast<double>(height())});
    }

    if(isSeeking()) {
        QPen cursorPen{m_colours.colour(Colours::Type::SeekingCursor, palette()), static_cast<double>(m_cursorWidth),
                       Qt::SolidLine, Qt::FlatCap};
        cursorPen.setCosmetic(true);
        painter.setPen(cursorPen);

        painter.drawLine(QPointF{static_cast<double>(m_seekPos.x()), 0.0},
                         QPointF{static_cast<double>(m_seekPos.x()), static_cast<double>(height())});
    }
}

void WaveSeekBar::paintWaveform(QPainter& painter, const QRect& rect, double renderWidth, PlaybackColourMode colourMode)
{
    const QRect waveformRect{rect.left(), 0, rect.width(), height()};

    if(m_data.empty()) {
        painter.setPen({m_colours.colour(Colours::Type::MaxUnplayed, palette()), 1, Qt::SolidLine, Qt::FlatCap});
        const int centreY = height() / 2;
        painter.drawLine(waveformRect.left(), centreY, waveformRect.right(), centreY);
        return;
    }

    const int channels       = m_data.channels;
    const double firstSample = static_cast<double>(rect.left()) / static_cast<double>(std::max(1, m_sampleWidth));
    const double lastSample  = static_cast<double>(rect.right() + 1) / static_cast<double>(std::max(1, m_sampleWidth));
    const int first          = std::max(0, static_cast<int>(std::floor(firstSample)));
    const int last           = std::max(first, static_cast<int>(std::ceil(lastSample)));
    const double firstX      = first * m_sampleWidth;

    double posX{0.0};
    switch(colourMode) {
        case PlaybackColourMode::Unplayed:
            posX = 0.0;
            break;
        case PlaybackColourMode::Played:
            posX = renderWidth;
            break;
        case PlaybackColourMode::Position:
            posX = positionFromValue(static_cast<double>(m_position), renderWidth);
            break;
    }

    painter.fillRect(waveformRect, m_colours.colour(Colours::Type::BgUnplayed, palette()));
    const double playedWidth = std::max(0.0, posX - firstX);
    painter.fillRect(QRectF{firstX, 0.0, playedWidth, static_cast<double>(height())},
                     m_colours.colour(Colours::Type::BgPlayed, palette()));

    const int channelHeight     = height() / channels;
    const double waveformHeight = (channelHeight - m_centreGap) * m_channelScale;

    int y = static_cast<int>((channelHeight - waveformHeight) / 2);
    for(int ch{0}; ch < channels; ++ch) {
        drawChannel(painter, ch, waveformHeight, first, last, y, renderWidth, colourMode);
        y += channelHeight;
    }
}

void WaveSeekBar::drawChannel(QPainter& painter, int channel, double height, int first, int last, int y,
                              double renderWidth, PlaybackColourMode colourMode)
{
    const auto& [max, min, rms] = m_data.channelData[channel];
    if(max.empty() || min.empty() || rms.empty()) {
        return;
    }

    const double maxScale = (height / 2) * m_maxScale;
    const double minScale = height - maxScale;
    const double centre   = maxScale + y;

    const bool drawMax     = maxScale > 0;
    const bool drawMin     = minScale > 0;
    const double centreGap = drawMax && drawMin ? m_centreGap : 0;

    double rmsScale{1.0};
    if(m_mode & Rms && !(m_mode & MinMax)) {
        rmsScale = *std::ranges::max_element(rms);
    }

    const auto total             = static_cast<int>(max.size());
    const double currentPosition = positionFromValue(static_cast<double>(m_position), renderWidth);

    if(!m_data.complete) {
        const auto finalX    = static_cast<double>(total * m_sampleWidth);
        const double endX    = renderWidth;
        const double centreY = centre + (centreGap > 0 ? centreGap / 2 : 0);
        drawSilence(painter, finalX, endX, centreY, currentPosition, colourMode);
    }
    else if(m_mode & Silence && drawMax && drawMin) {
        const auto firstX    = static_cast<double>(first * m_sampleWidth);
        const auto lastX     = static_cast<double>((last + 1) * m_sampleWidth);
        const double centreY = centre + (centreGap > 0 ? centreGap / 2 : 0);
        drawSilence(painter, firstX, lastX, centreY, currentPosition, colourMode);
    }

    const auto clipH    = static_cast<double>(this->height());
    const int drawFirst = std::max(0, first);
    const auto firstX   = static_cast<double>(drawFirst * m_sampleWidth);

    auto drawComponent = [this, &painter](double x, double barWidth, double waveCentre, double amplitude, double scale,
                                          bool draw, const QColor& unplayed, const QColor& played, const QColor& border,
                                          bool inProgress, bool isPlayed, double progress) {
        if(!draw) {
            return;
        }

        const double valueY = waveCentre - (amplitude * scale);
        const QRectF rect{x, std::min(valueY, waveCentre), barWidth, std::abs(valueY - waveCentre)};

        setupPainter(painter, inProgress, isPlayed, m_barWidth, progress, unplayed, played, border);
        painter.drawRect(rect);
    };

    auto drawBar = [&](int i, bool inProgress, bool isPlayed, double progress) {
        const double x        = i * m_sampleWidth;
        const double barWidth = m_barWidth;

        if(m_mode & MinMax) {
            drawComponent(x, barWidth, centre, max[i], maxScale, drawMax,
                          m_colours.colour(Colours::Type::MaxUnplayed, palette()),
                          m_colours.colour(Colours::Type::MaxPlayed, palette()),
                          m_colours.colour(Colours::Type::MaxBorder, palette()), inProgress, isPlayed, progress);
            drawComponent(x, barWidth, centre + centreGap, min[i], minScale, drawMin,
                          m_colours.colour(Colours::Type::MinUnplayed, palette()),
                          m_colours.colour(Colours::Type::MinPlayed, palette()),
                          m_colours.colour(Colours::Type::MinBorder, palette()), inProgress, isPlayed, progress);
        }

        if(m_mode & Rms) {
            const double rmsAmp = rms[i] / rmsScale;
            drawComponent(x, barWidth, centre, rmsAmp, maxScale, drawMax,
                          m_colours.colour(Colours::Type::RmsMaxUnplayed, palette()),
                          m_colours.colour(Colours::Type::RmsMaxPlayed, palette()),
                          m_colours.colour(Colours::Type::RmsMaxBorder, palette()), inProgress, isPlayed, progress);
            drawComponent(x, barWidth, centre + centreGap, -rmsAmp, minScale, drawMin,
                          m_colours.colour(Colours::Type::RmsMinUnplayed, palette()),
                          m_colours.colour(Colours::Type::RmsMinPlayed, palette()),
                          m_colours.colour(Colours::Type::RmsMinBorder, palette()), inProgress, isPlayed, progress);
        }
    };

    if(colourMode != PlaybackColourMode::Position) {
        const bool isPlayed = colourMode == PlaybackColourMode::Played;
        for(int i{drawFirst}; i <= last && i < total; ++i) {
            drawBar(i, false, isPlayed, 0.0);
        }
    }
    else if(m_barGap == 0) {
        // Pass 1: all bars unplayed
        for(int i{drawFirst}; i <= last && i < total; ++i) {
            drawBar(i, false, false, 0.0);
        }

        // Pass 2: clip to played region and redraw with played colours
        const double clipWidth = currentPosition - firstX;
        if(clipWidth > 0.0) {
            painter.setClipRect(QRectF{firstX, 0.0, clipWidth, clipH}, Qt::IntersectClip);
            for(int i{drawFirst}; i <= last && i < total; ++i) {
                drawBar(i, false, true, 0.0);
            }
            painter.setClipping(false);
        }

        // Pass 3: pixel-aligned gradient zone centred at currentPosition.
        // Each bar's colour is derived from its pixel distance to the cursor.
        const double gradRadius = static_cast<double>(m_barWidth) * 2.0;
        const int gradFirst
            = std::max({0, first, static_cast<int>(std::floor((currentPosition - gradRadius) / m_sampleWidth))});
        const int gradLast
            = std::min(last, static_cast<int>(std::ceil((currentPosition + gradRadius) / m_sampleWidth)));

        for(int i{gradFirst}; i <= gradLast && i < total; ++i) {
            const double barCenter = static_cast<double>(i * m_sampleWidth) + (static_cast<double>(m_barWidth) * 0.5);
            // t=0 → played colour, t=1 → unplayed colour
            const double t = std::clamp((barCenter - (currentPosition - gradRadius)) / (2.0 * gradRadius), 0.0, 1.0);
            drawBar(i, true, false, 1.0 - t);
        }
    }
    else {
        for(int i{drawFirst}; i <= last && i < total; ++i) {
            const double sampleEnd   = (i + 1) * m_sampleWidth;
            const double sampleStart = sampleEnd - static_cast<double>(m_sampleWidth);

            double progress{0.0};
            if(sampleEnd <= currentPosition) {
                progress = 1.0;
            }
            else if(currentPosition > sampleStart) {
                const double sampleSpan = std::max(sampleEnd - sampleStart, 0.000001);
                progress                = (currentPosition - sampleStart) / sampleSpan;
                progress                = std::clamp(progress, 0.0, 1.0);
            }

            const bool isPlayed     = progress >= 1.0;
            const bool isInProgress = !isPlayed && progress > 0.0;
            drawBar(i, isInProgress, isPlayed, progress);
        }
    }
}

void WaveSeekBar::drawSilence(QPainter& painter, double first, double last, double y, double currentPosition,
                              PlaybackColourMode colourMode)
{
    const bool showRms        = m_data.complete && m_mode & Rms;
    const auto unplayedColour = showRms ? m_colours.colour(Colours::Type::RmsMaxUnplayed, palette())
                                        : m_colours.colour(Colours::Type::MaxUnplayed, palette());
    const auto playedColour   = showRms ? m_colours.colour(Colours::Type::RmsMaxPlayed, palette())
                                        : m_colours.colour(Colours::Type::MaxPlayed, palette());

    if(colourMode != PlaybackColourMode::Position) {
        painter.setPen(colourMode == PlaybackColourMode::Played ? playedColour : unplayedColour);
        painter.drawLine(QLineF{first, y, last, y});
    }
    else if(currentPosition <= first) {
        painter.setPen(unplayedColour);
        const QLineF unplayedLine{first, y, last, y};
        painter.drawLine(unplayedLine);
    }
    else if(currentPosition >= last) {
        painter.setPen(playedColour);
        const QLineF playedLine{first, y, last, y};
        painter.drawLine(playedLine);
    }
    else {
        painter.setPen(playedColour);
        const QLineF unplayedLine{first, y, currentPosition, y};
        painter.drawLine(unplayedLine);

        painter.setPen(unplayedColour);
        const QLineF playedLine{currentPosition, y, last, y};
        painter.drawLine(playedLine);
    }
}

void WaveSeekBar::drawSeekTip()
{
    if(!m_seekTip) {
        m_seekTip = new ToolTip();
        m_seekTip->setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint);
        m_seekTip->setAttribute(Qt::WA_ShowWithoutActivating);
        m_seekTip->setPalette(palette());
        m_seekTip->show();
    }

    const uint64_t seekPos   = valueFromPosition(m_seekPos.x());
    const uint64_t seekDelta = std::max(m_position, seekPos) - std::min(m_position, seekPos);
    const QString text       = Utils::msToString(seekPos);
    const QString subText    = (seekPos > m_position ? u"+"_s : u"-"_s) + Utils::msToString(seekDelta);

    m_seekTip->setContent(text, subText);

    QPoint seekTipPos{m_seekPos};

    static constexpr int offset = 8;
    if(seekTipPos.x() > (width() / 2)) {
        // Display to left of cursor to avoid clipping
        seekTipPos.rx() -= m_seekTip->width() + m_cursorWidth + offset;
    }
    else {
        seekTipPos.rx() += m_cursorWidth + offset;
    }

    seekTipPos.ry() += m_seekTip->height() / 2;
    seekTipPos.ry() = std::max(seekTipPos.y(), m_seekTip->height() / 2);
    seekTipPos.ry() = std::min(seekTipPos.y(), height());

    m_seekTip->setPosition(mapToGlobal(seekTipPos));
}

void WaveSeekBar::invalidate()
{
    invalidateWaveformCache();
    update();
}
} // namespace Fooyin::WaveBar

#include "moc_waveseekbar.cpp"
