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

#include <core/track.h>

#include <QApplication>
#include <QMouseEvent>
#include <QPainter>
#include <QStyle>

// TODO: Make these user configurable
constexpr auto ChannelHeightScale = 0.9;
constexpr auto CursorWidth        = 3;

namespace Fooyin::WaveBar {
WaveSeekBar::WaveSeekBar(QWidget* parent)
    : QWidget{parent}
    , m_position{0}
    , m_isBeingMoved{false}
    , m_baseColour{50, 50, 200}
    , m_progressColour{palette().highlight().color()}
    , m_backgroundColour{Qt::transparent}
{ }

void WaveSeekBar::processData(const WaveformData<float>& waveData)
{
    m_data = waveData;
    update();
}

void WaveSeekBar::setPosition(uint64_t pos)
{
    const uint64_t oldPos = std::exchange(m_position, pos);

    if(oldPos == pos || m_isBeingMoved) {
        return;
    }

    const int oldX = positionFromValue(oldPos);
    const int x    = positionFromValue(pos);

    if(oldX == x) {
        return;
    }

    const int updateX = (oldX < x ? oldX : x) - CursorWidth;
    const int width   = std::abs(x - oldX) + (2 * CursorWidth);

    const QRect updateRect(updateX, 0, width, height());

    update(updateRect);
}

void WaveSeekBar::paintEvent(QPaintEvent* event)
{
    if(m_data.empty()) {
        return;
    }

    QPainter painter{this};

    painter.save();

    painter.setRenderHint(QPainter::Antialiasing);

    const int channels = m_data.format.channelCount();

    const QRect& rect = event->rect();
    const int first   = rect.left();
    const int last    = rect.right() + 1;

    const int channelHeight     = rect.height() / channels;
    const double waveformHeight = channelHeight * ChannelHeightScale;

    double y = (channelHeight - waveformHeight) / 2;

    for(int ch{0}; ch < channels; ++ch) {
        const auto& [max, min, rms] = m_data.channelData.at(ch);

        const double maxScale = waveformHeight / 2;
        const double minScale = waveformHeight - maxScale;
        const double centre   = maxScale + y;

        const int total = static_cast<int>(max.size());

        for(int i = first; i <= last && i < total; ++i) {
            const auto x = static_cast<double>(i);

            if(valueFromPosition(i) < m_position) {
                painter.setPen(QPen(m_progressColour, 1, Qt::SolidLine, Qt::FlatCap));
            }
            else {
                painter.setPen(QPen(m_baseColour, 1, Qt::SolidLine, Qt::FlatCap));
            }

            {
                const QPointF pt1{x, centre - (max.at(i) * maxScale)};
                const QPointF pt2{x, centre - (min.at(i) * minScale)};

                painter.drawLine(pt1, pt2);
            }

            if(valueFromPosition(i) < m_position) {
                painter.setPen(QPen(m_progressColour.lighter(), 1, Qt::SolidLine, Qt::FlatCap));
            }
            else {
                painter.setPen(QPen(m_baseColour.lighter(200), 1, Qt::SolidLine, Qt::FlatCap));
            }

            {
                const QPointF pt1{x, centre - (rms.at(i) * maxScale)};
                const QPointF pt2{x, centre - (-rms.at(i) * minScale)};

                painter.drawLine(pt1, pt2);
            }
        }
        y += channelHeight;
    }

    if(m_position > 0) {
        painter.setPen(QPen(m_progressColour, CursorWidth, Qt::SolidLine, Qt::FlatCap));
        const int posX = positionFromValue(m_position);
        painter.drawLine(posX, 0, posX, height());
    }

    painter.restore();
}

void WaveSeekBar::mouseMoveEvent(QMouseEvent* event)
{
    QWidget::mouseMoveEvent(event);

    m_isBeingMoved = true;

    const uint64_t pos = event->pos().x() > 10 ? valueFromPosition(event->pos().x()) : 0;
    emit sliderMoved(pos);
    update();

    m_isBeingMoved = false;
}

void WaveSeekBar::mousePressEvent(QMouseEvent* event)
{
    QWidget::mouseMoveEvent(event);

    if(event->button() == Qt::LeftButton) {
        m_isBeingMoved = true;

        const uint64_t pos = event->pos().x() > 10 ? valueFromPosition(event->pos().x()) : 0;
        emit sliderMoved(pos);
        update();

        m_isBeingMoved = false;
    }
}

int WaveSeekBar::positionFromValue(uint64_t value) const
{
    if(m_data.duration <= 0) {
        return 0;
    }

    if(value > m_data.duration) {
        return width();
    }

    const auto w         = static_cast<uint64_t>(width());
    const uint64_t range = m_data.duration;

    if(range > (std::numeric_limits<int>::max() / 4096)) {
        const double pos = static_cast<double>(value) / static_cast<double>(range) / static_cast<double>(w);
        return static_cast<int>(pos);
    }

    if(range > w) {
        return static_cast<int>((2 * value * w + range) / (2 * range));
    }

    const uint64_t div = w / range;
    const uint64_t mod = w % range;

    return static_cast<int>(value * div + (2 * value * mod + range) / (2 * range));
}

uint64_t WaveSeekBar::valueFromPosition(int pos) const
{
    if(pos <= 0) {
        return 0;
    }

    if(pos >= width()) {
        return m_data.duration;
    }

    const auto w         = static_cast<uint64_t>(width());
    const uint64_t range = m_data.duration;

    if(w > range) {
        return (2 * range * pos + w) / (2 * w);
    }

    const uint64_t div = range / w;
    const uint64_t mod = range % w;

    return pos * div + (2 * mod * pos + w) / (2 * w);
}
} // namespace Fooyin::WaveBar

#include "moc_waveseekbar.cpp"
