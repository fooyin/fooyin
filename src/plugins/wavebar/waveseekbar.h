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

#include "waveformdata.h"

#include <QWidget>

namespace Fooyin::WaveBar {
class WaveSeekBar : public QWidget
{
    Q_OBJECT

public:
    explicit WaveSeekBar(QWidget* parent = nullptr);

    void processData(const WaveformData<float>& waveData);
    void setPosition(uint64_t pos);

signals:
    void sliderMoved(uint64_t pos);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    [[nodiscard]] int positionFromValue(uint64_t value) const;
    [[nodiscard]] uint64_t valueFromPosition(int pos) const;

    WaveformData<float> m_data;
    uint64_t m_position;
    bool m_isBeingMoved;

    QColor m_baseColour;
    QColor m_progressColour;
    QColor m_backgroundColour;
};
} // namespace Fooyin::WaveBar
