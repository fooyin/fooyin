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

namespace {
QColor blendColors(const QColor& color1, const QColor& color2, double ratio)
{
    const int r = static_cast<int>(color1.red() * (1.0 - ratio) + color2.red() * ratio);
    const int g = static_cast<int>(color1.green() * (1.0 - ratio) + color2.green() * ratio);
    const int b = static_cast<int>(color1.blue() * (1.0 - ratio) + color2.blue() * ratio);
    const int a = static_cast<int>(color1.alpha() * (1.0 - ratio) + color2.alpha() * ratio);

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
WaveSeekBar::WaveSeekBar(SettingsManager* settings, QWidget* parent)
    : QWidget{parent}
    , m_settings{settings}
    , m_scale{1.0}
    , m_position{0}
    , m_showCursor{settings->value<Settings::WaveBar::ShowCursor>()}
    , m_cursorWidth{settings->value<Settings::WaveBar::CursorWidth>()}
    , m_channelScale{settings->value<Settings::WaveBar::ChannelScale>()}
    , m_barWidth{settings->value<Settings::WaveBar::BarWidth>()}
    , m_barGap{settings->value<Settings::WaveBar::BarGap>()}
    , m_maxScale{settings->value<Settings::WaveBar::MaxScale>()}
    , m_centreGap{settings->value<Settings::WaveBar::CentreGap>()}
    , m_mode{static_cast<WaveModes>(settings->value<Settings::WaveBar::Mode>())}
    , m_colours{settings->value<Settings::WaveBar::ColourOptions>().value<Colours>()}
{
    m_settings->subscribe<Settings::WaveBar::ShowCursor>(this, [this](const bool show) {
        m_showCursor = show;
        update();
    });
    m_settings->subscribe<Settings::WaveBar::CursorWidth>(this, [this](const double width) {
        m_cursorWidth = width;
        update();
    });
    m_settings->subscribe<Settings::WaveBar::ChannelScale>(this, [this](const double scale) {
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
    m_settings->subscribe<Settings::WaveBar::MaxScale>(this, [this](const double scale) {
        m_maxScale = scale;
        update();
    });
    m_settings->subscribe<Settings::WaveBar::CentreGap>(this, [this](const double gap) {
        m_centreGap = gap;
        update();
    });
    m_settings->subscribe<Settings::WaveBar::Mode>(this, [this](const int mode) {
        m_mode = static_cast<WaveModes>(mode);
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
        const double sampleWidth   = m_barWidth + m_barGap;
        const double waveformWidth = m_data.sampleCount() * sampleWidth;
        if(width() == waveformWidth) {
            m_scale = 1.0;
        }
        else {
            m_scale = std::round(static_cast<double>(width()) / sampleWidth) * sampleWidth / waveformWidth;
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

    updateRange(oldX, x);
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
        painter.setPen({m_colours.maxUnplayed, 1, Qt::SolidLine, Qt::FlatCap});
        const int centreY = height() / 2;
        painter.drawLine(0, centreY, rect().right(), centreY);
        return;
    }

    painter.save();

    painter.scale(m_scale, 1.0);

    const int channels       = m_data.channels;
    const double sampleWidth = m_barWidth + m_barGap;
    const QRect& rect        = event->rect();
    const int first          = static_cast<int>(static_cast<double>(rect.left()) / m_scale / sampleWidth);
    const int last           = static_cast<int>(static_cast<double>(rect.right() + 1) / m_scale);
    const double posX        = positionFromValue(m_position) / m_scale;

    painter.fillRect(rect, m_colours.bgUnplayed);
    painter.fillRect(QRect{first, 0, static_cast<int>(posX) - first, height()}, m_colours.bgPlayed);

    const int channelHeight     = rect.height() / channels;
    const double waveformHeight = (channelHeight - m_centreGap) * m_channelScale;

    double y = (channelHeight - waveformHeight) / 2;

    for(int ch{0}; ch < channels; ++ch) {
        drawChannel(painter, ch, waveformHeight, first, last, y);
        y += channelHeight;
    }

    if(m_showCursor) {
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

    const auto oldSeekPos = std::exchange(m_seekPos, widgetPos);
    updateRange(oldSeekPos.x(), m_seekPos.x());
}

void WaveSeekBar::updateRange(int first, int last)
{
    if(first == last) {
        return;
    }

    const int sampleWidth = m_barWidth + m_barGap;
    const auto left       = static_cast<int>((first < last ? first : last) - m_cursorWidth) - sampleWidth;
    const auto width      = static_cast<int>(std::abs(last - first) + (2 * m_cursorWidth)) + (2 * sampleWidth);

    const QRect updateRect(left, 0, width, height());
    update(updateRect);

    if(isSeeking()) {
        drawSeekTip();
    }
}

void WaveSeekBar::drawChannel(QPainter& painter, int channel, double height, int first, int last, double y)
{
    const auto& [max, min, rms] = m_data.channelData.at(channel);

    const double maxScale = (height / 2) * m_maxScale;
    const double minScale = height - maxScale;
    const double centre   = maxScale + y;

    double rmsScale{1.0};
    if(m_mode & WaveMode::Rms && !(m_mode & WaveMode::MinMax)) {
        rmsScale = *std::ranges::max_element(rms);
    }

    const auto total          = static_cast<int>(max.size());
    const auto sampleDuration = static_cast<uint64_t>(m_data.duration / total);
    const int sampleWidth     = m_barWidth + m_barGap;

    for(int i{first}; i < last && i < total; ++i) {
        const auto x        = static_cast<double>(i * sampleWidth);
        const auto barWidth = static_cast<double>(m_barWidth);

        const auto samplePosition
            = static_cast<uint64_t>((static_cast<double>(i + 1)) * static_cast<double>(sampleDuration));
        const auto timeLeft = std::max(samplePosition, m_position) - std::min(samplePosition, m_position);
        const auto progress = static_cast<double>(sampleDuration - timeLeft) / static_cast<double>(sampleDuration);
        const bool isPlayed = samplePosition < m_position;
        const bool isInProgress
            = !isPlayed && samplePosition > m_position && samplePosition <= m_position + sampleDuration;

        if(m_mode & WaveMode::MinMax) {
            double waveCentre{centre};

            const QPointF pt1{x, waveCentre - (max.at(i) * maxScale)};
            const QRectF rectMax{x, pt1.y(), barWidth, std::abs(pt1.y() - waveCentre)};

            waveCentre += m_centreGap;

            const QPointF pt2{x, waveCentre - (min.at(i) * minScale)};
            const QRectF rectMin{x, waveCentre, barWidth, std::abs(waveCentre - pt2.y())};

            setupPainter(painter, isInProgress, isPlayed, m_barWidth, progress, m_colours.maxUnplayed,
                         m_colours.maxPlayed, m_colours.maxBorder);

            painter.drawRect(rectMax);

            setupPainter(painter, isInProgress, isPlayed, m_barWidth, progress, m_colours.minUnplayed,
                         m_colours.minPlayed, m_colours.minBorder);

            painter.drawRect(rectMin);
        }

        if(m_mode & WaveMode::Rms) {
            double waveCentre{centre};

            const QPointF pt1{x, waveCentre - (rms.at(i) / rmsScale * maxScale)};
            const QRectF rectMax{x, pt1.y(), barWidth, std::abs(pt1.y() - waveCentre)};

            waveCentre += m_centreGap;

            const QPointF pt2{x, waveCentre - (-rms.at(i) / rmsScale * minScale)};
            const QRectF rectMin{x, waveCentre, barWidth, std::abs(waveCentre - pt2.y())};

            setupPainter(painter, isInProgress, isPlayed, m_barWidth, progress, m_colours.rmsMaxUnplayed,
                         m_colours.rmsMaxPlayed, m_colours.rmsMaxBorder);

            painter.drawRect(rectMax);

            setupPainter(painter, isInProgress, isPlayed, m_barWidth, progress, m_colours.rmsMinUnplayed,
                         m_colours.rmsMinPlayed, m_colours.rmsMinBorder);

            painter.drawRect(rectMin);
        }
    }

    if(!m_data.complete) {
        painter.setPen({m_colours.maxUnplayed, 1, Qt::SolidLine, Qt::FlatCap});
        const double finalX = (total)*sampleWidth;
        const QLineF centreLine{finalX, centre, static_cast<double>(rect().right()), centre};
        painter.drawLine(centreLine);
    }
}

void WaveSeekBar::drawSeekTip()
{
    if(!m_seekTip) {
        m_seekTip = new ToolTip(this);
        m_seekTip->show();
    }

    const uint64_t seekPos   = valueFromPosition(m_seekPos.x());
    const uint64_t seekDelta = std::max(m_position, seekPos) - std::min(m_position, seekPos);

    m_seekTip->setText(Utils::msToString(seekPos));
    m_seekTip->setSubtext((seekPos > m_position ? QStringLiteral("+") : QStringLiteral("-"))
                          + Utils::msToString(seekDelta));

    auto seekTipPos{m_seekPos};

    if(seekTipPos.x() > (width() / 2)) {
        // Display to left of cursor to avoid clipping
        seekTipPos.setX(seekTipPos.x() - (2 * m_seekTip->width()));
    }

    seekTipPos.setY(std::clamp(seekTipPos.y(), m_seekTip->height(), height()));
    m_seekTip->setPosition(seekTipPos);

    // Update to avoid artifacts
    const QRect updateRect(m_seekTip->x(), 0, (m_seekTip->width()), height());
    update(updateRect);
}
} // namespace Fooyin::WaveBar

#include "moc_waveseekbar.cpp"
