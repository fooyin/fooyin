/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <LukeT1@proton.me>
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
    , m_scale{1.0}
    , m_position{0}
    , m_showCursor{true}
    , m_cursorWidth{3}
    , m_channelScale{0.9}
    , m_barWidth{1}
    , m_barGap{0}
    , m_sampleWidth{m_barWidth + m_barGap}
    , m_maxScale{1.0}
    , m_centreGap{0}
    , m_mode{WaveMode::Default}
    , m_colours{}
{
    setFocusPolicy(Qt::FocusPolicy(style()->styleHint(QStyle::SH_Button_FocusPolicy)));
}

void WaveSeekBar::processData(const WaveformData<float>& waveData)
{
    m_data = waveData;

    if(m_data.complete) {
        const int waveformWidth = std::max(1, m_data.sampleCount() * m_sampleWidth);
        m_scale                 = static_cast<double>(width()) / static_cast<double>(waveformWidth);
    }

    update();
}

void WaveSeekBar::setPlayState(Player::PlayState state)
{
    m_playState = state;
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
        update();
    }
}

void WaveSeekBar::setBarWidth(int width)
{
    const int validatedWidth = std::max(1, width);
    if(std::exchange(m_barWidth, validatedWidth) != validatedWidth) {
        m_sampleWidth = m_barWidth + m_barGap;
        update();
    }
}

void WaveSeekBar::setBarGap(int gap)
{
    const int validatedGap = std::max(0, gap);
    if(std::exchange(m_barGap, validatedGap) != validatedGap) {
        m_sampleWidth = m_barWidth + m_barGap;
        update();
    }
}

void WaveSeekBar::setMaxScale(double scale)
{
    const double validatedScale = std::clamp(scale, 0.0, 2.0);
    if(std::exchange(m_maxScale, validatedScale) != validatedScale) {
        update();
    }
}

void WaveSeekBar::setCentreGap(int gap)
{
    const int validatedGap = std::max(0, gap);
    if(std::exchange(m_centreGap, validatedGap) != validatedGap) {
        update();
    }
}

void WaveSeekBar::setMode(WaveModes mode)
{
    if(std::exchange(m_mode, mode) != mode) {
        update();
    }
}

void WaveSeekBar::setColours(const Colours& colours)
{
    if(std::exchange(m_colours, colours) != colours) {
        update();
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
    painter.scale(m_scale, 1.0);

    if(m_data.empty()) {
        painter.setPen({m_colours.maxUnplayed, 1, Qt::SolidLine, Qt::FlatCap});
        const int centreY = height() / 2;
        painter.drawLine(0, centreY, rect().right(), centreY);
        return;
    }

    const int channels = m_data.channels;
    QRect rect         = event->rect();
    // Always repaint full height
    // Prevents clipping with seek tooltip and from other widgets
    rect.setHeight(contentsRect().height());

    const double invScale    = m_scale > 0.0 ? (1.0 / m_scale) : 1.0;
    const double firstSample = (static_cast<double>(rect.left()) * invScale) / static_cast<double>(m_sampleWidth);
    const double lastSample
        = ((static_cast<double>(rect.right()) + 1.0) * invScale) / static_cast<double>(m_sampleWidth);
    const int first     = std::max(0, static_cast<int>(std::floor(firstSample)));
    const int last      = std::max(first, static_cast<int>(std::ceil(lastSample)));
    const double firstX = first * m_sampleWidth;
    const double posX   = positionFromValue(static_cast<double>(m_position)) / m_scale;

    painter.fillRect(rect, m_colours.bgUnplayed);
    const double playedWidth = std::max(0.0, posX - firstX);
    painter.fillRect(QRectF{firstX, 0.0, playedWidth, static_cast<double>(height())}, m_colours.bgPlayed);

    const int channelHeight     = rect.height() / channels;
    const double waveformHeight = (channelHeight - m_centreGap) * m_channelScale;

    int y = static_cast<int>((channelHeight - waveformHeight) / 2);

    for(int ch{0}; ch < channels; ++ch) {
        drawChannel(painter, ch, waveformHeight, first, last, y);
        y += channelHeight;
    }

    if(m_showCursor && m_playState != Player::PlayState::Stopped) {
        painter.setPen({m_colours.cursor, static_cast<double>(m_cursorWidth), Qt::SolidLine, Qt::FlatCap});
        const QPointF pt1{posX, 0};
        const QPointF pt2{posX, static_cast<double>(height())};
        painter.drawLine(pt1, pt2);
    }

    if(isSeeking()) {
        painter.setPen({m_colours.seekingCursor, static_cast<double>(m_cursorWidth), Qt::SolidLine, Qt::FlatCap});
        const double seekX = static_cast<double>(m_seekPos.x()) / m_scale;
        painter.drawLine(QPointF{seekX, 0}, QPointF{seekX, static_cast<double>(height())});
    }
}

void WaveSeekBar::mouseMoveEvent(QMouseEvent* event)
{
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
    if(event->button() != Qt::LeftButton || !isSeeking()) {
        QWidget::mouseReleaseEvent(event);
        return;
    }

    stopSeeking();
    m_pressPos = {};

    m_position = valueFromPosition(static_cast<int>(event->position().x()));
    emit sliderMoved(m_position);
}

void WaveSeekBar::wheelEvent(QWheelEvent* event)
{
    if(event->angleDelta().y() < 0) {
        emit seekBackward();
    }
    else {
        emit seekForward();
    }

    event->accept();
}

void WaveSeekBar::keyPressEvent(QKeyEvent* event)
{
    const auto key = event->key();

    if(key == Qt::Key_Right || key == Qt::Key_Up) {
        emit seekForward();
        event->accept();
    }
    else if(key == Qt::Key_Left || key == Qt::Key_Down) {
        emit seekBackward();
        event->accept();
    }
    else {
        QWidget::keyPressEvent(event);
    }
}

double WaveSeekBar::positionFromValue(double value) const
{
    if(m_data.duration == 0) {
        return 0.0;
    }

    if(value >= m_data.duration) {
        return width();
    }

    const auto max   = static_cast<double>(m_data.duration);
    const auto ratio = value / max;

    return ratio * static_cast<double>(width());
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

void WaveSeekBar::drawChannel(QPainter& painter, int channel, double height, int first, int last, int y)
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
    const double currentPosition = positionFromValue(static_cast<double>(m_position)) / m_scale;

    if(!m_data.complete) {
        const auto finalX  = static_cast<double>(total * m_sampleWidth);
        const auto endX    = static_cast<double>(width()) / m_scale;
        const auto centreY = static_cast<double>(centre + (centreGap > 0 ? centreGap / 2 : 0));
        drawSilence(painter, finalX, endX, centreY);
    }
    else if(m_mode & Silence && drawMax && drawMin) {
        const auto firstX  = static_cast<double>(first * m_sampleWidth);
        const auto lastX   = static_cast<double>((last + 1) * m_sampleWidth);
        const auto centreY = static_cast<double>(centre + (centreGap > 0 ? centreGap / 2 : 0));
        drawSilence(painter, firstX, lastX, centreY);
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
            drawComponent(x, barWidth, centre, max[i], maxScale, drawMax, m_colours.maxUnplayed, m_colours.maxPlayed,
                          m_colours.maxBorder, inProgress, isPlayed, progress);
            drawComponent(x, barWidth, centre + centreGap, min[i], minScale, drawMin, m_colours.minUnplayed,
                          m_colours.minPlayed, m_colours.minBorder, inProgress, isPlayed, progress);
        }

        if(m_mode & Rms) {
            const double rmsAmp = rms[i] / rmsScale;
            drawComponent(x, barWidth, centre, rmsAmp, maxScale, drawMax, m_colours.rmsMaxUnplayed,
                          m_colours.rmsMaxPlayed, m_colours.rmsMaxBorder, inProgress, isPlayed, progress);
            drawComponent(x, barWidth, centre + centreGap, -rmsAmp, minScale, drawMin, m_colours.rmsMinUnplayed,
                          m_colours.rmsMinPlayed, m_colours.rmsMinBorder, inProgress, isPlayed, progress);
        }
    };

    if(m_barGap == 0) {
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

void WaveSeekBar::drawSilence(QPainter& painter, double first, double last, double y)
{
    const auto currentPosition = positionFromValue(static_cast<double>(m_position)) / m_scale;
    const bool showRms         = m_data.complete && m_mode & Rms;
    const auto unplayedColour  = showRms ? m_colours.rmsMaxUnplayed : m_colours.maxUnplayed;
    const auto playedColour    = showRms ? m_colours.rmsMaxPlayed : m_colours.maxPlayed;

    if(currentPosition <= first) {
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
        m_seekTip = new ToolTip(window());
        m_seekTip->show();
    }

    const uint64_t seekPos   = valueFromPosition(m_seekPos.x());
    const uint64_t seekDelta = std::max(m_position, seekPos) - std::min(m_position, seekPos);

    m_seekTip->setText(Utils::msToString(seekPos));
    m_seekTip->setSubtext((seekPos > m_position ? u"+"_s : u"-"_s) + Utils::msToString(seekDelta));

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

    m_seekTip->setPosition(mapTo(window(), seekTipPos));
}
} // namespace Fooyin::WaveBar

#include "moc_waveseekbar.cpp"
