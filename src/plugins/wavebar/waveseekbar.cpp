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

#include "settings/wavebarsettings.h"

#include <core/track.h>
#include <utils/settings/settingsmanager.h>
#include <utils/utils.h>

#include <QApplication>
#include <QMouseEvent>
#include <QPainter>
#include <QStyle>

namespace Fooyin::WaveBar {
WaveSeekBar::WaveSeekBar(SettingsManager* settings, QWidget* parent)
    : QWidget{parent}
    , m_settings{settings}
    , m_scale{1.0}
    , m_position{0}
    , m_cursorWidth{settings->value<Settings::WaveBar::CursorWidth>()}
    , m_channelScale{settings->value<Settings::WaveBar::ChannelHeightScale>()}
    , m_barWidth{settings->value<Settings::WaveBar::BarWidth>()}
    , m_barGap{settings->value<Settings::WaveBar::BarGap>()}
    , m_drawValues{settings->value<Settings::WaveBar::DrawValues>()}
    , m_colours{settings->value<Settings::WaveBar::ColourOptions>().value<Colours>()}
{
    m_settings->subscribe<Settings::WaveBar::CursorWidth>(this, [this](const double width) {
        m_cursorWidth = width;
        update();
    });
    m_settings->subscribe<Settings::WaveBar::ChannelHeightScale>(this, [this](const double scale) {
        m_channelScale = scale;
        update();
    });
    m_settings->subscribe<Settings::WaveBar::BarWidth>(this, [this](const int width) {
        m_barWidth = width;
        update();
    });
    m_settings->subscribe<Settings::WaveBar::BarGap>(this, [this](const int gap) {
        m_barGap = gap;
        update();
    });
    m_settings->subscribe<Settings::WaveBar::DrawValues>(this, [this](const int values) {
        m_drawValues = static_cast<ValueOptions>(values);
        update();
    });
    m_settings->subscribe<Settings::WaveBar::ColourOptions>(this, [this](const QVariant& var) {
        m_colours = var.value<Colours>();
        update();
    });
}

void WaveSeekBar::processData(const WaveformData<float>& waveData)
{
    m_data = waveData;

    if(m_data.complete) {
        const double waveformWidth = m_data.sampleCount() * (m_barWidth + m_barGap);
        if(width() == waveformWidth) {
            m_scale = 1.0;
        }
        else {
            m_scale = static_cast<double>(width()) / waveformWidth;
        }
    }

    update();
}

void WaveSeekBar::setPosition(uint64_t pos)
{
    const uint64_t oldPos = std::exchange(m_position, pos);

    if(oldPos == pos) {
        return;
    }

    const int oldX = positionFromValue(oldPos);
    const int x    = positionFromValue(pos);

    if(oldX == x) {
        return;
    }

    const auto updateX = static_cast<int>((oldX < x ? oldX : x) - m_cursorWidth);
    const auto width   = static_cast<int>(std::abs(x - oldX) + (2 * m_cursorWidth));

    const QRect updateRect(updateX, 0, width, height());
    update(updateRect);

    if(isSeeking()) {
        drawSeekTip();
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

    if(m_data.empty()) {
        painter.setPen({m_colours.fgUnplayed, 1, Qt::SolidLine, Qt::FlatCap});
        const int centreY = height() / 2;
        painter.drawLine(0, centreY, rect().right(), centreY);
        return;
    }

    painter.save();

    painter.scale(m_scale, 1.0);

    const int channels       = m_data.channels;
    const double sampleTotal = m_barWidth + m_barGap;
    const QRect& rect        = event->rect();
    const int first          = static_cast<int>(static_cast<double>(rect.left()) / m_scale / sampleTotal);
    const int last           = static_cast<int>(static_cast<double>(rect.right() + 1) / m_scale);
    const double posX        = positionFromValue(m_position) / m_scale;

    painter.fillRect(rect, m_colours.bgUnplayed);
    painter.fillRect(QRect{first, 0, static_cast<int>(posX) - first, height()}, m_colours.bgPlayed);

    const int channelHeight     = rect.height() / channels;
    const double waveformHeight = channelHeight * m_channelScale;

    double y = (channelHeight - waveformHeight) / 2;

    for(int ch{0}; ch < channels; ++ch) {
        drawChannel(painter, ch, waveformHeight, first, last, y);
        y += channelHeight;
    }

    if(m_cursorWidth > 0) {
        painter.setPen({m_colours.cursor, m_cursorWidth / m_scale, Qt::SolidLine, Qt::FlatCap});
        const QPointF pt1{posX, 0};
        const QPointF pt2{posX, static_cast<double>(height())};
        painter.drawLine(pt1, pt2);
    }

    if(isSeeking()) {
        painter.setPen({m_colours.seekingCursor, m_cursorWidth / m_scale, Qt::SolidLine, Qt::FlatCap});
        const int seekX = static_cast<int>(m_seekPos.x() / m_scale);
        painter.drawLine(seekX, 0, seekX, height());
    }

    painter.restore();
}

void WaveSeekBar::mouseMoveEvent(QMouseEvent* event)
{
    QWidget::mouseMoveEvent(event);

    if(isSeeking() && event->buttons() & Qt::LeftButton) {
        updateMousePosition(event->pos());
        drawSeekTip();
    }
}

void WaveSeekBar::mousePressEvent(QMouseEvent* event)
{
    QWidget::mousePressEvent(event);

    if(event->button() == Qt::LeftButton) {
        updateMousePosition(event->pos());
        drawSeekTip();
    }
}

void WaveSeekBar::mouseReleaseEvent(QMouseEvent* event)
{
    QWidget::mouseMoveEvent(event);

    if(event->button() != Qt::LeftButton || !isSeeking()) {
        return;
    }

    stopSeeking();

    m_position = valueFromPosition(event->pos().x());
    emit sliderMoved(m_position);
}

int WaveSeekBar::positionFromValue(uint64_t value) const
{
    if(m_data.duration == 0) {
        return 0;
    }

    if(value >= m_data.duration) {
        return width();
    }

    const auto w     = static_cast<double>(width());
    const auto max   = static_cast<double>(m_data.duration);
    const auto ratio = static_cast<double>(value) / max;

    return static_cast<int>(ratio * w);
}

uint64_t WaveSeekBar::valueFromPosition(int pos) const
{
    if(pos <= 0) {
        return 0;
    }

    if(pos >= width()) {
        return m_data.duration;
    }

    const auto w     = static_cast<double>(width());
    const auto max   = static_cast<double>(m_data.duration);
    const auto ratio = static_cast<double>(pos) / w;

    return static_cast<uint64_t>(ratio * max);
}

bool WaveSeekBar::positionHasPlayed(int pos) const
{
    return static_cast<uint64_t>(static_cast<double>(valueFromPosition(pos)) * m_scale) < m_position;
}

void WaveSeekBar::updateMousePosition(const QPoint& pos)
{
    QPoint widgetPos{pos};
    widgetPos.setX(std::clamp(pos.x(), 1, width()));
    m_seekPos = widgetPos;
    update();
}

void WaveSeekBar::drawChannel(QPainter& painter, int channel, double height, int first, int last, double y)
{
    const auto& [max, min, rms] = m_data.channelData.at(channel);

    const double maxScale = height / 2;
    const double minScale = height - maxScale;
    const double centre   = maxScale + y;

    double rmsScale{1.0};
    if(m_drawValues == ValueOptions::RMS) {
        rmsScale = *std::ranges::max_element(rms);
    }

    const int total       = static_cast<int>(max.size());
    const int sampleTotal = m_barWidth + m_barGap;

    auto x = static_cast<double>(first * sampleTotal);
    for(int i{first}; i < total; ++i, x += sampleTotal) {
        const bool hasPlayed = positionHasPlayed(static_cast<int>(x));
        const auto barWidth  = static_cast<double>(m_barWidth);

        if(m_drawValues == ValueOptions::All || m_drawValues == ValueOptions::MinMax) {
            const QPointF pt1{x, centre - (max.at(i) * maxScale)};
            const QPointF pt2{x, centre - (min.at(i) * minScale)};

            const QRectF rect{x, pt1.y(), barWidth, std::abs(pt1.y() - pt2.y())};
            painter.setBrush(hasPlayed ? m_colours.fgPlayed : m_colours.fgUnplayed);
            if(barWidth > 1) {
                painter.setPen(m_colours.fgBorder);
            }
            else {
                painter.setPen(Qt::NoPen);
            }
            painter.drawRect(rect);
        }

        if(m_drawValues == ValueOptions::All || m_drawValues == ValueOptions::RMS) {
            const QPointF pt1{x, centre - (rms.at(i) / rmsScale * maxScale)};
            const QPointF pt2{x, centre - (-rms.at(i) / rmsScale * minScale)};

            const QRectF rect{x, pt1.y(), barWidth, std::abs(pt1.y() - pt2.y())};
            painter.setBrush(hasPlayed ? m_colours.rmsPlayed : m_colours.rmsUnplayed);
            if(barWidth > 1) {
                painter.setPen(m_colours.rmsBorder);
            }
            else {
                painter.setPen(Qt::NoPen);
            }
            painter.drawRect(rect);
        }
    }

    const int finalX = total * sampleTotal;
    if(finalX < last) {
        painter.setPen({m_colours.fgUnplayed, 1, Qt::SolidLine, Qt::FlatCap});
        const int centreY = this->height() / 2;
        painter.drawLine(finalX, centreY, rect().right(), centreY);
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
    m_seekTip->setSubtext((seekPos > m_position ? QStringLiteral("+") : QStringLiteral("-"))
                          + Utils::msToString(seekDelta));

    auto seekTipPos{m_seekPos};
    seekTipPos.setY(std::max(seekTipPos.y(), 0));
    m_seekTip->setPosition(mapTo(window(), seekTipPos));
}
} // namespace Fooyin::WaveBar

#include "moc_waveseekbar.cpp"
