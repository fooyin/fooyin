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
#include "waveformdata.h"

#include <utils/worker.h>

namespace Fooyin::WaveBar {
class WaveformRescaler : public Worker
{
    Q_OBJECT

public:
    explicit WaveformRescaler(QObject* parent = nullptr);

signals:
    void waveformRescaled(const WaveformData<float>& data);

public slots:
    void rescale();
    void rescale(int width);
    void rescale(const WaveformData<float>& data, int width);

    void changeSamplePixelRatio(int ratio);
    void changeDownmix(DownmixOption option);

private:
    WaveformData<float> m_data;
    int m_width;
    int m_samplePixelRatio;
    DownmixOption m_downMix;
};
} // namespace Fooyin::WaveBar
