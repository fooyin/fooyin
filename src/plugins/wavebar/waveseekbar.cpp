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
    , m_position{0}
    , m_showCursor{settings->value<Settings::WaveBar::ShowCursor>()}
    , m_cursorWidth{settings->value<Settings::WaveBar::CursorWidth>()}
    , m_channelScale{settings->value<Settings::WaveBar::ChannelHeightScale>()}
    , m_antialiasing{settings->value<Settings::WaveBar::Antialiasing>()}
    , m_drawValues{settings->value<Settings::WaveBar::DrawValues>()}
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
    m_settings->subscribe<Settings::WaveBar::ChannelHeightScale>(this, [this](const double scale) {
        m_channelScale = scale;
        update();
    });
    m_settings->subscribe<Settings::WaveBar::DrawValues>(this, [this](const int values) {
        m_drawValues = static_cast<ValueOptions>(values);
        update();
    });
    m_settings->subscribe<Settings::WaveBar::Antialiasing>(this, [this](const bool enable) {
        m_antialiasing = enable;
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

    if(m_antialiasing) {
        painter.setRenderHint(QPainter::Antialiasing);
    }

    if(m_data.empty()) {
        painter.setPen({m_colours.fgUnplayed, 1, Qt::SolidLine, Qt::FlatCap});
        const int centreY = height() / 2;
        painter.drawLine(0, centreY, rect().right(), centreY);
        return;
    }

    painter.save();

    const int channels = m_data.channels;

    const QRect& rect = event->rect();
    const int first   = rect.left();
    const int last    = rect.right() + 1;
    const int posX    = positionFromValue(m_position);

    painter.fillRect(rect, m_colours.bgUnplayed);
    painter.fillRect(QRect{first, 0, posX - first, height()}, m_colours.bgPlayed);

    const int channelHeight     = rect.height() / channels;
    const double waveformHeight = channelHeight * m_channelScale;

    double y = (channelHeight - waveformHeight) / 2;

    for(int ch{0}; ch < channels; ++ch) {
        drawChannel(painter, ch, waveformHeight, first, last, y);
        y += channelHeight;
    }

    if(m_showCursor) {
        painter.setPen({m_colours.cursor, m_cursorWidth, Qt::SolidLine, Qt::FlatCap});
        painter.drawLine(posX, 0, posX, height());
    }

    if(!m_seekPos.isNull()) {
        painter.setPen({m_colours.seekingCursor, m_cursorWidth, Qt::SolidLine, Qt::FlatCap});
        painter.drawLine(m_seekPos.x(), 0, m_seekPos.x(), height());
    }

    painter.restore();
}

void WaveSeekBar::mouseMoveEvent(QMouseEvent* event)
{
    QWidget::mouseMoveEvent(event);

    if(event->buttons() & Qt::LeftButton) {
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

    if(event->button() != Qt::LeftButton) {
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

    const int total = static_cast<int>(max.size());

    for(int i = first; i <= last && i < total; ++i) {
        const auto x = static_cast<double>(i);

        const bool hasPlayed = valueFromPosition(i) < m_position;

        if(m_drawValues == ValueOptions::All || m_drawValues == ValueOptions::MinMax) {
            if(hasPlayed) {
                painter.setPen({m_colours.fgPlayed, 1, Qt::SolidLine, Qt::FlatCap});
            }
            else {
                painter.setPen({m_colours.fgUnplayed, 1, Qt::SolidLine, Qt::FlatCap});
            }

            const QPointF pt1{x, centre - (max.at(i) * maxScale)};
            const QPointF pt2{x, centre - (min.at(i) * minScale)};

            painter.drawLine(pt1, pt2);
        }

        if(m_drawValues == ValueOptions::All || m_drawValues == ValueOptions::RMS) {
            if(hasPlayed) {
                painter.setPen({m_colours.rmsPlayed, 1, Qt::SolidLine, Qt::FlatCap});
            }
            else {
                painter.setPen({m_colours.rmsUnplayed, 1, Qt::SolidLine, Qt::FlatCap});
            }

            const QPointF pt1{x, centre - (rms.at(i) / rmsScale * maxScale)};
            const QPointF pt2{x, centre - (-rms.at(i) / rmsScale * minScale)};

            painter.drawLine(pt1, pt2);
        }
    }

    if(total < last) {
        painter.setPen({m_colours.fgUnplayed, 1, Qt::SolidLine, Qt::FlatCap});
        const int centreY = this->height() / 2;
        painter.drawLine(total, centreY, rect().right(), centreY);
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
