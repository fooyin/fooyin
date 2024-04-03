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

#pragma once

#include "settings/wavebarsettings.h"
#include "wavebarcolours.h"
#include "waveformdata.h"

#include <utils/widgets/tooltip.h>

#include <QPointer>
#include <QWidget>

namespace Fooyin {
class SettingsManager;

namespace WaveBar {
class WaveSeekBar : public QWidget
{
    Q_OBJECT

public:
    explicit WaveSeekBar(SettingsManager* settings, QWidget* parent = nullptr);

    void processData(const WaveformData<float>& waveData);

    void setPosition(uint64_t pos);
    [[nodiscard]] bool isSeeking() const;
    void stopSeeking();

signals:
    void sliderMoved(uint64_t pos);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    [[nodiscard]] int positionFromValue(uint64_t value) const;
    [[nodiscard]] uint64_t valueFromPosition(int pos) const;
    void updateMousePosition(const QPoint& pos);
    void updateRange(int first, int last);

    void drawChannel(QPainter& painter, int channel, double height, int first, int last, int y);
    void drawSeekTip();

    SettingsManager* m_settings;

    WaveformData<float> m_data;
    double m_scale;
    uint64_t m_position;
    QPoint m_seekPos;
    QPointer<ToolTip> m_seekTip;

    bool m_showCursor;
    int m_cursorWidth;
    double m_channelScale;
    int m_barWidth;
    int m_barGap;
    int m_sampleWidth;
    double m_maxScale;
    int m_centreGap;

    WaveModes m_mode;
    Colours m_colours;
};
} // namespace WaveBar
} // namespace Fooyin
